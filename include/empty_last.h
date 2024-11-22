#pragma once

#include "vector"


// let's make one without any:
template <typename event>
// public: I need the begin(), end()
class empty_last_events_t : public std::vector<event>
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
};
