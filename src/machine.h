// (c) Michal Maruska 2003-2024
#pragma once

#include <assert.h>
#include <cstdarg>
#include "config.h"
#include <algorithm>
#include <functional>

#include "queue.h"
#include "platform.h"
#include "colors.h"
#include <boost/circular_buffer.hpp>
#include <memory>
#include <mutex>

#include "fork_enums.h"
#include "fork_configuration.h"

namespace forkNS {

template <typename Time>
bool time_difference_more(Time now, Time past, Time limit_difference) {
    return (now > past + limit_difference);
    // return ( (now - past) > limit_difference);
}


/* fixme: inherit from xorg! */
constexpr int MAX_KEYCODE = 256;

/**
 * machine keeps log of `archived_event_t'
 *
 * It is invoked and itself invokes `platformEnvironment'
 * it receives `PlatformEvent' abstract class
 * platformEnvironment extracts ... Keycode and Time
 *
 * and we update some state and sometimes rewrite the event
 * and output.
 * we also keep our own `key_event'.
 *
 *
 * Concepts:
 * archived_event_t ... contains platformEvent and "forked"
 *
 */
template <typename Keycode, typename Time, typename archived_event_t,
          typename last_events_t = boost::circular_buffer<archived_event_t>>
class forkingMachine {

    constexpr static const Keycode no_key = 0;

public:
        // typedef boost::circular_buffer<archived_event_t> last_events_t;


    /* Environment_t must be able to convert from
     * platformEvent to archived_event_t
     */
    typedef platformEnvironment<Keycode, Time, archived_event_t> Environment_t;

    /* `machine': the dynamic `state' */
    struct key_event {
        // I want a static variable pointing to the Class's Environment.
        inline static const Environment_t *env;

        PlatformEvent* p_event;
        /* if forked to (another keycode), this is the original key */
        // makes sense?. could be environment->detail_of(pevent);
        Keycode original_keycode = no_key;

        key_event(std::unique_ptr<PlatformEvent> p) : p_event(p.release()){};
        ~key_event() {
            // should be nullptr
            // bug: must call environment -> free_event()
            // notice that currently we nullify this pointer.
            // so virtual d-tor?
#if 1
            env->log("%s: %p %p\n", __func__, this, p_event);
#endif
            if (p_event != nullptr)
                env->free_event(p_event);
        }
    };

    typedef forkNS::my_queue<key_event*> list_with_tail;
private:

    /* states of the automaton: */
    enum fork_state_t {  // states of the automaton
        st_normal,
        st_suspect,
        st_verify,
        st_deactivated,
        st_activated
    };

    /* How we decided for the fork */
    enum class fork_reason_t {
        reason_long,               // key pressed too long
        reason_overlap,             // key press overlaps with another key
        reason_force                // mouse-button was pressed & triggered fork.
    };

    /* used only for debugging */
    static constexpr
    char const * const state_description[5] = {
        // map<fork_state_t, string>
        "normal",
        "suspect",
        "verify",
        "deactivated",
        "activated"
    };

    // atomic?
    // volatile mutable int
    mutable std::mutex mLock;
    /* the mouse interrupt handler should ..... err!  `volatile'
     *
     * useless mmc!  But i want to avoid any caching it.... SMP ??*/

public:
    std::unique_ptr<Environment_t> environment;
#if USE_LOCKING
    void stop() {
        // wait & stop
        std::scoped_lock wait_lock(mLock);
    }

    void set_debug(int level) {
        std::scoped_lock lock(mLock);
        config->debug = level;
        // (machine->config->debug? 0: 1);
    }

private:

    void lock() const
    {
        mLock.lock();
        // mdb_raw("/--\n");
    }
    void unlock() const
    {
        mLock.unlock();
        // mdb_raw("\\__ (unlock)\n");
    }
    void check_locked() const {
        // assert(mLock);
    }
    void check_unlocked() const {
        // assert(mLock == 0);
    }

#endif


private:
    /* This is how it works:
     * We have a `state' and 3 queues:
     *
     *  output Q  |   internal Q    | input Q
     *  waits for |
     *  thaw         Oxxxxxx        |  yyyyy
     *               ^ forked?
     *
     * We push at the end of input Q.  Then we pop from that Q and push on
     * Internal where we determine for 1 event, if forked/non-forked.
     *
     * Then we push on the output Q. At that moment, we also restart: all
     * from internal Q is returned/prepended to the input Q.
     */
    fork_state_t state;
    // only for certain states we keep (updated):

    Keycode suspect;
    Keycode verificator_keycode;

    // these are "registers"
    Time suspect_time;           /* time of the 1st event in the queue. */
    Time verificator_time = 0;       /* press of the `verificator' */

    /* To allow AR for forkable keys:
     * When we press a key the second time in a row, we might avoid forking:
     * So, this is for the detector:
     *
     * This means I cannot do this trick w/ 2 keys, only 1 is the last/considered! */
    Keycode last_released; // .- trick
    Time last_released_time;

