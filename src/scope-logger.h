#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x)   (void)(x)
#endif

// use CTAD
// ScopeLogger ql(this, __func__);
template <typename T>
struct ScopeLogger {
    T* obj_;
    const char* name_;

    ScopeLogger(T* obj, const char* name) : obj_(obj), name_(name) {
        obj_->trace(">> %s\n", name_);
    }
    ~ScopeLogger() { obj_->trace("<< %s\n", name_); }
};



#if 0

// log_queues on c-tor & d-tor.
struct scope_queue_logger {
    triqueue_t* parent;

public:
    scope_queue_logger(triqueue_t* parent, const char* msg) : parent(parent) {
#if DEBUG
        parent->log_queues(msg);
#else
        UNREFERENCED_PARAMETER(msg);
#endif
    };


    ~scope_queue_logger() {
#if DEBUG
        parent->log_queues(" post");
#endif
    }
};

#endif
