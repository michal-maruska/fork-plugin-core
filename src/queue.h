#pragma once

#include <ext/slist>
#include <string>

#ifdef DEBUG
#include "debug.h"
#endif

// using namespace __gnu_cxx;

namespace forkNS {
/* FIFO -- single-linked list
   + slice operation,

   Memory/ownerhip:
   the implementing container(list) handles pointers to objects.
   The objects are owned by the application!
   if a Reference is push()-ed, we make a Clone! But we never delete the objects.
   pop() returns the pointer!
*/
template <typename T>
class my_queue : public __gnu_cxx::slist<T> {
private:
    const std::string m_name;     // for debug string const char*
    // private type!
    // typename _Node *last_node;
    typename  __gnu_cxx::slist<T>::iterator m_last_node;
public:

    [[nodiscard]] const char* get_name() const {
        return m_name.c_str();
    }

    typename __gnu_cxx::slist<T>::size_type length() const {
        return this->size();
    }

    // override:
    T pop() {
#if DEBUG > 1
        DB("%s\n", __func__);
#endif
        // not thread-safe!
        T value = this->front(); // __gnu_cxx::slist<T>::
        // I cannot invoke:
        // there are no arguments to 'pop_front' that depend on a template parameter, so a declaration of 'pop_front' must be available
        this->pop_front();
        // invalidate iterators
        if (this->empty())
            m_last_node = this->begin();
        return value;
    }

    void push(T&& value) {
#if DEBUG > 1
        DB(("%s: %s: now %d + 1\n", __FUNCTION__, get_name(), length()));
#endif
        if (! this->empty()) {
            m_last_node = this->insert_after(m_last_node, value); // std::forward()
        } else {
            this->push_front(std::forward<T>(value));  // forward() didn't work!
            m_last_node = this->begin();
        }
    }

    // fixme: move-?
    void push(const T& value) {   // we clone the value!
        T clone(value);
        push(std::move(clone));
    }

    /* move the content of appendix to the END of this queue
     *      this      appendix    this        appendix
     *     xxxxxxx   yyyyy   ->   xxxxyyyy       (empty)
     */
    void append (my_queue& suffix) {
        // appendix
        if (this->empty()) { // fixme!
#ifdef DEBUG
            DB("%s: bad state\n", __func__);
#endif
            // fixme:
            return;
        }

#if DEBUG > 1
        DB(("%s: %s: appending/moving all from %s:\n",
            __func__,
            get_name(),
            suffix.get_name()));
#endif
        if (! suffix.empty()) {
            this->splice_after(m_last_node, suffix);
            m_last_node = suffix.m_last_node;
        }

#if DEBUG > 1
        DB(("%s now has %d\n", get_name(), length()));
        DB(("%s now has %d\n", suffix.get_name(), suffix.length()));
#endif
    }


    // const char* name = NULL
    explicit my_queue(std::string&& name) : m_name(std::move(name)) {
#ifdef DEBUG
        DB("%s: constructor\n", __func__);
#endif
        m_last_node = this->end();
    }

    void swap(my_queue& peer) noexcept {
        __gnu_cxx::slist<T>::swap(peer);
        // m_list.swap(peer.m_list);

        typename __gnu_cxx::slist<T>::iterator temp;
        temp = m_last_node;

        // iter_swap(last_node,peer.last_node);
        if (this->empty())
            m_last_node = this->begin();
        else {
            m_last_node = peer.m_last_node;
        }

        if (peer.empty())
            peer.m_last_node = peer.begin();
        else
            peer.m_last_node = temp;
    }
};

template <typename T>
void reverse_splice(my_queue<T> & pre, my_queue<T> &post) {
    // (pre) (post) -> () (pre post)
    if (!(pre.empty())) {
        pre.append(post);
        pre.swap(post);
    };
}
}
