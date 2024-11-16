#pragma once

#include "queue.h"

template <typename item_t, typename Environment_t>
class triqueue_t {
    // interface:
    typedef forkNS::my_queue<item_t> list_with_tail;

public:
    inline static const Environment_t *env = nullptr;
private:
    list_with_tail internal_queue;
    /* Still undecided events: these events alone don't decide
       how to interpret the first event on the queue.*/
    list_with_tail input_queue; /* Not yet processed at all. Since we wait for external
                  * events to resume processing (Grab is active-frozen) */
    list_with_tail output_queue; /* We have decided, so possibly modified events (forked),
                   * Externals don't accept, so we keep them. */

// internal_queue
// empty


// output_queue.push
//

// input_queue
//    length  empty
//  pop push

    void log_queues(const char* msg) {
        env->log("%s: %lu %lu %lu\n", msg, output_queue.size(),
                 internal_queue.size(), input_queue.size());
    }
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
    triqueue_t(int capacity) : internal_queue("internal"),
                               input_queue("input_queue"),
                               output_queue("output_queue")
        {};

    // Queries
    // int length();

    bool empty() {
        return (output_queue.empty() && input_queue.empty() && internal_queue.empty());
    }

    bool middle_empty() {
        return (input_queue.empty() && internal_queue.empty());
    }

    bool third_empty() {
        return input_queue.empty();
    }

    void rewind_middle() {
        log_queues(__func__);
        reverse_splice(internal_queue, input_queue);
        log_queues("post");
    }

    // modifiers:
    void push(const item_t &i) {
        env->log("%s: %lu\n", __func__, sizeof(i));

        dump_item(i);
        input_queue.push(i);

        env->log("double check\n");
        dump_item(input_queue.front());
        log_queues(__func__);
    }

    bool can_pop() {
        return ! output_queue.empty();
    }

    item_t pop() {
        log_queues(__func__);
        item_t item = output_queue.pop();
        dump_item(item);
        return item;
    }

    const item_t& peek_third() {
        // fixme: not pop. peek
        return input_queue.front();
    }
    // assert(!internal_queue.empty());


    // rewritable!
    item_t& rewrite_head() {
        // fixme: not pop. peek
        return internal_queue.front();
    }

    // insert
    // push();
    // tq.shift_to_first();
    void move_to_first() {
        log_queues(__func__);
        output_queue.push(internal_queue.pop());
        log_queues(" post");
    }
    void move_to_second() {
        internal_queue.push(input_queue.pop());
    }

    // internal_queue, input_queue
    const item_t* first() {
        if (!output_queue.empty()) {
            return &output_queue.front();
        } else if (!internal_queue.empty()) {
            return &internal_queue.front();
        } else if (!input_queue.empty()) {
            return &input_queue.front();
        } else {
            return nullptr;
        }
    }
};

