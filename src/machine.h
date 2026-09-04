// (c) Michal Maruska 2003-2025
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
#include <optional>
#else
#include "empty_unique_lock.h"
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
    // fixme: constraints on the types:
    // static_assert(std::is_same_v<Keycode, decltype(keycode_of(PlatformEvent()))>);
    /* Environment must be able to convert from
     * PlatformEvent to archived_event_t
     */
class forkingMachine {
    /** constants: */
    static constexpr Keycode no_key = KEYCODE_UNUSED;
    // todo: so Time type must allow 0 NO_TIME
    static constexpr Time NO_TIME = (Time) 0;

public:
    // Types
    using fork_configuration = ForkConfiguration<Keycode, Time, MAX_KEYCODE>;

private:

#if !defined(DISABLE_STD_LIBRARY) && USE_LOCKING
    mutable std::mutex mLock;
    using  unique_lock = std::unique_lock<std::mutex>;
    void check_locked() const {}
#else
    mutable int mLock = 0;

    using  unique_lock = empty_unique_lock<int>;

    void check_locked() const {}
#endif
    bool mStopped = false;


public:
    // why public?
    // todo: #ifdef HAVE_SMART_POINTERS
#ifndef DISABLE_STD_LIBRARY
    std::unique_ptr<Environment> environment;
#else
    Environment *environment;

    void* operator new(size_t size, void* p) noexcept {
        UNUSED(size);
        return p;
    }
#endif


private:
    /* decision states for one key: */
    enum fork_state_t {
        st_normal,
        st_suspect,
        st_verify,      // we suspect, but also have another `particular' key.
    };

    /* used only for debugging */
    static constexpr char const * const state_description[3] = {
        // map<fork_state_t, string>
        "normal",
        "suspect",
        "verify",
    };


    /* This is how it works:
     * We have a `state' and 3 queues:
     *
     * Q. output  |  internal   | input
     *  ----------+-------------+------
     *  waits for |
     *  thaw      |  Kxxxxxx    | yyy
     *               ^ forked?       ^ append
     *
     * We push at the end of input Q. Then we pop from that Q and push on
     * Internal where we determine for 1 event, if forked/non-forked.
     *
     * Then we push on the output queue. At that moment, we also restart: all
     * from internal Q is returned/prepended to the input Q.
     */

    // rust: enum {normal_state, suspect(suspect, verificator) ....}
    fork_state_t state;

    // only for certain states we keep (updated):
    Keycode suspect;
    Time suspect_time;               /* time of the 1st event in the internal queue. */

    Keycode verificator_keycode;
    Time verificator_time = 0;       /* press of the `verificator' */

    /* To allow AR for forkable keys:
     * When we press a key the second time in a row, we might avoid forking:
     * So, this is for the detector:
     *
     * This means I cannot do this trick w/ 2 keys, only 1 is the last/considered! */
    Keycode last_released;
    Time last_released_time;


    // calculated:
    Time mDecision_time;        // Time .. when the suspected event forks.
                                // recalculated on more context
    Time mCurrent_time;         // the last time we received from previous
                                // plugin/device. But only if after the last event.


    /* How we decided for the fork */
    enum class fork_reason_t {
        reason_long,               // key pressed too long
        reason_overlap,             // key press overlaps with another key
        reason_force,                // mouse-button was pressed & triggered fork.
        reason_short,
        reason_wrong,
    };


    triqueue_t<PlatformEvent, Environment> tq{100}; // total capacity

    static bool time_difference_more(Time now, Time past, Time limit_difference) {
        return (now > past + limit_difference);
        // it's supposed to be monotonic, and 0... number is always included in the type range.
        // return ( (now - past) > limit_difference);
    }

    last_events_t last_events_log;
    int max_last = 10; // can be updated!

public:
    /* forkActive(x) == y  means we sent downstream Keycode Y instead of X.
     * what is the meaning of:  KEYCODE_UNUSED and X ? */
    Keycode          forkActive[MAX_KEYCODE] = {};

