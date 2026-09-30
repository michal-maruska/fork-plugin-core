#pragma once

#include <cstdint>
#include <libinput.h>

// #include "fork_requests.h"
#include "platform.h"
#include "colors.h"
#include <memory>
#include <string>
#include "fork_base.h"

// struct libinput_event_keyboard;

// typedef archived_event
struct archived_event
{
    uint64_t time;
    int key;
    int forked;
    bool press;                  /* client type? */
};


class libinputEvent {
public:
  // private:
  // we don't own them!
  const libinput_event_keyboard* event;
  const libinput_device *device;

public:
  libinputEvent(const libinput_event_keyboard *event, const libinput_device *device) : event(event), device(device) {};
  libinputEvent() {
    event = NULL;
    device = NULL;
  };

  ~libinputEvent() {}
};

#if 0
class xorg_event_publisher : public event_publisher<archived_event>
{
    private:
    char* memory;
    const ClientPtr client;
    /* const */ PluginInstance* plugin;
    std::size_t appendix_len;

    public:
    xorg_event_publisher(ClientPtr client, PluginInstance* plugin) : client(client), plugin(plugin) {};

    virtual ~xorg_event_publisher() {
        free(memory);
        memory = nullptr;
    }

    virtual void prepare(int max_events) override{
        // memory =
        appendix_len = sizeof(fork_events_reply) + (max_events * sizeof(archived_event));
        memory = (char*) malloc(appendix_len);
        // if this fails?
    }

    virtual int commit() override{
        xkb_plugin_send_reply(client, plugin, memory, appendix_len);
          /* What XReply to send?? */

        // can do now:
        free(memory);
        memory = nullptr;
        return 0;
    }
    virtual void event(const archived_event& event) override {
        // memcpy into the buffer:
        // typecast
        // const archived_event&
    }
};
#endif

// Closure
using archive_info = archive_entry<ForkInfo,archived_event>;
class Dumper
{
public:
    void operator() (const archive_info& info) {
#if 0
      services->vlog(services, LIBINPUT_LOG_PRIORITY_INFO,
                     "%s: dumping event %d\n", __func__, info.second.key);
#endif
    }
};


class libinputEnvironment {
public:
  using Keycode         = int;
  using Time            = uint64_t;
  using PlatformArchive = archived_event;
  using PlatformEvent   = libinputEvent;

private:
  libinput_fork_services *services;

public:
  explicit libinputEnvironment(libinput_fork_services* services) : services(services) {};

  ~libinputEnvironment() = default;


  bool output_frozen() {
    return false;
  };

  bool ignore_event(const libinputEvent& pevent) {
    return false;
  }

  // libinput_event_get_keyboard_event(
#define  GET_EVENT(pevent)                                              \
  (const_cast<libinput_event_keyboard*>(static_cast<const libinputEvent&>(pevent).event))

#define GET_DEVICE(pevent) \
  (const_cast<libinput_device*>(static_cast<const libinputEvent&>(pevent).device))


  int detail_of(const libinputEvent& pevent) const {
    // struct libinput_event_keyboard *
    auto* event = GET_EVENT(pevent);
    return libinput_event_keyboard_get_key(event);
  };

  void rewrite_event(libinputEvent& pevent, int code) {
    auto event = GET_EVENT(pevent);
    services->rewrite(event, code);
  }



  void free_event(libinputEvent* pevent) const {
    log("%s: %p\n", __func__, pevent);
    if (pevent == nullptr) {
      // ErrorF("BUG %s: %p\n", __func__, pevent);
      return;
    }

    auto* event = GET_EVENT(*pevent);
    free(event);
  }

  bool press_p(const libinputEvent& pevent) const {
    auto* event = GET_EVENT(pevent);

    return (libinput_event_keyboard_get_key_state(event) == LIBINPUT_KEY_STATE_PRESSED);
  }

  bool release_p(const libinputEvent& pevent) const {
    auto* event = GET_EVENT(pevent);

    return (libinput_event_keyboard_get_key_state(event) == LIBINPUT_KEY_STATE_RELEASED);
  }

  uint64_t time_of(const libinputEvent& pevent) const {
    auto* event = GET_EVENT(pevent);
    // libinput_event_keyboard_get_time
#if DEBUG
    log("%s: %lu, %lu usec\n", __func__, libinput_event_keyboard_get_time(event),
        libinput_event_keyboard_get_time_usec(event));
#endif
    return libinput_event_keyboard_get_time(event);
  }

  // so this is orthogonal? platform-independent?
  void archive_event(archived_event& archived_event, const libinputEvent &pevent) {

#if DEBUG > 1
    auto xevent = static_cast<libinputEvent*>(pevent)->event;
    // dynamic_cast

    log("%s:%d type: %d\n", __func__, __LINE__, xevent->any.type);
    log("%s:%d keycode: %d\n", __func__, __LINE__, detail_of(pevent));
    log("%s:%d keycode via function: %d\n", __func__, __LINE__, detail_of(pevent));
#endif
    archived_event.key = detail_of(pevent);
    archived_event.time = time_of(pevent);
    archived_event.press = press_p(pevent);
  };

  void relay_event(const libinputEvent &pevent) const {
    auto &li_event = const_cast<libinputEvent&>(static_cast<const libinputEvent&>(pevent));
#if 0
    log("%s: (%p) %p, device %p\n", __func__, pevent, event, GET_DEVICE(pevent));
#endif

    services->post_event(services,
                         const_cast<libinput_device*>(li_event.device),
                         const_cast<libinput_event_keyboard*>(li_event.event));
#if 0
    li_event.event = NULL;
    li_event.device = NULL;
#endif
    // bug: todo:
    // delete li_event;
  };

  void push_time(uint64_t now) {
    // services->push_time(now)
  }


  void log(const char* format ...) const {
    va_list args;
    va_start(args, format);
    services->vlog(services, LIBINPUT_LOG_PRIORITY_INFO, format, args);
    va_end(args);
  }

  void vlog(const char* format, va_list args) const {
    services->vlog(services, LIBINPUT_LOG_PRIORITY_INFO, format, args);
  }


  // the idea was to return a string. but who will deallocate it?
  void fmt_event(const char* message, const libinputEvent &pevent) const {
    // return std::string("");
    log("%s (%s): %pm\n", message, __func__, GET_EVENT(pevent));
  };

#if 0
  virtual
  std::unique_ptr<forkNS::event_dumper<archived_event>> get_event_dumper() override {
    // return nullptr;
    return std::make_unique<libinput_event_dumper>(); // services
  }
#endif

};

