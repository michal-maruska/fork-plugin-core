#pragma once

#include "config.h"

template <typename Mutex>
class empty_unique_lock
{
public:
    explicit empty_unique_lock(const Mutex& m) {
        UNUSED(m);
    }

    ~empty_unique_lock() = default;

    empty_unique_lock(const empty_unique_lock&) = delete;
    empty_unique_lock& operator=(const empty_unique_lock&) = delete;

    empty_unique_lock(empty_unique_lock&&) noexcept = default;
    empty_unique_lock& operator=(empty_unique_lock&&) noexcept = default;
};
