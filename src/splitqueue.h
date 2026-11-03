#pragma once
#include "scope-logger.h"
#include "config.h"

#include "fork_base.h"
// This code is supposed to have minimum dependencies.
// It can be compiled for Kernel environment, where C++ std-lib is absent.
// this limits: exceptions, memory allocation, iterators
#include <stdarg.h>
// todo: move to common
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x)   (void)(x)
#endif

template <typename item_t,
          template <typename> typename CircularBuffer>
class SplitQueue {
public:
    using LogFn = FunctionRef<int(const char*, va_list)>; // int
private:
    using circular_buffer_t = CircularBuffer<item_t>;
    using iterator = typename circular_buffer_t::iterator;

    circular_buffer_t buffer;
    iterator split;
    LogFn log_;
public:


#define FORK_DEBUG 1

    void trace(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
#ifdef FORK_DEBUG
      if (log_) {
        va_list ap;
        va_start(ap, fmt);
        /* call log_ */
        log_(fmt, ap);
        va_end(ap);
      }
#endif
    }

  public:
    // for gdb
    void log_queues(const char* msg) { // const -> mysterious operator- miss
        if (! log_)
            return;

        trace("%s: %lu [%lu | %lu]\n", msg,
                 buffer.size(),
                 split - buffer.begin(),
                 buffer.end() - split);
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
        trace("%s\n", message);
    }
#endif

public:
    explicit SplitQueue(int capacity) : SplitQueue(capacity, LogFn{}) {};

    explicit SplitQueue(int capacity, LogFn fn) : buffer(circular_buffer_t(capacity)),
                                                  split(buffer.begin()),
                                                  log_(fn) {
        log_queues(__func__);
    };

    // Queries

    // not used! would need 2 locks
    bool empty() {
        return buffer.empty();
    }

    bool front_empty() {
        return split == buffer.begin();
    }

    bool back_empty() {
        log_queues(__func__);
        return split == buffer.end();
    }

    void rewind() {
        ScopeLogger QL(this, __func__);
        split = buffer.begin();
    }

    // modifiers:
    void push(const item_t &item) {
        buffer.push_back(item);
#if DEBUG
        log_queues("post-push");
#endif
    }

    const item_t& peek_back() {
        const item_t& tmp = *(split);
#if DEBUG
        dump_item(__func__, tmp);
#endif
        return tmp;
    }

    item_t& peek_front() {
        item_t& tmp = buffer.front();
#if DEBUG
        trace("%s: %p\n", __func__, &tmp);
#endif
        return tmp;
    }

    item_t pop() {
        ScopeLogger QL(this, __func__);

        log_queues("pre-pop");
        item_t item = buffer.front();
        buffer.pop_front();
        split-=1;
#if DEBUG
        dump_item(__func__, item);
#endif
        log_queues("post-pop");
        return item;
    }



    void move_to_front() {
        ScopeLogger QL(this, __func__);
        if (back_empty()) {
            trace("%s: BUG\n", __func__);
// bug:           abort();
            return;
        }
        ++split;
        log_queues("post-move-to-front");
    }

    //
    void drop_from_back() {
        //
        // if (split + 1 == buffer.end()) {
        buffer.pop_end();
        // } else {
        //     trace("%s: BUG\n", __func__);
        // }

    }

};