    // calculated:
    Time mDecision_time;         /* Time to wait... so that the HEAD event in queue could decide more*/
    Time mCurrent_time;          // the last time we received from previous plugin/device

    list_with_tail internal_queue;
    /* Still undecided events: these events alone don't decide
       how to interpret the first event on the queue.*/
    list_with_tail input_queue;  /* Not yet processed at all. Since we wait for external
                                  * events to resume processing (Grab is active-frozen) */
    list_with_tail output_queue; /* We have decided, so possibly modified events (forked),
                                  * Externals don't accept, so we keep them. */

public:
    /* forkActive(x) == y  means we sent downstream Keycode Y instead of X.
     * what is the meaning of:  0 and X ? */
    Keycode          forkActive[MAX_KEYCODE];

private:
    last_events_t last_events_log;
    int max_last = 10; // can be updated!

    // this must be public:
    using fork_configuration = ForkConfiguration<Keycode, Time, MAX_KEYCODE>;
public:
    std::unique_ptr<fork_configuration> config; // list<fork_configuration>

/* The Static state = configuration.
 * This is the matrix with some Time values:
 * using the fact, that valid Keycodes are non zero, we use
 * the 0 column for `code's global values

 * Global      xxxxxxxx unused xxxxxx
 * key-wise   per-pair per-pair ....
 * key-wise   per-pair per-pair ....
 * ....
 */

private:
    void set_last_events_count(const int new_max) {
        check_locked();
        mdb("%s: allocating %d events\n", __func__, new_max);

        if (max_last > new_max) {
            // shrink. todo! in the circular.h!
            mdb("%s: unimplemented\n", __func__);
        } else {
            last_events_log.set_capacity(new_max);
        }

        max_last = new_max;
    }

    void save_event_log(const key_event *event) {
        // could I emplace it?
        // reference = last_events_log.emplace_back()
        // reference.forked = ev->forked;
        archived_event_t archived_event;
        environment->archive_event(archived_event, event->p_event);
        archived_event.forked = event->original_keycode; // todo: rename original_keycode

        last_events_log.push_back(archived_event);
    }


    static bool
    forkable_p(const fork_configuration* config, Keycode code)
    {
        return (config->fork_keycode[code] != no_key);
    }

    static constexpr int BufferLength = 200;

    [[nodiscard]]
    static const char*
    describe_machine_state(fork_state_t state) {
        static char buffer[BufferLength];

        snprintf(buffer, BufferLength, "%s[%dm%s%s",
                 escape_sequence, 32 + state,
                 state_description[state], color_reset);
        return buffer;
    }

    // PluginInstance* mPlugin;

    fork_configuration** find_configuration_n(int n);

    bool queues_non_empty() const {
        return (!output_queue.empty() || !input_queue.empty() || !internal_queue.empty());
    }
    Time queue_front_time(list_with_tail &queue) const {
        return environment->time_of(queue.front()->p_event);
    }

    /**
     * Resets the machine, so as to reconsider the events on the
     * `internal' queue from scratch.
     * Apparently the criteria/configuration has changed!
     * Reasonably this is in response to a key event. So we are in Final state.
     */
    void replay_events(bool force_also) {
        mdb("%s\n", __func__);

        rewind_machine(st_normal);

        // todo: what else?
        // last_released & last_released_time no more available.
        last_released = no_key; // bug!

        run_automaton(force_also);
    };

    /**
     * Apply event EV to (state, internal-queue, time).
     * This can append to the output-queue
     *      sets: `mDecision_time'
     *
     * input:
     *   internal-queue  + ev + input-queue
     * output:
     *   either the ev  is pushed on internal_queue, or to the output-queue
     *   the head of internal_queue may be pushed to the output-queue as well.
     */
   void transition_by_key(std::unique_ptr<key_event> ev) {
        check_locked();
        assert(ev);
        const PlatformEvent* pevent = ev->p_event;
        const Keycode key = environment->detail_of(pevent);

        mdb("%s: %lu\n", __func__, key);

        /* Please, first change the state, then enqueue, and then EMIT_EVENT.
         * fixme: should be a function then  !!!*/

#if DDX_REPEATS_KEYS || 1
        /* `quick_ignore': I want to ignore _quickly_ the hw-repeated unfiltered (forked) modifiers.
           Normal modifier are ignored before put in the X input pipe/queue.
           This is only necessary if the lower level (keyboard driver) passes through the
           HW auto-repeat events. */

        if ((key_forked(key)) && environment->press_p(pevent)
            && (key != forkActive[key])) // not `self_forked'
            {
                mdb("%s: the key is forked, ignoring\n", __func__);

                environment->free_event(ev->p_event);
                return;
            }
#endif
        // `limitation':
        // A currently forked keycode cannot be (suddenly) pressed 2nd time.
        // assert(release_p(event) || (key < MAX_KEYCODE && forkActive[key] == 0));
        switch (state) {
        case st_normal:
            apply_event_to_normal(std::move(ev));
            return;
        case st_suspect:
        {
            apply_event_to_suspect(std::move(ev));
            return;
        }
        case st_verify:
        {
            apply_event_to_verify_state(std::move(ev));
            return;
        }
        default:
            mdb("----------unexpected state---------\n");
        }
    };

