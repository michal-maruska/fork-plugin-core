#include "config.h"
#include "xmachine.h"
// This is the price of not including machine.cpp queueu.h in fork-plugin.cpp

extern "C" {
// todo: archived_event coud be avoided ... it's X specific
#include "fork_requests.h"
#undef max
#undef min
}


// template Environment_t* forkingMachine<KeyCode, Time, archived_event>::key_event::env;

namespace forkNS {


// explicit instantation:

// public api:
template Time forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>::accept_event(const XorgEvent& pevent);
template Time forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>::accept_time(const Time);

#if MULTIPLE_CONFIGURATIONS
template void forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>::switch_config(int);
#endif
template bool forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>::create_configs();


template int forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>::configure_key(int type, KeyCode key, int value, bool set);

template int forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>::configure_global(int type, int value, bool set);

template int forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>::configure_twins(int type, KeyCode key, KeyCode twin, int value, bool set);

template int forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>::dump_last_events_to_client(event_publisher<archived_event>* publisher, int max_requested);

}
