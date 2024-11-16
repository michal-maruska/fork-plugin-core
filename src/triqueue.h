#pragma once

#include "circular.h"
#include <memory>

template <typename item_t, typename Environment_t>
class triqueue_t {

    using circular_buffer_t = circular_buffer<item_t, false, std::allocator<item_t>>;
    using iterator = typename circular_buffer_t::iterator;

public:
    inline static const Environment_t *env = nullptr;
private:

    circular_buffer_t buffer;
    iterator end_output;
    iterator end_internal;


    void log_queues(const char* msg) {
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
        env->log("%s(%s): %lu %lu %lu\n", msg, __func__,
                 end_output - buffer.begin(),
                 end_internal - end_output,
                 buffer.end() - end_internal);
    };

    void dump_item(const item_t &item) {
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
        env->fmt_event(__func__, item);
    }

public:
        triqueue_t(int capacity) : buffer(circular_buffer_t(capacity)),
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

    bool third_empty() {
        return end_internal == buffer.end();
    }

    void rewind_middle() {
        log_queues(__func__);
        end_internal = end_output;
        log_queues("post");
    }

    // modifiers:
    void push(const item_t &item) {
#if 0
        env->log("%s: %lu\n", __func__, sizeof(item));
        dump_item(item);
#endif
        buffer.push_back(item);

        // dump_item(input_queue.front());
        log_queues("post-push");
#if 0
        peek_third();
#endif
    }

    bool can_pop() {
        return buffer.begin() != end_output;
    }

    item_t pop() {
        log_queues(__func__);

        item_t& item = buffer.front();

        buffer.pop_front();
        end_internal-=1;
        end_output-=1;

        dump_item(item);
        log_queues("post");
        return item;
    }

    const item_t& peek_third() {
        const item_t& tmp = *(end_internal); // +1
        // env->log("%s: %p\n", __func__, &tmp);
        dump_item(tmp);
        return tmp;
    }

    // rewritable!
    item_t& rewrite_head() {
        // fixme: not pop. peek
        item_t& tmp = *(end_output); // +1
        env->log("%s: %p\n", __func__, &tmp);
        return tmp;
    }

    void move_to_first() {
        log_queues(__func__);
        end_output++;
        log_queues(" post");
    }

    void move_to_second() {
        log_queues(__func__);
        end_internal++;
        log_queues(" post");
    }

    const item_t* first() {
        if (buffer.empty()) {
            return nullptr;
        } else {
            return &(buffer.front());
        }
    }
};