    /**
       returns:
       state  (in the `machine')
       relay_event   possibly, othewise 0
       machine->mDecision_time   ... for another timer.
    */
    bool transition_by_time(Time current_time) {
      check_locked();
      // confirm fork:
      mdb("%s%s%s state: %s, queue: %d, time: %u key: %d\n", fork_color,
          __func__, color_reset, describe_machine_state(this->state),
          internal_queue.length(), (int)current_time, suspect);

      /* First, I try the simple (fork-by-one-keys).
       * If that works, -> fork! Otherwise, I try w/ 2-key forking, overlapping.
       */

      // notice, how mDecision_time is rewritten here:
      if (0 == (mDecision_time = key_pressed_too_long(current_time))) {

        activate_fork(fork_reason_t::reason_long);
        return true;
      };

      /* To test 2 keys overlap, we need the 2nd key: a verificator! */
      if (state == st_verify) {
        // verify overlap
        Time decision_time = config->verifier_decision_time(
            current_time, suspect, suspect_time, verificator_keycode,
            verificator_time);

        if (decision_time == 0) {
          activate_fork(fork_reason_t::reason_overlap);
          return true;
        }

        if (decision_time < mDecision_time)
          mDecision_time = decision_time;
      }
      // so, now we are surely in the replay_mode. All we need is to
      // get an estimate on time still needed:

      /* So, we were woken too early. */
      mdb("*** %s: returning with some more time-to-wait: %lu"
          "(prematurely woken)\n",
          __func__, mDecision_time - current_time);
      return false;
    };

    /**  First (press)
     *    v   ^     0   <-- we are here.
     *        Second
     */
    void apply_event_to_suspect(std::unique_ptr<key_event> ev) {
        const PlatformEvent *pevent = ev->p_event;
        Time simulated_time = environment->time_of(pevent);
        Keycode key = environment->detail_of(pevent);

        auto &queue = internal_queue;
        assert(!queue.empty() && state == st_suspect);

        /* Here, we can
         * o refuse .... if suspected/forkable is released quickly,
         * o fork (definitively),  ... for _time_
         * o start verifying, or wait, or confirm (timeout)
         * todo: I should repeat a bi-depressed forkable.
         */

        // first we look at the time:
        if (0 == (mDecision_time = key_pressed_too_long(simulated_time))) {
            confirm_fork_and_enqueue(std::move(ev), fork_reason_t::reason_long);
            return;
        };

        /* So, we now have a second key, since the duration of 1 key
         * was not enough. */
        if (environment->release_p(pevent)) {
            mdb("suspect/release: suspected = %d, time diff: %d\n", suspect,
                (int)(simulated_time - suspect_time));
            if (key == suspect) {
                do_confirm_non_fork_by(std::move(ev));
                return;
                /* fixme:  here we confirm, that it was not a user error.....
                   bad synchro. i.e. the suspected key was just released  */
            } else {
                /* something released, but not verificating, b/c we are in `suspect',
                 * not `confirm'  */
                internal_queue.push(ev.release());
                return;
            };
        } else {
            if (!environment->press_p(pevent)) { // why not release_p() ?
                // RawPress & Device events.
                if (!environment->release_p(pevent)) {
                    mdb("a bizzare event scanned\n");
                }
                internal_queue.push(ev.release());
                return;
            }
            // press-event here:
            if (key == suspect) {
                /* How could this happen? Auto-repeat on the lower/hw level?
                 * And that AR interval is shorter than the fork-verification */
                if (config->fork_repeatable[key]) {
                    mdb("The suspected key is configured to repeat, so ...\n");
                    forkActive[suspect] = suspect;
                    do_confirm_non_fork_by(std::move(ev));
                    return;
                } else {
                    // fixme: this keycode is repeating, but we still don't know what to
                    // do.
                    // ..... `discard' the event???
                    // fixme: but we should recalc the mDecision_time !!
                    return;
                }
            } else {
                // another key pressed
                change_state(st_verify);
                verificator_time = simulated_time;
                verificator_keycode =
                    key; /* if already we had one -> we are not in this state!
                            if the verificator becomes a modifier ?? fixme:*/
                // verify overlap
                Time decision_time = config->verifier_decision_time(
                    simulated_time, suspect, suspect_time, verificator_keycode,
                    verificator_time);

                // well, this is an abuse ... this should never be 0.
                if (decision_time == 0) {
                    mdb("absurd\n"); // this means that verificator key verifies
                    // immediately!
                }
                if (decision_time < mDecision_time)
                    mDecision_time = decision_time;

                internal_queue.push(ev.release());
                return;
            };
        }
    };

