#pragma once

#include "empty_unique_lock.h"

struct empty_mutex_t {
    void lock() {}
    void unlock() {}
    bool try_lock() { return true; }
};

struct empty_atomic_bool {
    bool val_{false};

    constexpr empty_atomic_bool() = default;
    constexpr empty_atomic_bool(bool v) : val_(v) {}

    bool compare_exchange_strong(bool& expected, bool desired) noexcept {
        if (val_ == expected) {
            val_ = desired;
            return true;
        }
        expected = val_;
        return false;
    }

    template <typename... Args>
    bool compare_exchange_strong(bool& expected, bool desired, Args...) noexcept {
        return compare_exchange_strong(expected, desired);
    }

    void store(bool desired) noexcept {
        val_ = desired;
    }

    template <typename Order>
    void store(bool desired, Order) noexcept {
        val_ = desired;
    }

    bool load() const noexcept {
        return val_;
    }

    template <typename Order>
    bool load(Order) const noexcept {
        return val_;
    }

    operator bool() const noexcept {
        return val_;
    }
};

struct KernelspacePolicies {
    using mutex_t     = empty_mutex_t;
    using unique_lock = empty_unique_lock<empty_mutex_t>;
    using atomic_bool = empty_atomic_bool;
};

using KernelspaceTrait = KernelspacePolicies;
