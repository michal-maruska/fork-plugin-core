#pragma once

// #include "vector"

#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x)   (void)(x)
#endif

// let's make one without any:
template <typename event>
class empty_last_events_t
#ifndef KERNEL
  : public std::vector<event>
#endif
{
public:
    // override:
    void push_back(const event& __x) {}

    // emplace_back()
    void set_capacity(const int& n) {
      UNREFERENCED_PARAMETER(n);
    };

    size_t size() const {
        return 0;
    }

    bool full() const {return false;}

#if 0
  // public: I need the begin(), end()
   begin()
   end()
#endif
};