    /**
     * One key-event investigation finished,
     * now reset for the next one */
    void rewind_machine(const fork_state_t new_state) {
        check_locked();
#if 0
        assert ((new_state == st_deactivated) || (new_state == st_activated));
        change_state(new_state);
#endif
        /* reset the machine */
        mdb("== Resetting the fork machine (internal %d, input %d)\n",
            internal_queue.length (),
            input_queue.length ());

        change_state(st_normal);
        mDecision_time = 0; // nothing to decide
        suspect = no_key;
        verificator_keycode = no_key;
        reverse_splice(internal_queue, input_queue);
    }



   /**
    * We concluded the key is forked. "Output" it and prepare for the next one.
    * fixme: locking -- possibly unlocks?
    */
    void activate_fork(fork_reason_t fork_reason) {
        check_locked();
        assert(!internal_queue.empty());

        std::unique_ptr<key_event> event(internal_queue.pop());

        // here or at the end?:
        rewind_machine(st_activated);

        Keycode forked_key = environment->detail_of(event->p_event);
        // why not:
        // assert(forked_key == suspect);
        event->original_keycode = forked_key;

        /* Change the keycode, but remember the original: */
        forkActive[forked_key] = config->fork_keycode[forked_key];
        // todo: use std::format(), not hard-coded %d
        mdb("%s the key %d-> forked to: %d. Internal queue has %d events. %s\n",
            __func__,
            forked_key, forkActive[forked_key],
            internal_queue.length (),
            describe_machine_state(this->state));

        environment->rewrite_event(event->p_event, forkActive[forked_key]);
        issue_event(std::move(event));
    };



    /**
     * Operations on the machine
     * fixme: should it include the `self_forked' keys ?
     * `self_forked' means, that i decided to NOT fork. to mark this decision
     * (for when a repeated event arrives), i fork it to its own keycode
     */

    void
    change_state(fork_state_t new_state)
    {
        state = new_state;
        mdb(" --->%s[%dm%s%s\n", escape_sequence, 32 + new_state,
            state_description[new_state], color_reset);
    }

    /**
     * Now the operations on the Dynamic state
     */

    /**
     * Take from `input_queue', + the mCurrent_time + force  -> run the machine.
     */
    void run_automaton(bool force_also) {
        // fixme: maybe All I need is the nextPlugin?
        std::scoped_lock lock(mLock);
        if (environment->output_frozen() ||
            (!input_queue.empty() || !internal_queue.empty())
           ) {
            // log_queues_and_nextplugin(message)
            mdb("%s: next %sfrozen: internal %d, input: %d\n", __func__,
                (environment->output_frozen()?"":"NOT "),
                internal_queue.length(),
                input_queue.length());
        }

        // notice that instead of recursion, all the calls to `rewind_machine' are
        // followed by return to this cycle!
        while (! environment->output_frozen()) {
            if (! input_queue.empty()) {
                std::unique_ptr<key_event> ev(input_queue.pop());
                transition_by_key(std::move(ev)); // here crash?
            } else {
                if (mCurrent_time && (state != st_normal)) {
                    if (transition_by_time(mCurrent_time))
                        // If this time helped to decide -> machine rewound,
                        // we have to try again, maybe the queue is not empty?.
                        continue;
                }

                if (force_also && (state != st_normal)) {
                    transition_by_force();
                } else {
                    break;
                }
            }
        }
    };


    // so EVENT confirms fork of the current event, and also enters queue,
    // which will be `immediately' relocated to the input queue.
    void confirm_fork_and_enqueue(std::unique_ptr<key_event> event, fork_reason_t fork_reason) {
        /* fixme: event is the just-read event. But that is surely not the head
           of queue (which is confirmed to fork) */
        mdb("confirm:\n");
        internal_queue.push(event.release());
        activate_fork(fork_reason);
    }

    // So the event proves, that the current event is not forked.
    // /----internal--queue--/ event /----input event----/
    //  ^ suspect                ^ confirmation.
    void do_confirm_non_fork_by(std::unique_ptr<key_event> ev) {
        check_locked();
        assert(state == st_suspect || state == st_verify);

        std::unique_ptr<key_event> non_forked_event(internal_queue.pop());
        mdb("this is not a fork! %d\n",
            environment->detail_of(non_forked_event->p_event));
        internal_queue.push(ev.release());
        rewind_machine(st_deactivated); // short-lived state. is it worth it?
        // possibly unlocks
        issue_event(std::move(non_forked_event));
    };

