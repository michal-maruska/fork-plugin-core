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

    circular_buffer_t buffer;
    iterator end_output;
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
                 buffer.size(),
                 end_output - buffer.begin(),
                 end_internal - end_output,
                 buffer.end() - end_internal);
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
        explicit triqueue_t(int capacity) : buffer(circular_buffer_t(capacity)),
                                            end_output(buffer.begin()),
                                            end_internal(buffer.begin())
        {
            log_queues(__func__);
        };

    // Queries
    // int length();

    bool empty() {
        return (buffer.empty());
    }

    bool middle_empty() {
        return (end_internal == end_output);
    }

    // so empty would be   end_output = 0; end_internal = 0; buffer.end() ... lenght ....0 or 1?
    bool third_empty() {
        // log_queues(__func__);
        return end_internal == buffer.end();
    }

    void rewind_middle() {
        scope_queue_logger QL(this, __func__);
        end_internal = end_output;
    }

    // modifiers:
    void push(const item_t &item) {
#if 0
        env->log("%s: %lu\n", __func__, sizeof(item));
        dump_item(__func__, item);
#endif
        buffer.push_back(item);

#if DEBUG
        // dump_item(input_queue.front());
        log_queues("post-push");
#endif
#if 0
        peek_third();
#endif
    }

    bool can_pop() {
        return buffer.begin() != end_output;
    }

    item_t pop() {
        scope_queue_logger QL(this, __func__);

        item_t item = buffer.front();

        buffer.pop_front();
        // buffer.increment_head();
        // these are relative? yes. indices.
        end_output-=1;
        end_internal-=1;
#if DEBUG
        dump_item(__func__, item);
#endif
        return item;
    }

    const item_t& peek_third() {
        const item_t& tmp = *(end_internal); // +1
        // env->log("%s: %p\n", __func__, &tmp);
#if DEBUG
        dump_item(__func__, tmp);
#endif
        return tmp;
    }

    item_t& peek_middle() {
        item_t& tmp = *(end_output);
#if DEBUG
        trace("%s: %p\n", __func__, &tmp);
#endif
        return tmp;
    }


    // rewritable!
    item_t& head() {
        // why?
        // fixme: not pop. peek
        item_t& tmp = buffer.front(); // *(end_output);
#if DEBUG
        trace("%s: %p\n", __func__, &tmp);
#endif
        return tmp;
    }

    void move_to_first() {
        scope_queue_logger QL(this, __func__);
        if (middle_empty()) {
#if DEBUG
            trace("%s: BUG\n", __func__);
#endif
            return;
        }

        ++end_output;
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
        buffer.pop_end();
        // } else {
        //     trace("%s: BUG\n", __func__);
        // }

    }


    const item_t* first() {
        if (buffer.empty()) {
            return nullptr;
        } else {
            return &(buffer.front());
        }
    }
};

