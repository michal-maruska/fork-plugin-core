#pragma once

/**
 * Interface/concept for platform environments used by forkingMachine.
 */

#include <cstdarg>

#if defined(__cpp_concepts) && __cpp_concepts >= 201907
#include <concepts>
#define USE_CONCEPTS 1
#endif

namespace forkNS {

#if USE_CONCEPTS
template <typename Env>
concept EnvironmentConcept = requires(
    Env& env,
    const typename Env::PlatformEvent& const_ev,
    typename Env::PlatformEvent& ev,
    typename Env::PlatformEvent* ev_ptr,
    typename Env::PlatformArchive& ae,
    typename Env::Keycode kc,
    typename Env::Time now,
    const char* fmt,
    va_list va
) {
    typename Env::PlatformEvent;
    typename Env::PlatformArchive;
    typename Env::Keycode;
    typename Env::Time;

    { env.detail_of(const_ev) }        -> std::same_as<typename Env::Keycode>;
    { env.time_of(const_ev) }          -> std::same_as<typename Env::Time>;
    { env.press_p(const_ev) }          -> std::convertible_to<bool>;
    { env.release_p(const_ev) }        -> std::convertible_to<bool>;
    { env.ignore_event(const_ev) }     -> std::convertible_to<bool>;
    { env.output_frozen() }            -> std::convertible_to<bool>;
    { env.relay_event(const_ev) }      -> std::same_as<void>;
    { env.push_time(now) }             -> std::same_as<void>;
    { env.archive_event(ae, const_ev) } -> std::same_as<void>;
    { env.free_event(ev_ptr) }         -> std::same_as<void>;
    { env.rewrite_event(ev, kc) }      -> std::same_as<void>;
    { env.log(fmt) }                   -> std::same_as<void>;
    { env.vlog(fmt, va) }              -> std::same_as<void>;
    { env.fmt_event(fmt, const_ev) }   -> std::same_as<void>;
};
#endif

} // namespace forkNS