    /**
     * Timeline:
     * ========
     * first
     * second .. verifier
     * third-event  < we are here now.
     *
     * ???? how long?
     * second Released.
     * So, already 2 keys have been pressed, and still no decision.
     * Now we have the 3rd key.
     * We wait only for time, and for the release of the key
     */
    void apply_event_to_verify_state(std::unique_ptr<key_event> ev) {
        const PlatformEvent* pevent = ev->p_event;
        Time simulated_time = environment->time_of(pevent);
        Keycode key = environment->detail_of(pevent);

        /* We pressed a forkable key I, and another one E (which could possibly
           use the modifier). Now, either the forkable key was intended
           to be `released' before the press of the other key (and we have an
           error due to mis-synchronization), or in fact, the forkable
           was actually `used' as a modifier.

           This should not be fork:
           I----I (short)
           E--E

           This should be a fork:
           I-----I (long)
           E--E

           Motivation:  we want to press the modifier for short time (simultaneously
           pressing other keys). But sometimes writing quickly, we
           press before we release the previous letter. We handle this, ignoring
           a short overlay. I.e. we wait for the verification key
           to be down in parallel for at least X ms.

           There might be a matrix of values! How to train it?
        */

        /* As before, in the suspect case, we check the 1-key timeout ? But this time,
           we have the 2 key, and we can have a more specific parameter:  Some keys
           are slow to release, when we press a specific one afterwards. So in this case fork slower!
        */

        if (0 == (mDecision_time = key_pressed_too_long(simulated_time))) {
            confirm_fork_and_enqueue(std::move(ev), fork_reason_t::reason_long);
            return;
        }

        /* now, check the overlap of the 2 first keys */
        Time decision_time = config->verifier_decision_time(simulated_time,
                                                            suspect, suspect_time,
                                                            verificator_keycode, verificator_time);
        if (decision_time == 0) {
            confirm_fork_and_enqueue(std::move(ev), fork_reason_t::reason_overlap);
            return;
        }

        // how is this possible?
        if (decision_time < mDecision_time)
            mDecision_time = decision_time;

        if ((key == suspect) && environment->release_p(pevent)){ // fixme: is release_p(event) useless?
            mdb("fork-key released on time: %" TIME_FMT "ms is a tolerated error (< %lu)\n",
                (simulated_time -  suspect_time),
                config->verification_interval_of(suspect,
                                                 verificator_keycode));
            do_confirm_non_fork_by(std::move(ev));

        } else if ((verificator_keycode == key) && environment->release_p(pevent)) {
            // todo: maybe this is too weak.

            // todo: we might be interested in percentage, then here we should do the work!

            change_state(st_suspect);
            verificator_keycode = 0;   // we _should_ take the next possible verificator
            internal_queue.push(ev.release());
        } else {
            // fixme: a (repeated) press of the verificator ?
            // fixme: we pressed another key: but we should tell XKB to repeat it !
            internal_queue.push(ev.release());
        };
    };

    // is mDecision_time always recalculated?
    // possibly unlocks
    void apply_event_to_normal(std::unique_ptr<key_event> event) {
        const PlatformEvent *pevent = event->p_event;

        const Keycode key = environment->detail_of(pevent);
        const Time simulated_time = environment->time_of(pevent);

        assert(state == st_normal && internal_queue.empty());

        // if this key might start a fork....
        if (forkable_p(config.get(), key)
            && environment->press_p(pevent)
            && !environment->ignore_event(pevent)) {
            /* ".-" AR-trick: by depressing/re-pressing the key rapidly, AR is invoked, not fork */
#if DEBUG
            if ( !key_forked(key) && (last_released == key )) {
                mdb("can we invoke autorepeat? %d  upper bound %d ms\n",
                    // mmc: config is pointing outside memory range!
                    (int)(simulated_time - last_released_time), config->repeat_max);
            }
#endif
            /* So, unless we see the .- trick, we do suspect: */
            if (!key_forked(key) &&
                ((last_released != key )
                 || time_difference_more(simulated_time, last_released_time, config->repeat_max))) {

                change_state(st_suspect);
                suspect = key;
                suspect_time = environment->time_of(pevent);
                mDecision_time = suspect_time +
                    config->verification_interval_of(key, 0);
                internal_queue.push(event.release());
                return;
            } else {
                // .- trick: (fixme: or self-forked)
                mdb("re-pressed very quickly\n");
                forkActive[key] = key; // fixme: why not 0 ?
                issue_event(std::move(event));
                return;
            };
        } else if (environment->release_p(pevent) && (key_forked(key))) {
            mdb("releasing forked key\n");
            // fixme:  we should see if the fork was `used'.
            if (config->consider_forks_for_repeat){
                // C-f   f long becomes fork. now we wanted to repeat it....
                last_released = environment->detail_of(pevent);
                last_released_time = environment->time_of(pevent);
            } else {
                // imagine mouse-button during the short 1st press. Then
                // the 2nd press ..... should not relate the the 1st one.
                last_released = no_key;
                last_released_time = 0;
            }
            /* we finally release a (self-)forked key. Rewrite back the keycode.
             *
             * fixme: do i do this in other machine states?
             */
            environment->rewrite_event(const_cast<PlatformEvent*>(pevent), forkActive[key]);
            forkActive[key] = 0;

            issue_event(std::move(event));
        } else {
            // non forkable, for example:
            if (environment->release_p(pevent)) {
                last_released = environment->detail_of(pevent);
                last_released_time = environment->time_of(pevent);
            };
            // pass along the un-forkable event.
            issue_event(std::move(event));
        };
    }

