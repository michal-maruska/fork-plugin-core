#pragma once

/**
 * Created by michal on 8/10/24.
 *
 * The interface to the platforms: fork-machine should use this
 *
 */

#include <string>
#include <memory>

namespace forkNS {


template <typename archived_fork_event>
class event_dumper {
    public:
    virtual void operator() (const archived_fork_event& event) = 0;
    virtual ~event_dumper() {};
};

template <typename archived_fork_event>
class event_publisher {
    public:
    virtual void prepare(int max_events) = 0;
    virtual int commit() = 0;
    virtual void event(const archived_fork_event& event) = 0;
    virtual ~event_publisher() {};
};


class PlatformEvent {};

// fork-machine passes its parameters to this:
template <typename Keycode, typename Time, typename archived_fork_event>
class platformEnvironment {
public:
    platformEnvironment() = default;

    virtual bool press_p(const PlatformEvent& event) = 0;
    virtual bool release_p(const PlatformEvent& event) = 0;
    // fixme:
    virtual Time time_of(const PlatformEvent& event) = 0;
    virtual Keycode detail_of(const PlatformEvent& event) = 0;

    virtual bool ignore_event(const PlatformEvent& pevent) = 0;

    virtual bool output_frozen() = 0;
    virtual void relay_event(const PlatformEvent &pevent) = 0; // very important to pass-by-ref
    virtual void push_time(Time now) = 0;

    virtual void log(const char* format...) const = 0;
    virtual void vlog(const char* format, va_list argptr) const = 0;
    virtual std::string fmt_event(const PlatformEvent& pevent) = 0;

    virtual void archive_event(archived_fork_event& ae, const PlatformEvent& event) = 0;
    virtual void free_event(PlatformEvent* pevent) const = 0; // not reference?
    virtual void rewrite_event(PlatformEvent& pevent, Keycode code) = 0;

    // factory:
    virtual std::unique_ptr<event_dumper<archived_fork_event>> get_event_dumper() = 0;

    virtual ~platformEnvironment() = default;
};
}
