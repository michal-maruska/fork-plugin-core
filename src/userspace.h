#pragma once

#include <mutex>
#include <atomic>

struct UserspacePolicies {
    using mutex_t     = std::mutex;
    using unique_lock = std::unique_lock<std::mutex>;
    using atomic_bool = std::atomic<bool>;
};

using UserspaceTrait = UserspacePolicies;