    // todo: so Time type must allow 0
    // return 0  if the current/first key is pressed enough time to fork.
    // or time when this will happen.
    Time key_pressed_too_long(Time current_time) {
        assert(state== st_verify || state == st_suspect);

        // verificator_keycode
        Time verification_interval = config->verification_interval_of(suspect,no_key);
        // note: this can be 0 (& should be, unless)

        Time decision_time = suspect_time + verification_interval;

        mdb("time: verification_interval = %dms elapsed so far =%dms\n",
            verification_interval,
            (int)(current_time - suspect_time));

        if (decision_time <= current_time)
            return 0;
        else
            return decision_time;
    };
    Time verifier_decision_time(Time current_time);

   /* Return the keycode into which CODE has forked _last_ time.
   Returns code itself, if not forked. */
    [[nodiscard]] bool
    key_forked(Keycode code) const {
        return (forkActive[code]);
    }


    /** Another event has been determined. So:
     * todo:  possible emit a (notification) event immediately,
     * ... and push the event down the pipeline, when not frozen.
     */
    void issue_event(std::unique_ptr<key_event> ev) {
        assert(ev->p_event != nullptr);
        mdb("%s: %p %p\n", __func__, ev.get(), ev->p_event);
        output_queue.push(ev.release());
    }

    // can modify the event!
    void relay_event(key_event *event) {
        // (ORDER) this event must be delivered before any other!
        // so no preemption of this part!  Are we re-entrant?
        // yet, the next plugin could call in here? to do what?

        // machine. fixme: it's not true -- it cannot!
        // 2020: it can!
        // so ... this is front_lock?

        // why unlock during this? maybe also during the push_time_to_next then?
        // because it can call into back to us? NotifyThaw()

        // fixme: was here a bigger message?
        // bug: environment->fmt_event(ev->p_event);
        unlock();
        // we must gurantee ORDER
        environment->relay_event(event->p_event);
        lock();
    };


public:
    // prefix with a space.
    void mdb(const char* format...) const
    {
        if (config->debug) {
            // alloca()
            char* new_format = (char*) malloc(strlen(format) + 2);
            new_format[0] = ' ';
            strcpy(new_format + 1, format);

            va_list argptr;
            va_start(argptr, format);
            environment->vlog(new_format, argptr);
            va_end(argptr);

            free(new_format);
        }
    };

    // without the leading space
    void mdb_raw(const char* format...) const {
        if (config->debug) {
            va_list argptr;
            va_start(argptr, format);
            environment->vlog(format, argptr);
            va_end(argptr);
        }
    };

    ~forkingMachine() {};

    explicit forkingMachine(Environment_t* environment)
        : environment(environment),
          state(st_normal), suspect(0), verificator_keycode(0), suspect_time(0),
          last_released(KEYCODE_UNUSED), last_released_time(0),
          mDecision_time(0),
          mCurrent_time(0),
          internal_queue("internal"),
          input_queue("input_queue"),
          output_queue("output_queue"),
          config(nullptr) {

        key_event::env = environment;
        environment->log("ctor: allocating last_events\n");
        last_events_log.set_capacity(max_last);
        environment->log("ctor: allocated last_events %lu (%lu\n", last_events_log.size(), max_last);

        environment->log("ctor: resetting forkActive\n");
        for (auto &i: forkActive) { // unsigned char
            i = KEYCODE_UNUSED; /* not active */
        };
        environment->log("ctor: end\n");
    };

