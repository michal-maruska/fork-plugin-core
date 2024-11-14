#pragma once


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
template <typename Keycode, typename Time, typename archived_event_t>
class forkingMachine {

    constexpr static const Keycode no_key = 0;

public:
    typedef boost::circular_buffer<archived_event_t> last_events_t;


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

    void run_automaton(bool force);

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
    }


    void apply_event_to_suspect(std::unique_ptr<key_event> ev);

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


    void activate_fork(fork_reason_t fork_reason);

    void
    change_state(fork_state_t new_state)
    {
        state = new_state;
        mdb(" --->%s[%dm%s%s\n", escape_sequence, 32 + new_state,
            state_description[new_state], color_reset);
    }

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
    }

    void apply_event_to_verify_state(std::unique_ptr<key_event> ev);


    Time key_pressed_too_long(Time current_time);
    Time verifier_decision_time(Time current_time);

   /* Return the keycode into which CODE has forked _last_ time.
   Returns code itself, if not forked. */
    [[nodiscard]] bool
    key_forked(Keycode code) const {
        return (forkActive[code]);
    }

    void apply_event_to_normal(std::unique_ptr<key_event> ev);

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

    int configure_global(int type, int value, bool set);
    int configure_twins(int type, Keycode key, Keycode twin, int value, bool set);
    int configure_key(int type, Keycode key, int value, bool set);

    int dump_last_events_to_client(event_publisher<archived_event_t>* publisher, int max_requested);

    // This is a public api!
    Time accept_event(std::unique_ptr<PlatformEvent> pevent);

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

private:
    void transition_by_key(std::unique_ptr<key_event> ev);

    bool transition_by_time(Time current_time);

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
};

}


// extern int dump_last_events_to_client(PluginInstance* plugin, ClientPtr client, int n);
