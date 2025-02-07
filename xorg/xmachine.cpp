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
    template class forkingMachine<KeyCode, Time, XorgEvent, XOrgEnvironment, archived_event, last_events_t>;
}