    /** update the configuration */
    int configure_global(int type, int value, bool set) {
        std::scoped_lock lock(mLock);
        const auto fork_configuration = this->config.get();

        switch (type) {
        case fork_configure_overlap_limit:
            if (set)
                fork_configuration->overlap_tolerance[0][0] = value;
            else
                return fork_configuration->overlap_tolerance[0][0];
            break;

        case fork_configure_total_limit:
            if (set)
                fork_configuration->verification_interval[0][0] = value;
            else
                return fork_configuration->verification_interval[0][0];
            break;

        case fork_configure_clear_interval:
            if (set)
                fork_configuration->clear_interval = value;
            else
                return fork_configuration->clear_interval;
            break;

        case fork_configure_repeat_limit:
            if (set)
                fork_configuration->repeat_max = value;
            else
                return fork_configuration->repeat_max;
            break;

        case fork_configure_repeat_consider_forks:
            if (set)
                fork_configuration->consider_forks_for_repeat = value;
            return fork_configuration->consider_forks_for_repeat;
        case fork_configure_last_events:
            if (set)
                set_last_events_count(value);
            else
                return max_last;
            break;
        case fork_configure_debug:
            if (set) {
                //  here we force, rather than using MDB !
                mdb("fork_configure_debug set: %d -> %d\n",
                    config->debug,
                    value);
                fork_configuration->debug = value;
            } else {
                mdb("fork_configure_debug get: %d\n",
                    fork_configuration->debug);
                return fork_configuration->debug; // (bool) ?True:FALSE
            }
            break;

        case fork_server_dump_keys:
            dump_last_events(environment->get_event_dumper().get());
            break;

            // mmc: this is special:
        case fork_configure_switch:
            assert(set);

            mdb("fork_configure_switch: %d\n", value);
#if MULTIPLE_CONFIGURATIONS
            switch_config(value);
#endif
            break;

        default:
            mdb("%s: invalid option %d\n", __func__, value);
        }
        return 0;
    }

/**
 * key and twin have a relationship, given by type.
 * |--------------|========\-----------\
 * A key pressed  B pressed B released  A released
 *
 * |----------- |========\------------\
 * A pressed    B pressed A released  B released
 */
    enum twin_parameter {
        verification_time = 1, // A--B-- t  -> A is forked.
        overlap_limit = 2,     // A B--t  -> A is forked.
        // notice t is measured from A and B
    };

    int configure_twins(int type, Keycode key, Keycode twin, int value, bool set) {
        switch (type) {
        case fork_configure_total_limit:
            if (set)
                config->verification_interval[key][twin] = value;
            else
                return config->verification_interval[key][twin];
            break;
        case fork_configure_overlap_limit:
            if (set)
                config->overlap_tolerance[key][twin] = value;
            else return config->overlap_tolerance[key][twin];
            break;
        default:
            mdb("%s: invalid type %d\n", __func__, type);;
        }
        return 0;
    }

    enum key_parameters {
        key_fork,                   // Keycode
        key_repeat,                 // true/false
    };
    int configure_key(int type, Keycode key, int value, bool set) {
        mdb("%s: keycode %d -> value %d, function %d\n",
            __func__, key, value, type);

        switch (type) {
        case fork_configure_key_fork: // define  key -> key2 ?
            if (set)
                config->fork_keycode[key] = value;
            else return config->fork_keycode[key];
            break;
        case fork_configure_key_fork_repeat: // if AR?
            if (set)
                config->fork_repeatable[key] = value;
            else return config->fork_repeatable[key];
            break;
        default:
            mdb("%s: invalid option %d\n", __func__, value);
        }
        return 0;
    };

    /** ask the platform environment to send events as data. */
    int dump_last_events_to_client(event_publisher<archived_event_t>* publisher, int max_requested) {
        // I don't need to count them! last_events_count
        // should be locked
        int queue_count = last_events_log.size();

        if (max_requested > queue_count) {
            max_requested = queue_count;
        };

        publisher->prepare(max_requested);

        std::function<void(const archived_event_t&)> lambda =
            [publisher](const archived_event_t& ev){ publisher->event(ev); };
        // auto f = std::function<void(const archived_event&)>(bind(publisher->event(), publisher,));

        // todo:
        // fixme: we need to increase an iterator .. pointer .... to the C array!
        // last_events.
        for_each(last_events_log.begin(),
                 last_events_log.end(),
                 lambda);

        mdb("sending %d events\n", max_requested);

        return publisher->commit();
    };



private:

    /**
     * Push as many as possible from the OUTPUT queue to the next layer.
     * Also the time.
     * The machine is locked here.  It also does not change state. Only the 1
     *queue. Unlocks to be re-entrant!
     **/
    void flush_to_next() {
        while (!environment->output_frozen() && !output_queue.empty()) {
            std::scoped_lock lock(mLock);
            std::unique_ptr<key_event> event(output_queue.pop());
            save_event_log(event.get());
            // unlocks!
            relay_event(event.get());
        }
        if (!environment->output_frozen()) {
            push_time_to_next();
        }
        if (!output_queue.empty())
            mdb("%s: still %d events to output\n", __func__, output_queue.length());
    }

    void push_time_to_next() {
        // send out time:

        // interesting: after handing over, the nextPlugin might need to be refreshed.
        // if that plugin is gone. todo!

        Time now;
        if (!output_queue.empty()) {
            now = queue_front_time(output_queue);
        } else if (!internal_queue.empty()) {
            now = queue_front_time(internal_queue);
        } else if (!input_queue.empty()) {
            now = queue_front_time(input_queue);
        } else {
            now = mCurrent_time;
            // in this case we might:
            mCurrent_time = 0;
        }

        if (now) {
            // this can thaw, freeze,?
            environment->push_time(now);
        }
    }

