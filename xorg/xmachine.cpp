#include "config.h"

#include "xmachine.h"
// This is the price of not including machine.cpp queueu.h in fork-plugin.cpp


namespace forkNS {
    // must be in sync.
    template class forkingMachine<XOrgEnvironment,
                                  CircularArchive<ForkInfo, archived_event>
                                  >;
}
