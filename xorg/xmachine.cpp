#include "config.h"
#include "xmachine.h"
// This is the price of not including machine.cpp queueu.h in fork-plugin.cpp

extern "C" {
// todo: archived_event coud be avoided ... it's X specific
#include "fork_requests.h"
#undef max
#undef min
}


namespace forkNS {
    // must be in sync.
    template class forkNS::forkingMachine<XOrgEnvironment,
                                          CircularArchive<ForkInfo, archived_event>
                                          >;
}