    // fixme: returned by the accept_* public API methods
    [[nodiscard]] Time next_decision_time() const {
        if ((state == st_verify)
            || (state == st_suspect))
            // we are indeed waiting:
            return mDecision_time;
        else
            return 0;
    }

public:
    /** Create 2 configuration sets:
        0. w/o forking,  no-op.
        1. user-configurable
        this is on loading, so should not use Abort allocation policy:

        @return false if allocation  failed.
    */
    bool create_configs() {
        std::scoped_lock lock(mLock);

        environment->log("%s\n", __func__);

        try {
#if MULTIPLE_CONFIGURATIONS
            auto config_no_fork = std::unique_ptr<fork_configuration>(new fork_configuration);
            config_no_fork->debug = 0; // should be settable somehow.

            auto user_configurable = std::unique_ptr<fork_configuration>(new fork_configuration);
            user_configurable->debug = 1;

            // todo:
            // user_configurable->next = config_no_fork.release();

            config = user_configurable.release();
#else
            config = std::make_unique<fork_configuration>();
#endif
            return true;

        } catch (std::bad_alloc &exc) {
            return false;
        }
    }

    void dump_last_events(event_dumper<archived_event_t>* dumper) const {
        std::scoped_lock lock(mLock);
#if 0
        std::function<void(const event_dumper&, const archived_event_t&)> doit0 = &event_dumper::operator();
        // lambda?
        std::function<void(const archived_event_t&)> doit = std::bind(&event_dumper::operator(), doit, placeholders::_1);
#else
        std::function<void(const archived_event_t&)> lambda = [dumper](const archived_event_t& ev){ dumper->operator()(ev); };
#endif
        if (last_events_log.full()) {
            std::for_each(last_events_log.begin(),
                          last_events_log.end(),
                          lambda);
        } else {
            std::for_each(last_events_log.begin(),
                          last_events_log.begin() + last_events_log.size(),
                          lambda);
        }
    }

private:
    /**
     * Logging ... why templated?
     */
    void log_state(const char *message) const {
        mdb("%s%s%s state: %s, queue: %d.  %s\n", fork_color, __func__, color_reset,
            describe_machine_state(this->state), internal_queue.length(), message);
    }

    void log_queues(const char *message) const {
        mdb("%s: Queues: output: %d\t internal: %d\t input: %d\n", message,
            output_queue.length(), internal_queue.length(), input_queue.length());
    }

    void log_state_and_event(const char* message, const key_event *ev) {

        mdb("%s%s%s state: %s, queue: %d\n", // , event: %d %s%c %s %s
            info_color,message,color_reset,
            describe_machine_state(this->state),
            internal_queue.length ()
            );
        environment->fmt_event(ev->p_event);
    }

public:
// main api:

    /**
     *  We take over pevent and promise to deliver back via
     *  relay_event -> hand_over_event_to_next_plugin
     *
     *  todo: PlatformEvent is now owned ... it will be destroyed by
     * Environment.
     */
    Time accept_event(std::unique_ptr<PlatformEvent> pevent) noexcept(false) {
        {
            std::scoped_lock lock(mLock);
            // environment->fmt_event(pevent.get());
            // mdb("%s: event time: %ul\n", __func__, );

            // mdb("%s: event time: %ul\n", __func__, environment->time_of(pevent.get()));

            // fixme: mouse must not preempt us. But what if it does?
            // mmc: allocation:
            auto event = std::make_unique<key_event>(std::move(pevent));

            // mdb("event time: %ul\n", environment->time_of(event->p_event));

            if (mCurrent_time > environment->time_of(event->p_event))
                mdb("bug: time moved backwards!");
#if 0
            PlatformEvent* ref = event->p_event;
            environment->relay_event(ref);
            event->p_event = ref;
            return 0;
#endif
            input_queue.push(event.release());
        }
        run_automaton(false);

        flush_to_next();
        return next_decision_time();
    }


    Time accept_time(const Time now) {
        {
            std::scoped_lock lock(mLock);
            /* push the time ! */
            if (mCurrent_time > now)
                mdb("bug: time moved backwards!");
            else
                mCurrent_time = now;
        }

        run_automaton(false);
        flush_to_next();
        return next_decision_time();
    }

    /**
     * low-level machine step.
     */
    void transition_by_force() {
      if (state == st_normal) {
        // so (internal_queue.empty())
        return;
      }

      if (state == st_deactivated) {
        environment->log("%s: BUG.\n", __func__);
        return;
      }

      /* so, the state is one of: verify, suspect or activated. */
      log_state(__func__);

      // bug: it might activate multiple forks!
      activate_fork(fork_reason_t::reason_force);
    }

    /** public api
     * Called by mouse button press processing.
     * Make all the forkable (pressed)  forked! (i.e. confirm them all)
     * (could use a bitmask to configure what reacts)
     * If in Suspect or Verify state, force the fork. (todo: should be
     * configurable)
     */
    void accept_confirmation() { // fixme!
        /* bug: if we were frozen, then we have a sequence of keys, which
         * might be already released, so the head is not to be forked!
         */
        run_automaton(true);
        flush_to_next();        // does this clash?
    }

};

}


// extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
