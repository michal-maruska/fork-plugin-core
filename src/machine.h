// (c) Michal Maruska 2003-2024
#pragma once

#ifdef KERNEL
#define assert(x)
#else
#include <assert.h>
#endif

#include "config.h"

#include "triqueue.h"
#include "platform.h"
#include "colors.h"

#ifndef DISABLE_STD_LIBRARY
#include <mutex>
// a couple of unique_ptr
#include <memory>
#include <algorithm>
#include <functional>
#endif

#ifndef KERNEL
#include <cstdlib>
#include <cstring>
#endif

#include "fork_enums.h"
#include "fork_configuration.h"

namespace forkNS {

/**
 * Machine:
 * ... keeps log of the most-recent `archived_event_t'
 *
 * It is invoked and itself invokes `platformEnvironment'
 * it receives `PlatformEvent' abstract class
 * platformEnvironment extracts ... Keycode and Time
 *
 * and we update some state and sometimes rewrite the event
 * and output.
 *
 *
 * Concepts:
 * archived_event_t ... contains platformEvent and "forked"
 */

template <typename Keycode,
          typename Time, // these will be decltype(keycode_of(PlatformEvent))
          typename PlatformEvent,
          typename Environment,
          typename archived_event_t,
          typename last_events_t,
          int MAX_KEYCODE = 256>

class forkingMachine {
    constexpr static const Keycode no_key = 0;
    /* Environment must be able to convert from
     * PlatformEvent to archived_event_t
     */

// fixme:
// static_assert(std::is_same_v<Keycode, decltype(keycode_of(PlatformEvent()))>);

// Types
public:
    using fork_configuration = ForkConfiguration<Keycode, Time, MAX_KEYCODE>;

private:

#ifndef DISABLE_STD_LIBRARY
    mutable std::mutex mLock;
    using  scoped_lock = std::scoped_lock<std::mutex>;

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
        // assert(mLock.locked);
    }
    void check_unlocked() const {
        // assert(mLock == 0);
    }
#else
    int mLock = 0;

    template <typename mutex>
    class empty_scoped_lock
    {
    public:
        empty_scoped_lock(mutex m) {
            UNUSED(m);
        };

        ~empty_scoped_lock() {}
    };
    using  scoped_lock = empty_scoped_lock<int>;

    void lock() const {};
    void unlock() const {};
    void check_locked() const {}
    void check_unlocked() const {}
#endif




public:

    // todo: HAVE_SMART_POINTERS
#ifndef DISABLE_STD_LIBRARY
    std::unique_ptr<Environment> environment;
#else
    Environment *environment;

    void* operator new(size_t size, void* p) noexcept {
        UNREFERENCED_PARAMETER(size);
        return p;
    }
#endif


private:
    /* states of the automaton: */
    enum fork_state_t {
        st_normal,
        st_suspect,             // difference ?
        st_verify,              // current keycode seems but...?
        st_deactivated,
        st_activated
    };

    /* This is how it works:
     * We have a `state' and 3 queues:
     *
     *  output    |   internal     | input
     *  waits for |
     *  thaw      |  Kxxxxxx       |  yyyyy
     *               ^ forked?
     *
     * We push at the end of input Q. Then we pop from that Q and push on
     * Internal where we determine for 1 event, if forked/non-forked.
     *
     * Then we push on the output Q. At that moment, we also restart: all
     * from internal Q is returned/prepended to the input Q.
     */

    // rust: enum {normal_state, suspect(suspect, verificator) ....}
    fork_state_t state;

    // only for certain states we keep (updated):
    Keycode suspect;
    Keycode verificator_keycode;

    // these are "registers"
    Time suspect_time;               /* time of the 1st event in the internal queue. */
    Time verificator_time = 0;       /* press of the `verificator' */

    /* To allow AR for forkable keys:
     * When we press a key the second time in a row, we might avoid forking:
     * So, this is for the detector:
     *
     * This means I cannot do this trick w/ 2 keys, only 1 is the last/considered! */
    Keycode last_released;
    Time last_released_time;


    // calculated:
    Time mDecision_time;        // Time to wait... so that the HEAD event in queue could decide more
    Time mCurrent_time;         // the last time we received from previous plugin/device


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


    triqueue_t<PlatformEvent, Environment> tq{100}; // total capacity

    // template <typename Time>
    bool time_difference_more(Time now, Time past, Time limit_difference) {
        // return (now > past + limit_difference);
        // it's supposed to be monotonic, and 0... number is always included in the type range.
        return ( (now - past) > limit_difference);
    }

    last_events_t last_events_log;
    int max_last = 10; // can be updated!

public:

