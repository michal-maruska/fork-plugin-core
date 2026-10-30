#pragma once

#include "machine.h"
#include "userspace.h"
#include "xorg.h"

#define HAS_STD_LIBRARY 1
#include "circular.h"

// #include "empty_last.h"
// using PlatformArchive = archived_event;
using machineRec = forkNS::forkingMachine<XOrgEnvironment,
                                          UserspaceTrait,
                                          circular_buffer
                                          >;

// using last_events_t = empty_last_events_t<archived_event>;
extern template class forkNS::forkingMachine<XOrgEnvironment,
                                             UserspaceTrait,
                                             circular_buffer
                                             >;
