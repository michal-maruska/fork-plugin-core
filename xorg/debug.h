#pragma once

#include "config.h"

#if DEBUG

/*
 * Logging for code which does not yet have the `machine'.
 * X specific.
 */

extern "C" {
#include <xorg-server.h>
#include <xorg/os.h>

#undef max
#undef min
}

// #ifndef NDEBUG
// #define DEBUG 1

# define DB(fmt, ...)     ErrorF(fmt, ##__VA_ARGS__)
# else  /* DEBUG */
#define DB(fmt, ...)  do { ; } while (0)
#endif /* DEBUG */
