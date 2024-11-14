/* I use it only to print out the keysym in debugging stuff*/
#pragma once

extern "C" {
    // this must precede!
#include <xorg-server.h>
#include <xorg/eventstr.h>
#undef max
#undef min
}

bool release_p(const InternalEvent* event);
bool press_p(const InternalEvent* event);
Time time_of(const InternalEvent* event);
KeyCode detail_of(const InternalEvent* event);
