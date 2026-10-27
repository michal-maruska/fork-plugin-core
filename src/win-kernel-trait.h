#pragma once

#include "kernelspace.h"

/**
 * Windows kernel-space filter driver policies / traits.
 *
 * In a Windows kernel keyboard filter driver (e.g. upper filter above kbdclass),
 * keyboard input packets are delivered sequentially per device stack.
 * Because callback processing is already serialized by the driver framework
 * (typically at DISPATCH_LEVEL where blocking mutexes cannot be used),
 * locking operations are no-ops and a non-atomic boolean flag is sufficient.
 */
using WinKernelPolicies = KernelspacePolicies;
using WinKernelTrait = WinKernelPolicies;