    /* forkActive(x) == y  means we sent downstream Keycode Y instead of X.
     * what is the meaning of:  0 and X ? */
    Keycode          forkActive[MAX_KEYCODE] = {};
#ifndef DISABLE_STD_LIBRARY
    std::unique_ptr<fork_configuration> config; // list<fork_configuration>
#else
    fork_configuration* config;
#endif

    // prefix with a space.
    void mdb(const char* format...) const
    {
        if (config->debug) {
            va_list argptr;
            va_start(argptr, format);
#ifdef KERNEL
            environment->vlog(format, argptr);
#else
            // does MS/kernel have alloca?
            char* new_format = (char*) alloca(strlen(format) + 2);
            new_format[0] = ' ';
            strcpy(new_format + 1, format);
            environment->vlog(new_format, argptr);
#endif
            va_end(argptr);

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

    // we don't store/own anything? or environment is unique_ptr but in kernel it's not!
    ~forkingMachine() {};

    explicit forkingMachine(Environment* environment)
        : environment(environment),
          state(st_normal), suspect(0), verificator_keycode(0), suspect_time(0),
          last_released(KEYCODE_UNUSED), last_released_time(0),
          mDecision_time(0),
          mCurrent_time(0),
          config(nullptr) {

        triqueue_t<PlatformEvent, Environment>::env = environment;

        environment->log("ctor: allocating last_events\n");
        last_events_log.set_capacity(max_last);
        environment->log("ctor: allocated last_events %lu (%lu\n", last_events_log.size(), max_last);

        environment->log("ctor: resetting forkActive\n");
        for (auto &i: forkActive) { // unsigned char
            i = KEYCODE_UNUSED; /* not active */
        };
        environment->log("ctor: end\n");
    };

    /** update the configuration
     * @type the feature/parameter
     * @value .. either parameter is set to this value if @set is 1
     * or ... ignored  */
    int configure_global(fork_configuration_t type, int value, bool set) {
        scoped_lock lock(mLock);
        const auto fork_configuration =
#ifndef DISABLE_STD_LIBRARY
            this->config.get()
#else
            this->config
#endif
            ;

        switch (type) {
#if VERIFICATION_MATRIX
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

#endif // VERIFICATION_MATRIX
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
//            dump_last_events(environment->get_event_dumper().get());
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

    void set_debug(int level) {
        scoped_lock lock(mLock);
        config->debug = level;
        // (machine->config->debug? 0: 1);
    }

    void stop() {
        // wait & stop
        scoped_lock wait_lock(mLock);
    }


private:
    void set_last_events_count(const int new_max) {
        check_locked();
        mdb("%s: allocating %d events\n", __func__, new_max);

        if (max_last > new_max) {
            // shrink. todo! in the circular.h!
            mdb("%s: shrinking unimplemented\n", __func__);
        } else {
            last_events_log.set_capacity(new_max);
            max_last = new_max;
        }
    }

    void save_event_to_log(const PlatformEvent& event) {
        // could I emplace it?
        // reference = last_events_log.emplace_back()
        // reference.forked = ev->forked;
        UNUSED(event);
#if 0
        archived_event_t archived_event;
        environment->archive_event(archived_event, event->p_event);
        archived_event.forked = event->original_keycode; // todo: rename original_keycode

        last_events_log.push_back(archived_event);
#endif
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
#if 0
        static char buffer[BufferLength];
        snprintf(buffer, BufferLength, "%s[%dm%s%s",
                 escape_sequence, 32 + state,
                 state_description[state], color_reset);
        return buffer;
#else
        UNUSED(state);
        return "";
#endif
    }

#if MULTIPLE_CONFIGURATIONS
    fork_configuration** find_configuration_n(int n);

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
#endif

    /** Another event has been determined. So:
     * todo:  possible emit a (notification) event immediately,
     * ... and push the event down the pipeline, when not frozen.
     */
    void issue_event() {
        // assert(ev->p_event != nullptr);
        // mdb("%s: %p %p\n", __func__, ev.get(), ev->p_event);
        tq.move_to_first();
        // output_queue.push(ev.release());
    }

    // so EVENT confirms fork of the current event, and also enters queue,
    // which will be `immediately' relocated to the input queue.
    void confirm_fork_and_enqueue(fork_reason_t fork_reason) {
        /* fixme: event is the just-read event. But that is surely not the head
           of queue (which is confirmed to fork) */
        mdb("confirm:\n");

        activate_fork(fork_reason);
    }

    // So the event proves, that the current event is not forked.
    // /----internal--queue--/ event /----input event----/
    //  ^ suspect                ^ confirmation.
    void do_confirm_non_fork_by() {
        check_locked();
        UNUSED(reason);
        assert(state == st_suspect || state == st_verify);
#if 0
        std::unique_ptr<key_event> non_forked_event(internal_queue.pop());
        mdb("this is not a fork! %d\n",
            environment->detail_of(non_forked_event->p_event));
        internal_queue.push(ev.release());
#endif
        issue_event();
        rewind_machine(st_deactivated); // short-lived state. is it worth it?
        // possibly unlocks
    };


    /**  First (press)
     *    v   ^     (0   <-- we are here. Input q
     *        Second|
     *             next @event
     */
    void apply_event_to_suspect(const PlatformEvent &pevent) {
        assert(state == st_suspect);

        Time simulated_time = environment->time_of(pevent);
        Keycode key = environment->detail_of(pevent);

        /* Here, we can
         * o refuse .... if suspected/forkable is released quickly,
         * o fork (definitively),  ... for _time_
         * o start verifying, or wait, or confirm (timeout)
         * todo: I should repeat a bi-depressed forkable.
         */

        // first we look at the time:
        if (0 == (mDecision_time = key_pressed_too_long(simulated_time))) {
            confirm_fork_and_enqueue(fork_reason_t::reason_long);
            return;
        };

        /* So, we now have a second key, since the duration of 1 key
         * was not enough. */
        if (environment->release_p(pevent)) {
            mdb("suspect/release: suspected = %d, time diff: %d\n", suspect,
                (int)(simulated_time - suspect_time));
            if (key == suspect) {
                do_confirm_non_fork_by();
                return;
                /* fixme:  here we confirm, that it was not a user error.....
                   bad synchro. i.e. the suspected key was just released  */
            } else {
                /* something released, but not verificating, b/c we are in `suspect',
                 * not `confirm'  */
                tq.move_to_second();
                return;
            };
        } else {
            if (!environment->press_p(pevent)) { // why not release_p() ?
                // RawPress & Device events.
                if (!environment->release_p(pevent)) {
                    mdb("a bizzare event scanned\n");
                }
                tq.move_to_second();
                return;
            }
            // press-event here:
            if (key == suspect) {
                /* How could this happen? Auto-repeat on the lower/hw level?
                 * And that AR interval is shorter than the fork-verification */
                if (config->fork_repeatable[key]) {
                    mdb("The suspected key is configured to repeat, so ...\n");
                    forkActive[suspect] = suspect;
                    do_confirm_non_fork_by();
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

                tq.move_to_second();
                return;
            };
        }
    };


    // is mDecision_time always recalculated?
    // possibly unlocks
    void apply_event_to_normal(const PlatformEvent &pevent) {

        const Keycode key = environment->detail_of(pevent);
        const Time simulated_time = environment->time_of(pevent);

        assert(state == st_normal);

        // if this key might start a fork....
        if (forkable_p(
#ifndef DISABLE_STD_LIBRARY
                config.get()
#else
                config
#endif
                , key)
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

                tq.move_to_second();
                return;
            } else {
                // .- trick: (fixme: or self-forked)
                mdb("re-pressed very quickly\n");
                forkActive[key] = key; // fixme: why not 0 ?

                // double move:
                tq.move_to_second();
                issue_event();
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

            tq.move_to_second();
            environment->rewrite_event(tq.rewrite_head(), forkActive[key]);
            forkActive[key] = 0;
            issue_event();
        } else {
            // non forkable, for example:
            if (environment->release_p(pevent)) {

                last_released = environment->detail_of(pevent);
                last_released_time = environment->time_of(pevent);
            };
            // pass along the un-forkable event.
            tq.move_to_second();
            issue_event();
        };
    }

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
    void apply_event_to_verify_state(const PlatformEvent &pevent) {
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
            confirm_fork_and_enqueue(fork_reason_t::reason_long);
            return;
        }

        /* now, check the overlap of the 2 first keys */
        Time decision_time = config->verifier_decision_time(simulated_time,
                                                            suspect, suspect_time,
                                                            verificator_keycode, verificator_time);
        if (decision_time == 0) {
            confirm_fork_and_enqueue(fork_reason_t::reason_overlap);
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
            do_confirm_non_fork_by();

        } else if ((verificator_keycode == key) && environment->release_p(pevent)) {
            // todo: maybe this is too weak.

            // todo: we might be interested in percentage, then here we should do the work!

            change_state(st_suspect);
            verificator_keycode = 0;   // we _should_ take the next possible verificator

            tq.move_to_second();
        } else {
            // fixme: a (repeated) press of the verificator ?
            // fixme: we pressed another key: but we should tell XKB to repeat it !
            tq.move_to_second();
        };
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
   void transition_by_key(const PlatformEvent& pevent) {
        check_locked();
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
#if 0
                // bug:
                tq.drop();
                environment->free_event(ev->p_event);
                // mmc:  fork again, and pass-through
#endif
                tq.move_to_second();
                activate_fork(fork_reason_t::reason_force);
                return;
            }
#endif
        // `limitation':
        // A currently forked keycode cannot be (suddenly) pressed 2nd time.
        // assert(release_p(event) || (key < MAX_KEYCODE && forkActive[key] == 0));
        switch (state) {
        case st_normal:
            apply_event_to_normal(pevent);
            return;
        case st_suspect:
        {
            apply_event_to_suspect(pevent);
            return;
        }
        case st_verify:
        {
            apply_event_to_verify_state(pevent);
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
#if 0
      mdb("%s%s%s state: %s, queue: %d, time: %u key: %d\n", fork_color,
          __func__, color_reset, describe_machine_state(this->state),
          internal_queue.length(), (int)current_time, suspect);
#endif
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


    /**
     * low-level machine step.
     */
    void transition_by_force() {
      if (state == st_normal) {
        // so (tq.middle_empty())
        return;
      }

      if (state == st_deactivated) {
        environment->log("%s: BUG.\n", __func__);
        return;
      }

      /* so, the state is one of: verify, suspect or activated. */
      log_state(__func__);

      // fixme: only if there are in inter
      if (!tq.middle_empty())
          // bug: it might activate multiple forks!
          activate_fork(fork_reason_t::reason_force);
      else
          mdb("%s: BUG -- state but empty\n", __func__);
    }


    /**
     * One key-event investigation finished,
     * now reset for the next one */
    void rewind_machine(const fork_state_t new_state) {
        check_locked();
#if 0
        assert ((new_state == st_deactivated) || (new_state == st_activated));
        change_state(new_state);
#else
        UNUSED(new_state);
#endif
        /* reset the machine */
#if 0
        mdb("== Resetting the fork machine (internal %d, input %d)\n",
            internal_queue.length (),
            input_queue.length ());
#endif

        change_state(st_normal);
        mDecision_time = 0; // nothing to decide
        suspect = no_key;
        verificator_keycode = no_key;
        tq.rewind_middle();
    }



   /**
    * We concluded the key is forked. "Output" it and prepare for the next one.
    * fixme: locking -- possibly unlocks?
    */
    void activate_fork(fork_reason_t fork_reason) {
        UNUSED(fork_reason);
        check_locked();

        PlatformEvent& pevent = tq.rewrite_head();
        Keycode forked_key = environment->detail_of(pevent);
        // why not:
        // assert(forked_key == suspect);

        // todo:
        // fixme: event->original_keycode = forked_key;

        /* Change the keycode, but remember the original: */
        forkActive[forked_key] = config->fork_keycode[forked_key];
        // todo: use std::format(), not hard-coded %d
        mdb("%s the key %d-> forked to: %d. %s\n",
            __func__,
            forked_key, forkActive[forked_key],
            describe_machine_state(this->state));

        environment->rewrite_event(pevent, forkActive[forked_key]);
        issue_event();

        rewind_machine(st_activated);
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
        scoped_lock lock(mLock);
        if (environment->output_frozen() || (! tq.middle_empty() )) {
            // log_queues_and_nextplugin(message)
#if 0
            mdb("%s: next %sfrozen: internal %d, input: %d\n", __func__,
                (environment->output_frozen()?"":"NOT "),
                internal_queue.length(),
                input_queue.length());
#endif
        }

        // notice that instead of recursion, all the calls to `rewind_machine' are
        // followed by return to this cycle!
        while (! environment->output_frozen()) {

            if (! tq.third_empty()) {
                const PlatformEvent& event = tq.peek_third();
                transition_by_key(event); // here crash?
            } else {
                if (mCurrent_time && (state != st_normal)) {
                    // !middle_empty()
                    if (transition_by_time(mCurrent_time))
                        // If this time helped to decide -> machine rewound,
                        // we have to try again, maybe the queue is not empty?.
                        continue;
                }

                if (force_also && (state != st_normal)) {
                    // !middle_empty()
                    transition_by_force();
                } else {
                    break;
                }
            }
        }
    };


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

   /* Return the keycode into which CODE has forked _last_ time.
   Returns code itself, if not forked. */
    [[nodiscard]] bool
    key_forked(Keycode code) const {
        return (forkActive[code]);
    }


    // can modify the event!
    void relay_event(const PlatformEvent& event) {
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
        environment->relay_event(event);
        lock();
    };


public:
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
#if VERIFICATION_MATRIX
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
#else
        UNUSED(type);
        UNUSED(key);
        UNUSED(twin);
        UNUSED(value);
        UNUSED(set);
#endif
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
                config->fork_keycode[key] = (Keycode) value; // fixme!
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
#if DISABLE_STD_LIBRARY
        std::function<void(const archived_event_t&)> lambda =
            [publisher](const archived_event_t& ev){ publisher->event(ev); };
        // auto f = std::function<void(const archived_event&)>(bind(publisher->event(), publisher,));

        // todo:
        // fixme: we need to increase an iterator .. pointer .... to the C array!
        // last_events.
        for_each(last_events_log.begin(),
                 last_events_log.end(),
                 lambda);
#endif
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
        while (!environment->output_frozen() && tq.can_pop()) {
            scoped_lock lock(mLock);
            PlatformEvent event = tq.pop(); // copy ?
            // fixme ... temporarily ... not pop before sending off !
            save_event_to_log(event);
            // unlocks!
            relay_event(event);
        }
        if (!environment->output_frozen()) {
            push_time_to_next();
        }
#if 0
        if (!tq.can_pop())
            mdb("%s: still %d events to output\n", __func__, output_queue.length());
#endif
    }

    void push_time_to_next() {
        // send out time:

        // interesting: after handing over, the nextPlugin might need to be refreshed.
        // if that plugin is gone. todo!

        Time now;
        const PlatformEvent *item = tq.first();
        if (item == nullptr) {
            now = mCurrent_time;
        } else {
            now = environment->time_of(*item);
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
        // bug: not locked
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
        scoped_lock lock(mLock);

        environment->log("%s\n", __func__);

#ifndef KERNEL
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
#else
        return false;
#endif // KERNEL
    }

    void dump_last_events(event_dumper<archived_event_t>* dumper) const {
        scoped_lock lock(mLock);
#if DISABLE_STD_LIBRARY
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
#endif // DISABLE_STD_LIBRARY
    }

private:
    /**
     * Logging ... why templated?
     */
    void log_state(const char *message) const {
        UNUSED(message);
#if 0
        mdb("%s%s%s state: %s, queue: %d.  %s\n", fork_color, __func__, color_reset,
            describe_machine_state(this->state), internal_queue.length(), message);
#endif
    }

    void log_queues(const char *message) const {
#if 0
        mdb("%s: Queues: output: %d\t internal: %d\t input: %d\n", message,
            output_queue.length(), internal_queue.length(), input_queue.length());
#endif
    }

    void log_state_and_event(const char* message, const PlatformEvent & pevent) {
#if 0
        mdb("%s%s%s state: %s, queue: %d\n", // , event: %d %s%c %s %s
            info_color,message,color_reset,
            describe_machine_state(this->state),
            internal_queue.length ()
            );
#endif
        environment->fmt_event(__func__, pevent);
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
    Time accept_event(const PlatformEvent& pevent) noexcept(false) {
        {
            scoped_lock lock(mLock);
#if 0
            environment->fmt_event(__func__, pevent);
#else
            // mdb("%s: event time: %ul\n", __func__, );
            mdb("%s: event %u (%s) time: %" TIME_FMT "\n",
                __func__,
                environment->detail_of(pevent),
                environment->press_p(pevent)?"press":"release",
                environment->time_of(pevent));
#endif
// todo: if  forked (= modifier), and Press repeated -> discard. Just time.
// if same press already in the queue?

            // fixme: mouse must not preempt us. But what if it does?
            // mmc: allocation:

            if (mCurrent_time > environment->time_of(pevent))
                mdb("%s: bug: time moved backwards!\n", __func__);
            // no need:
            mCurrent_time = 0;

            // here:
            if (environment->press_p(pevent)
                && forkActive[environment->detail_of(pevent)]
                //
                )
            {
                mdb("%s: skipping this Press -- it's a forked modifier and AR!\n", __func__);
                // environment->free_event(&pevent);
                // return;
            } else {
                tq.push(pevent);
            }
        }
        run_automaton(false);

        flush_to_next();
        return next_decision_time();
    }


    Time accept_time(const Time now) {
        {
            scoped_lock lock(mLock);
            /* push the time ! */
            // sometimes now is 0 -- when I ungrab-keyboard from sfc.
            if (mCurrent_time > now) {
                mdb("%s: bug: time moved backwards!\n", __func__);
                return next_decision_time();
            }
            else
                mCurrent_time = now;
        }

        run_automaton(false);
        flush_to_next();
        return next_decision_time();
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
