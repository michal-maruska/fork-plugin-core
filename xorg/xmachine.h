#pragma once

#include "machine.h"
// KeyCode and Time:
extern "C"
{
    // We are inside server:
#include <xorg-server.h>

// todo: archived_event coud be avoided ... it's X specific
// plugin should know how to translate/send them away.l
#include "fork_requests.h"
#undef max
#undef min

    // the types:
#include <X11/X.h>
#undef max
#undef min
}

using machineRec = forkNS::forkingMachine<KeyCode, Time, archived_event>;
