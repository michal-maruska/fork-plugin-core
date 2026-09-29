#pragma once

// to keep in the Archive
template <typename P1, typename P2> struct archive_entry {
  P1 first;
  P2 second;
};

//

/* How we decided for the fork */
enum class fork_reason_t {
  reason_long,    // key pressed too long
  reason_overlap, // key press overlaps with another key
  reason_force,   // mouse-button was pressed & triggered fork.
  reason_short,
  reason_wrong,
};

struct ForkInfo {
  bool forked;
  fork_reason_t reason;
};



// Dumper:
template <typename Sig> class FunctionRef;
template <typename R, typename... Args> class FunctionRef<R(Args...)> {
  void *obj_ = nullptr;
  R (*call_)(void *, Args...) = nullptr;

public:
  FunctionRef() = default;

  template <typename F>
  FunctionRef(F &f) // holds a pointer to f, does NOT copy it
      : obj_(&f), call_([](void *o, Args... a) -> R {
          return (*static_cast<F *>(o))(static_cast<Args>(a)...);
        }) {}

  explicit operator bool() const { return call_ != nullptr; }
  R operator()(Args... a) const { return call_(obj_, static_cast<Args>(a)...); }
};
