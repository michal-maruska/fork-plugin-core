#pragma once

#ifndef KERNEL
#include <vector>
#endif

extern "C" {
#include "stddef.h"
}


#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x)   (void)(x)
#endif

// let's make one without any:
template <typename event>
class empty_last_events_t
#ifndef KERNEL
  : std::vector<event>
#endif
{
public:
    // override:
    static
    void push_back(const event& __x) {}

    // emplace_back()
    static
    void set_capacity(const int& n) {
      UNREFERENCED_PARAMETER(n);
    };

    static
    size_t size() {
        return 0;
    }

  // bool full() const {return false;}
};
