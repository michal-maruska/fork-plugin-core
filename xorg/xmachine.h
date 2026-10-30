#pragma once

#include "machine.h"
#include "userspace.h"
#include "xorg.h"


// using PlatformArchive = archived_event;
using machineRec = forkNS::forkingMachine<XOrgEnvironment,
                                          UserspaceTrait
                                          >;

// using last_events_t = empty_last_events_t<archived_event>;
extern template class forkNS::forkingMachine<XOrgEnvironment,
                                             UserspaceTrait
                                          >;