    // not private, because used in tests: fixme!
#ifndef DISABLE_STD_LIBRARY
    std::unique_ptr<fork_configuration> config; // list<fork_configuration>
#else
    fork_configuration* config;
#endif

    // prefix with a space.
    void mdb(const char* fmt...) const {
        if (config->debug) {
            va_list argptr;
            va_start(argptr, fmt);
#ifdef KERNEL
            environment->vlog(fmt, argptr);
#else
            // does MS/kernel have alloca?
            char buf[strlen(fmt) + 2];  // VLA, +1 for space, +1 for \0
            buf[0] = ' ';
            strcpy(buf + 1, fmt);
            environment->vlog(buf, argptr);
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
    ~forkingMachine() = default;

    explicit forkingMachine(Environment* environment)
        : environment(environment),
          state(st_normal), suspect(0), suspect_time(0),
          verificator_keycode(0),
          last_released(KEYCODE_UNUSED), last_released_time(0),
          mDecision_time(0),
          mCurrent_time(0),
          config(nullptr) {

        triqueue_t<PlatformEvent, Environment>::env = environment;

        environment->log("ctor: allocating last_events\n");
        last_events_log.set_capacity(max_last);
        environment->log("ctor: allocated last_events %lu (%lu\n", last_events_log.size(), max_last);

        environment->log("ctor: resetting forkActive\n");
#ifndef DISABLE_STD_LIBRARY
        std::fill(std::begin(forkActive), std::end(forkActive), KEYCODE_UNUSED);
#else
        for (auto &i: forkActive) { // unsigned char
            i = KEYCODE_UNUSED; /* not active */
        };
#endif
        environment->log("ctor: end\n");
    };

    /** update the configuration
     * @type the feature/parameter
     * @value .. either parameter is set to this value if @set is 1
     * or ... ignored  */
    int configure_global(fork_configuration_t type, int value, bool set) {
        unique_lock lock(mLock);
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
        unique_lock lock(mLock);
        config->debug = level;
        // (machine->config->debug? 0: 1);
    }

    void stop() {
        // wait & stop
        unique_lock wait_lock(mLock);
        UNUSED(wait_lock);
        mStopped = true;
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

    bool forkable_p(Keycode code)
    {
        const fork_configuration* pconfig =
#ifndef DISABLE_STD_LIBRARY
            this->config.get()
#else
            this->config
#endif
            ;
        return (pconfig->fork_keycode[code] != no_key);
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
// see ./no/multiple-configs.cpp
#endif


    // So the event proves, that the current event is not forked.
    // /----internal--queue--/ event /----input event----/
    //  ^ suspect                ^ confirmation.
    void do_confirm_non_fork_by(fork_reason_t reason) {
        check_locked();
        UNUSED(reason);
        assert(state == st_suspect || state == st_verify);

        tq.move_to_first();
        rewind_machine();
    };

    /**
     * One key-event investigation finished,
     * now reset for the next one */
    void rewind_machine() {
        check_locked();
        /* reset the machine */
        change_state(st_normal);
        suspect = no_key;
        // suspect_time =  // not necessary
        mDecision_time = 0; // nothing to decide
        verificator_keycode = no_key;

        tq.rewind_middle();
        if (config->debug) {
            log_queues("after rewind");
        }
    }

   /**
    * We concluded the key is forked. "Output" it and prepare for the next one.
    * fixme: locking -- possibly unlocks?
    */
    void activate_fork_rewind(fork_reason_t fork_reason) {
        UNUSED(fork_reason);
        check_locked();

        // assert()
        if (tq.middle_empty()) {
            environment->log("Bug %s -- empty queue\n", __func__);
            return;
        }

        PlatformEvent& pevent = tq.peek_middle();
        Keycode original_key = environment->detail_of(pevent);
        /* Change the keycode, but remember the original: */
        forkActive[original_key] = config->fork_keycode[original_key];

        // todo: use std::format(), not hard-coded %d
        // but in kernel it's impossible
        mdb("%s[%dm%s%s: the key %d-> forked to: %d. %s\n",
            escape_sequence, 32 + (int)fork_reason,
            __func__, color_reset,

            original_key, forkActive[original_key],
            describe_machine_state(this->state));

        environment->rewrite_event(pevent, forkActive[original_key]);

        tq.move_to_first();
        rewind_machine();
    };

    /**
     * Operations on the machine
     * fixme: should it include the `self_forked' keys ?
     * `self_forked' means, that i decided to NOT fork. to mark this decision
     * (for when a repeated event arrives), i fork it to its own keycode
     */
    void change_state(const fork_state_t new_state)
    {
        state = new_state;
// #if ANSI_COLOR
        mdb(" --->%s[%dm%s%s\n", escape_sequence, 32 + new_state,
            state_description[new_state], color_reset);
    }

    // only the `release'
    void record_last_release_event(const PlatformEvent &pevent) {
        last_released = environment->detail_of(pevent);
        last_released_time = environment->time_of(pevent);
    }

    // is mDecision_time always recalculated?
    // possibly unlocks
    void apply_event_to_normal(const PlatformEvent &pevent) {

        const Keycode key = environment->detail_of(pevent);
        const Time simulated_time = environment->time_of(pevent);

        assert(state == st_normal);

        // if this key might start a fork....
        if (forkable_p(key)
            && environment->press_p(pevent)
            && !environment->ignore_event(pevent)) {
            /* ".-" AR-trick: by depressing/re-pressing the key rapidly, AR is invoked, not fork */
#if DEBUG
            if (!key_forked(key)
                && (last_released == key )) {
                mdb("can we invoke autorepeat? %" TIME_FMT ",  upper bound %d ms\n",
                    // mmc: config is pointing outside memory range!
                    (Time)(simulated_time - last_released_time), config->repeat_max);
            }
#endif
            /* So, unless we see the .- trick, we do suspect: */
            if (!key_forked(key)
                && ((last_released != key )
                    || time_difference_more(simulated_time, last_released_time, config->repeat_max))) {

                if (forkActive[key] == key) {
                    // it's AR .- now
                    tq.move_to_second();
                    tq.move_to_first();
                    return;
                }

                if (last_released == key)
                    environment->log("restarting the same re-pressed  -- not quickly\n");

                // so this state has following items:
                change_state(st_suspect);
                suspect = key;
                suspect_time = environment->time_of(pevent);
                mDecision_time = suspect_time +
                    config->verification_interval_of(key, KEYCODE_UNUSED);

                tq.move_to_second();
                return;
            } else {
                // .- trick: (fixme: or self-forked)
                mdb("re-pressed very quickly %d\n", key);

                forkActive[key] = key;

                // double move:
                if (! tq.middle_empty()) {
                    environment->log("Bug! 2nd is not empty!\n");
                }
                assert(tq.middle_empty());

                tq.move_to_second(); // this is first in the middle queue
                // move to output:
                tq.move_to_first();
                log_queues("after kicking off AR of forkable key");
                return;
            };
        } else if (environment->release_p(pevent) && forkActive[key] != no_key) {
            // fixme: but this does not happen when suspecting it?
            mdb("releasing forked key\n");

            // fixme:  we should see if the fork was `used'.
            if (config->consider_forks_for_repeat){
                // C-f   f long becomes fork.
                // now we wanted to repeat it....
                record_last_release_event(pevent);
            } else {
                // imagine mouse-button during the short 1st press. Then
                // the 2nd press ..... should not relate the the 1st one.
                // record_last_event(no_key_event)
                last_released = no_key;
                last_released_time = 0;
            }
            /* we finally release a (self-)forked key. Rewrite back the keycode.
             *  bug: not true!
             * fixme: do i do this in other machine states?
             */
            environment->rewrite_event(const_cast<PlatformEvent&>(pevent), forkActive[key]);
            forkActive[key] = no_key;
            tq.move_to_second();
            tq.move_to_first();
        } else {
            // non forkable, for example:
            if (environment->release_p(pevent)) {
                record_last_release_event(pevent);
            };
            // pass along the un-forkable event.
            tq.move_to_second();
            tq.move_to_first();
        };
    }

    /**  the internal queue:
     *    First (press)
     *    v   ^
     *        here
     */
    void apply_event_to_suspect(const PlatformEvent &pevent) {
        assert(state == st_suspect);

        const Time simulated_time = environment->time_of(pevent);
        const Keycode key = environment->detail_of(pevent);

        /* Here, we can
         * o refuse .... if suspected/forkable is released quickly,
         * o fork (definitively),  ... for _time_
         * o start verifying, or wait, or confirm (timeout)
         * todo: I should repeat a bi-depressed forkable.
         */

        /* So, we now have a second key, since the duration of 1 key
         * was not enough. */
        if (environment->release_p(pevent)) {
            mdb("suspect/release: suspected = %d, time diff: %d\n", suspect,
                (int)(simulated_time - suspect_time));
            if (key == suspect) {
                do_confirm_non_fork_by(fork_reason_t::reason_short);
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
            // press-event here:
            if (key == suspect) {
                /* How could this happen? Auto-repeat on the lower/hw level?
                 * And that AR interval is shorter than the fork-verification */
                if (config->fork_repeatable[key]) {
                    environment->log("The suspected key is configured to repeat, so ...\n");

                    // bug!
                    forkActive[suspect] = suspect;
                    do_confirm_non_fork_by(fork_reason_t::reason_wrong); // misconfiguration?
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
                verificator_keycode = key;
                /* if already we had one -> we are not in this state!
                   if the verificator becomes a modifier ?? fixme:*/

                // update mDecision_time:
                // verify overlap
                {
                    Time decision_time =
                        config->verifier_decision_time(suspect, suspect_time,
                                                       verificator_keycode, verificator_time);

                    if (decision_time < mDecision_time)
                        mDecision_time = decision_time;

                }

                tq.move_to_second();
                return;
            };
        }
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
    void apply_event_to_verify_state(const PlatformEvent &pevent) {
        const Time simulated_time = environment->time_of(pevent);
        const Keycode key = environment->detail_of(pevent);

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

        if ((key == suspect) && environment->release_p(pevent)){ // fixme: is release_p(event) useless?
            mdb("fork-key released on time: %" TIME_FMT "ms is a tolerated error (< %lu)\n",
                (simulated_time -  suspect_time),
                config->verification_interval_of(suspect,
                                                 verificator_keycode));
            do_confirm_non_fork_by(fork_reason_t::reason_short);

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
                activate_fork_rewind(fork_reason_t::reason_force);
                return;
            }
#endif
        // `limitation':
        // A currently forked keycode cannot be (suddenly) pressed 2nd time.
        // assert(release_p(event) || (key < MAX_KEYCODE && forkActive[key] == 0));
        if  (state == st_normal) {
            apply_event_to_normal(pevent);
            return;
        }

        // first we look at the `time':
        const Time simulated_time = environment->time_of(pevent);

        if (simulated_time >= mDecision_time) {
            activate_fork_rewind(fork_reason_t::reason_long);
            return;
        };

        // `bizzare' events:
        if (!environment->press_p(pevent)
            && ! environment->release_p(pevent)
            ) {
            mdb("a bizzare event scanned\n");
            tq.move_to_second();
            return;
        }

        if (state == st_suspect) {
            apply_event_to_suspect(pevent);
            return;
        }
        else // st_verify:
        {
            apply_event_to_verify_state(pevent);
            return;
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
      if (state == st_normal) {
          environment->log("Bug %s -- but state NORMAL\n", __func__);
          return false;
      }

      if (current_time >= mDecision_time) {
          activate_fork_rewind(fork_reason_t::reason_long);
          return true;
      };

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
      check_locked();
      if (state == st_normal) {
        // so (tq.middle_empty())
        return;
      }

      /* so, the state is one of: verify, suspect or activated. */
      log_state(__func__);

      // fixme: only if there are in inter
      if (!tq.middle_empty())
          // bug: it might activate multiple forks!
          activate_fork_rewind(fork_reason_t::reason_force);
      else
          mdb("%s: BUG -- state but empty\n", __func__);
    }

    /**
     * Now the operations on the Dynamic state
     */

    /**
     * Internal: Run the automaton transitions on the queues.
     * Take from `input_queue', + the mCurrent_time + force  -> run the machine.
     * Assumes mLock is already held by caller.
     */
    void process_automaton_locked(bool force_also) {
        check_locked();
        // notice that instead of recursion, all the calls to `rewind_machine' are
        // followed by return to this cycle!
        while (! environment->output_frozen()) {

            if (! tq.third_empty()) {
                const PlatformEvent& event = tq.peek_third();
                transition_by_key(event);
            } else {
                if ((state != st_normal) && mCurrent_time) {
                    // If this time helped to decide -> machine rewound,
                    // we have to try again, maybe the queue is not empty?.
                    if (transition_by_time(mCurrent_time))
                        continue;
                }

                if (force_also && (state != st_normal)) {
                    transition_by_force();
                    // continue;
                } else {
                    break;
                }
            }
        }
    }


   /* Return the keycode into which CODE has forked _last_ time.
   Returns code itself, if not forked. */
    [[nodiscard]] bool
    key_forked(Keycode code) const {
        return (forkActive[code] != code && forkActive[code]!= KEYCODE_UNUSED);
    }


    // can modify the event!
    void relay_event(const PlatformEvent& event) {
        // we must guarantee ORDER
        environment->relay_event(event);
    }

#ifndef DISABLE_STD_LIBRARY
    [[nodiscard]] inline std::optional<PlatformEvent> pop_event_if_present_locked() {
        if (tq.can_pop()) {
            PlatformEvent ev = tq.head();
            save_event_to_log(ev);
            tq.pop();
            return ev;
        }
        return std::nullopt;
    }

    [[nodiscard]] inline std::optional<PlatformEvent> pop_event_if_present() {
        unique_lock lock(mLock);
        return pop_event_if_present_locked();
    }
#else
    // same body but different result transfer:
    bool pop_event_if_present_locked(PlatformEvent& out_event) {
        if (tq.can_pop()) {
            out_event = tq.head();
            save_event_to_log(out_event);
            tq.pop();
            return true;
        }
        return false;
    }

    bool pop_event_if_present(PlatformEvent& out_event) {
        unique_lock lock(mLock);
        return pop_event_if_present_locked(out_event);
    }
#endif

    /**
     * Push as many as possible from the OUTPUT queue to the next layer.
     * Also the time.
     * Beware: not locked.
     **/
    void flush_to_next_locked() {
        while (!environment->output_frozen()) {
#ifndef DISABLE_STD_LIBRARY
            auto event = pop_event_if_present_locked();
            if (event.has_value()) {
                relay_event(*event);
            } else {
                break;
            }
#else
            PlatformEvent event;
            if (pop_event_if_present_locked(event)) {
                relay_event(event);
            } else {
                break;
            }
#endif
        }
        if (!environment->output_frozen()) {
            Time now = push_time_to_next_locked();
            if (now) {
                environment->push_time(now);
            }
        }
    }

    void flush_to_next() {
        unique_lock lock(mLock);
        flush_to_next_locked();
    }

    [[nodiscard]] Time push_time_to_next_locked() {
        const PlatformEvent *item = tq.first();
        if (item == nullptr) {
            return mCurrent_time;
        } else {
            Time now = environment->time_of(*item);
            mCurrent_time = 0;
            return now;
        }
    }

    void push_time_to_next() {
        // send out time:

        // interesting: after handing over, the nextPlugin might need to be refreshed.
        // if that plugin is gone. todo!

        Time now;
        {
            unique_lock lock(mLock);
            now = push_time_to_next_locked();
        }

        if (now) {
            // this can thaw, freeze,?
            environment->push_time(now);
        }
    }

private:
    // Internal helper when lock is already held
    [[nodiscard]] Time next_decision_time_locked() const {
        check_locked();
        if ((state == st_verify)
            || (state == st_suspect))
            return mDecision_time;
        else
            return 0;
    }

public:
    [[nodiscard]] Time next_decision_time() const {
        unique_lock lock(mLock);
        return next_decision_time_locked();
    }

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
        unique_lock lock(mLock);
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
            mdb("%s: invalid type %d\n", __func__, type);
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
        unique_lock lock(mLock);
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
    }

    /** ask the platform environment to send events as data. */
    int dump_last_events_to_client(event_publisher<archived_event_t>* publisher, int max_requested) {
        // I don't need to count them! last_events_count
        // should be locked
        unique_lock lock(mLock);
        int queue_count = last_events_log.size();

        if (max_requested > queue_count) {
            max_requested = queue_count;
        };

        publisher->prepare(max_requested);
#ifndef DISABLE_STD_LIBRARY
        std::function<void(const archived_event_t&)> lambda =
            [publisher](const archived_event_t& ev){ publisher->event(ev); };

        std::for_each(last_events_log.begin(),
                      last_events_log.end(),
                      lambda);
#else
        for (const auto& ev : last_events_log) {
            publisher->event(ev);
        }
#endif
        mdb("sending %d events\n", max_requested);

        return publisher->commit();
    };

public:
    /** Create 2 configuration sets:
        0. w/o forking,  no-op.
        1. user-configurable
        this is on loading, so should not use Abort allocation policy:

        @return false if allocation  failed.
    */
    bool create_configs() {
        unique_lock lock(mLock);

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

        } catch (const std::bad_alloc &exc) {
            return false;
        }
#else
        return false;
#endif // KERNEL
    }

    void dump_last_events(event_dumper<archived_event_t>* dumper) const {
        unique_lock lock(mLock);
#ifndef DISABLE_STD_LIBRARY
        std::function<void(const archived_event_t&)> lambda = [dumper](const archived_event_t& ev){ dumper->operator()(ev); };
        if (last_events_log.full()) {
            std::for_each(last_events_log.begin(),
                          last_events_log.end(),
                          lambda);
        } else {
            std::for_each(last_events_log.begin(),
                          last_events_log.begin() + last_events_log.size(),
                          lambda);
        }
#else
        for (const auto& ev : last_events_log) {
            (*dumper)(ev);
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

    void log_queues(const char *message) { // const
        tq.log_queues(message);
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

        const Keycode key = environment->detail_of(pevent);
        {
            unique_lock lock(mLock);
            if (mStopped) {
                return 0;
            }
            mdb("%s: event %u (%s) time: %" TIME_FMT "\n",
                __func__,
                environment->detail_of(pevent),
                environment->press_p(pevent)?"press":"release",
                environment->time_of(pevent));

            if (mCurrent_time > environment->time_of(pevent)) {
                mdb("%s: bug: time moved backwards!\n", __func__);
            }

            mCurrent_time = 0;

            if (key > MAX_KEYCODE) {
                mdb("%s: out-of-bound event %d\n", __func__);
                return 0;
            }

            if (environment->press_p(pevent)
                && key_forked(key))
            {
                mdb("%s: skipping this Press -- it's a forked modifier and AR!\n", __func__);
            } else {
                tq.push(pevent);
            }

            process_automaton_locked(false);
        }

        flush_to_next();

        mCurrent_time = 0;

        if (key > MAX_KEYCODE) {
            mdb("%s: out-of-bound event %d\n", __func__);
            return 0;
        }

        if (environment->press_p(pevent)
            && key_forked(key))
        {
            mdb("%s: skipping this Press -- it's a forked modifier and AR!\n", __func__);
        } else {
            tq.push(pevent);
        }

        process_automaton_locked(false);

        return next_decision_time_locked();
    }


    Time accept_time(const Time now) {
        {
            unique_lock lock(mLock);
            if (mStopped) {
                return 0;
            }
            /* push the time ! */
            if (mCurrent_time > now) {
                environment->log("%s: bug: time moved backwards!\n", __func__);
                return next_decision_time_locked();
            }
            else
                mCurrent_time = now;

            process_automaton_locked(false);
        }
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
    void accept_confirmation() {
        {
            // fixme: but this is possibly non-reentrant.
            // std::atomic<uint32_t> pending_{0};
            unique_lock lock(mLock);
            process_automaton_locked(true);
        }
        flush_to_next();
    }

};

}


// extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
