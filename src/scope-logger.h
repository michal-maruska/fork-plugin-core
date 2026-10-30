#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(x)   (void)(x)
#endif

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
