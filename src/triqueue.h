#pragma once

#include "queue.h"

template <typename item_t>
class triqueue_t {
    // interface:
    typedef forkNS::my_queue<item_t> list_with_tail;
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
//
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
        reverse_splice(internal_queue, input_queue);
    }


    // modifiers:
    void push(const item_t &i) {
        input_queue.push(i);
    }

    bool can_pop() {
        return ! output_queue.empty();
    }

    item_t pop() {
        return output_queue.pop();
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
        output_queue.push(internal_queue.pop());
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

