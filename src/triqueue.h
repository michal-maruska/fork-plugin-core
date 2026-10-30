#pragma once

// This code is supposed to have minimum dependencies.
// It can be compiled for Kernel environment, where C++ std-lib is absent.
// this limits: exceptions, memory allocation, iterators

// todo: move to common
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x)   (void)(x)
#endif

template <typename item_t, template <typename> typename CircularBuffer>
class triqueue_t {
    using circular_buffer_t = CircularBuffer<item_t>;
    using iterator = typename circular_buffer_t::iterator;
    using LogFn = void (*)(const char* fmt, ...);
public:
    // inline static const Environment_t *env = nullptr;
#include "scope-logger.h"
private:

    circular_buffer_t output_buffer;
    circular_buffer_t internal_buffer;
    iterator end_internal;


  private:
    LogFn log_ = nullptr;

void trace(const char* fmt, ...) {
#ifdef FORK_DEBUG
        if (log_) { va_list ap; va_start(ap, fmt); /* call log_ */ va_end(ap); }
#endif
    }



public:
    // for gdb
    void log_queues(const char* msg) { // const -> mysterious operator- miss
        if (log_ == nullptr)
            return;
#if 0
        trace("(%s) %s: %lu\n", __func__, msg, buffer.size());


        trace("%s: %lu %lu %lu %lu\n", msg,
                 buffer.begin().pos_,
                 end_output.pos_,
                 end_internal.pos_,
                 buffer.end().pos_);
#endif
        trace("%s: %lu [%lu %lu %lu]\n", msg,
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
            trace("%hhx ", as_string[i]);

            if ( 0 ==  (++i % per_line))
                trace("\n");
        }
        trace("\n");
#endif
        trace(message, item);
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
        // log_queues(__func__);
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
        trace("%s: %p\n", __func__, &tmp);
#endif
        return tmp;
    }

    item_t& head() {
        item_t& tmp = output_buffer.empty() ? internal_buffer.front() : output_buffer.front();
#if DEBUG
        trace("%s: %p\n", __func__, &tmp);
#endif
        return tmp;
    }

    // drop item from 2nd part. It must be there.
    void drop_from_second() {
        internal_buffer.pop_front();
        if (middle_empty()) {
            trace("%s: BUG\n", __func__);
            return;
        }
        --end_internal;
    }

    void move_to_first() {
        scope_queue_logger QL(this, __func__);
        if (middle_empty()) {
#if DEBUG
            trace("%s: BUG\n", __func__);
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
//            trace("%s: BUG\n", __func__);
// bug:           abort();
            return;
        }

        ++end_internal;
    }

    //
    void drop_from_third() {
        //
        // if (end_internal + 1 == internal_buffer.end()) {
        internal_buffer.pop_end();
        // } else {
        //     trace("%s: BUG\n", __func__);
        // }

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

