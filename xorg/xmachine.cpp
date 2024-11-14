#include "config.h"
#include "xmachine.h"
#include "machine.cpp"
#include "queue.h"

extern "C" {
// todo: archived_event coud be avoided ... it's X specific
#include "fork_requests.h"
#undef max
#undef min
}


// template Environment_t* forkingMachine<KeyCode, Time, archived_event>::key_event::env;

namespace forkNS {

template void forkNS::reverse_splice(my_queue<Time> & pre, my_queue<Time> &post);

// explicit instantation:

// public api:
template Time forkingMachine<KeyCode, Time, archived_event>::accept_event(std::unique_ptr<PlatformEvent> pevent);
template Time forkingMachine<KeyCode, Time, archived_event>::accept_time(const Time);

#if MULTIPLE_CONFIGURATIONS
template void forkingMachine<KeyCode, Time, archived_event>::switch_config(int);
#endif
template bool forkingMachine<KeyCode, Time, archived_event>::create_configs();


template int forkingMachine<KeyCode, Time, archived_event>::configure_key(int type, KeyCode key, int value, bool set);

template int forkingMachine<KeyCode, Time, archived_event>::configure_global(int type, int value, bool set);

template int forkingMachine<KeyCode, Time, archived_event>::configure_twins(int type, KeyCode key, KeyCode twin, int value, bool set);

template int forkingMachine<KeyCode, Time, archived_event>::dump_last_events_to_client(event_publisher<archived_event>* publisher, int max_requested);

}
