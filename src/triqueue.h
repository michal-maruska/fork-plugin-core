#pragma once

#include "circular.h"
#include <cstdlib>

#ifndef KERNEL
// std::allocator
#include <memory>
#else
#include "my-memory.h"
#endif

// todo: move to common
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x)   (void)(x)
#endif

template <typename item_t, typename Environment_t>
class triqueue_t {

    using circular_buffer_t = circular_buffer<item_t, false,
// fixme: this should be a template parameter!
#ifndef KERNEL
                                              std::allocator<item_t>
#else
                                              kernelAllocator<item_t>
#endif
                                              >;
    using iterator = typename circular_buffer_t::iterator;

public:
    inline static const Environment_t *env = nullptr;
private:

    circular_buffer_t output_buffer;
    circular_buffer_t internal_buffer;
    iterator end_internal;

// todo: elsewhere?
    struct scope_queue_logger {
        triqueue_t* parent;
        scope_queue_logger(triqueue_t* parent, const char* msg) : parent(parent) {
#if DEBUG
            parent->log_queues(msg);
#else
            UNREFERENCED_PARAMETER(msg);
#endif
        }
        ~scope_queue_logger() {
#if DEBUG
            parent->log_queues(" post");
#endif
        }
    };

public:
    // for gdb
    void log_queues(const char* msg) { // const -> mysterious operator- miss
        if (env == nullptr)
            return;
#if 0
        env->log("(%s) %s: %lu\n", __func__, msg, buffer.size());


        env->log("%s: %lu %lu %lu %lu\n", msg,
                 buffer.begin().pos_,
                 end_output.pos_,
                 end_internal.pos_,
                 buffer.end().pos_);
#endif
        env->log("%s: %lu [%lu %lu %lu]\n", msg,
                 output_buffer.size() + internal_buffer.size(),
                 output_buffer.size(),
                 end_internal - internal_buffer.begin(),
                 internal_buffer.end() - end_internal);
    };

#if DEBUG
    void dump_item(const char* message, const item_t &item) {
#if 0
        constexpr int per_line = 50;
        const char* as_string = (char*) &item;
        for (int i=0; i< per_line - 1;) { // sizeof(item)
            // hh unsigned char?
            env->log("%hhx ", as_string[i]);

            if ( 0 ==  (++i % per_line))
                env->log("\n");
        }
        env->log("\n");
#endif
        env->fmt_event(message, item);
    }
#endif

public:
        explicit triqueue_t(int capacity) : output_buffer(circular_buffer_t(capacity)),
                                            internal_buffer(circular_buffer_t(capacity)),
                                            end_internal(internal_buffer.begin())
        {
            log_queues(__func__);
        };

    // Queries

    bool empty() {
        return output_buffer.empty() && internal_buffer.empty();
    }

    bool middle_empty() {
        return end_internal == internal_buffer.begin();
    }

    bool third_empty() {
        log_queues(__func__);
        return end_internal == internal_buffer.end();
    }

    void rewind_middle() {
        scope_queue_logger QL(this, __func__);
        end_internal = internal_buffer.begin();
    }

    // modifiers:
    void push(const item_t &item) {
        internal_buffer.push_back(item);
#if DEBUG
        log_queues("post-push");
#endif
    }

    bool can_pop() {
        return !output_buffer.empty();
    }

    item_t pop() {
        scope_queue_logger QL(this, __func__);

        item_t item = output_buffer.front();
        output_buffer.pop_front();
#if DEBUG
        dump_item(__func__, item);
#endif
        return item;
    }

    const item_t& peek_third() {
        const item_t& tmp = *(end_internal);
#if DEBUG
        dump_item(__func__, tmp);
#endif
        return tmp;
    }

    item_t& peek_middle() {
        item_t& tmp = internal_buffer.front();
#if DEBUG
        env->log("%s: %p\n", __func__, &tmp);
#endif
        return tmp;
    }

    item_t& head() {
        item_t& tmp = output_buffer.empty() ? internal_buffer.front() : output_buffer.front();
#if DEBUG
        env->log("%s: %p\n", __func__, &tmp);
#endif
        return tmp;
    }

    void move_to_first() {
        scope_queue_logger QL(this, __func__);
        if (middle_empty()) {
#if DEBUG
            env->log("%s: BUG\n", __func__);
#endif
            return;
        }

        output_buffer.push_back(internal_buffer.front());
        internal_buffer.pop_front();
        --end_internal;
    }

    void move_to_second() {
        scope_queue_logger QL(this, __func__);
        if (third_empty()) {
            env->log("%s: BUG\n", __func__);
            abort();
            return;
        }

        ++end_internal;
    }

    const item_t* first() {
        if (!output_buffer.empty()) {
            return &(output_buffer.front());
        } else if (!internal_buffer.empty()) {
            return &(internal_buffer.front());
        } else {
            return nullptr;
        }
    }
};

