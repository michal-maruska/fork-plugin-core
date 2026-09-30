// #include <boost/circular_buffer.hpp>

#include <algorithm>

#include "fork_base.h"
// archive_entry ...
// Fill is a function-method of the Environment
// Dumper ....writes out (somehow) info from the *Rec.

// Both parts must be default-constructible and copy-assignable (Part1), because of push_back() and v.first = p1.

#include "circular.h"


template <typename Part1, typename Part2>
class CircularArchive {
public:
    // what KeyProcessor's static_assert checks
    using first_type  = Part1;
    using second_type = Part2;
    using value_type  = archive_entry<Part1, Part2>;

    explicit CircularArchive(size_t capacity = 100) : buf_(capacity) {}

    void set_capacity(size_t n) {
        // sorry!
        // buf_.rset_capacity(n);
    }

    // fill(PlatformArchive&) runs only if this archive keeps the event
    template <typename Fill>
    void record(const Part1& p1, Fill&& fill) {
        if (buf_.capacity() == 0) return;

        // buf_.push_back();                  // default-constructed slot, overwrites the oldest when full
        // value_type& v = buf_.back();
        value_type v;
        v.first = p1;
        fill(v.second);
        buf_.push_back(v);
    }

    template <typename Dumper>                          // newest first
    void for_each_recent(Dumper&& f) const {
        size_t limit = 100;
        size_t n = std::min(limit, buf_.size());
        auto it = buf_.begin(); // rbegin
        for (size_t i = 0; i < n; ++i, ++it) f(*it);
    }

private:
    // boost::circular_buffer<value_type>
    circular_buffer<value_type> buf_;
};


// user code:
// archive_.record(forked, reason,
//    [&](typename Environment::PlatformArchive& ae) { env_.archive_event(ae, raw_ev); });
