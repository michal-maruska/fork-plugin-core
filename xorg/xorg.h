#pragma once

#include "fork_requests.h"
#include "platform.h"
#include "colors.h"
#include <memory>

extern "C" {
#include <xorg-server.h>

#ifndef MMC_PIPELINE
#error "This is useful only when the xorg-server is configured with --enable-pipeline"
#endif

// _XSERVER64
#include <X11/X.h>
#include <X11/Xproto.h>
#include <xorg/inputstr.h>

#include <xorg/xkbsrv.h>
#include <X11/Xdefs.h>
#include <xorg/input.h>
#include <xorg/eventstr.h>

// these macros are not used, and only clash with C++
#undef xalloc
#undef max
#undef min
}
#include <string>

extern void
hand_over_event_to_next_plugin(const InternalEvent& event, PluginInstance* const nextPlugin);


static void dump_event(KeyCode key, KeyCode fork, bool press, Time event_time,
                       XkbDescPtr xkb, XkbSrvInfoPtr xkbi, Time prev_time);


class xorg_event_publisher : public forkNS::event_publisher<archived_event>
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


// Closure
class xorg_event_dumper : public forkNS::event_dumper<archived_event>
{
private:
    const XkbSrvInfoPtr xkbi;
    const XkbDescPtr xkb;
    Time previous_time;

public:
    void operator() (const archived_event& event) override {
        dump_event(event.key,
                   event.forked,
                   event.press,
                   event.time,
                   xkb, xkbi, previous_time);
        previous_time = event.time;
    };

    virtual ~xorg_event_dumper() {};


    explicit xorg_event_dumper(const DeviceIntPtr keybd) :
        xkbi(keybd->key->xkbInfo),
        xkb(xkbi->desc),
        previous_time(0) {
#if DEBUG > 1
        ErrorF("%s: creating dumper for %s\n", __func__, keybd->name);
#endif
    };
};


class XorgEvent {
    friend class XOrgEnvironment;
private:
    InternalEvent event;
public:
    // take ownership:  -- no
    XorgEvent(const InternalEvent* event) : event(*event) {};
    // so why not UniquePointer?
};

class XOrgEnvironment : public forkNS::platformEnvironment<KeyCode, Time, archived_event, XorgEvent> {

private:
    const DeviceIntPtr keybd; // reference
    PluginInstance* const plugin;

public:
    XOrgEnvironment(const DeviceIntPtr keybd, PluginInstance* plugin): keybd(keybd), plugin(plugin){};
    // should I just assert(keybd)

    virtual ~XOrgEnvironment() = default;

    bool output_frozen() override {
        const PluginInstance* const nextPlugin = plugin->next;
        return plugin_frozen(nextPlugin);
    };

    /* certain keys might be emulating a different device. */
    bool ignore_event(const XorgEvent &pevent) override {
        // __unused__ ?
        if (!keybd || !keybd->key) {
            // should I just assert(keybd)
            ErrorF("%s: keybd is wrong!", __func__);
            return false;
        }

        const XkbSrvInfoPtr xkbi= keybd->key->xkbInfo;
        const XkbDescPtr xkb = xkbi->desc;
        return (xkb->ctrls->enabled_ctrls & XkbMouseKeysMask);
    }



    KeyCode detail_of(const XorgEvent& pevent) const override {
        return pevent.event.device_event.detail.key;
    };

    virtual void rewrite_event(XorgEvent& pevent, KeyCode code) override {
        auto& event = pevent.event;
        event.device_event.detail.key = code;
    }

    virtual bool press_p(const XorgEvent& pevent) const override {
        auto& event = static_cast<const XorgEvent&>(pevent).event;
        return (event.any.type == ET_KeyPress);
    }
    virtual bool release_p(const XorgEvent& pevent) const override {
        auto& event = static_cast<const XorgEvent&>(pevent).event;
        return (event.any.type == ET_KeyRelease);
    }
    virtual Time time_of(const XorgEvent& pevent) const override {
        auto& event = static_cast<const XorgEvent&>(pevent).event;
        return event.any.time;
    }

    virtual void free_event(XorgEvent* pevent) const override {
        if (pevent == nullptr) {
            ErrorF("BUG %s: %p\n", __func__, pevent);
            return;
        }
#if 0
        InternalEvent& event = static_cast<XorgEvent*>(pevent)->event;
        free(event);
        static_cast<XorgEvent*>(pevent)->event = nullptr;
#endif
    }

    // so this is orthogonal? platform-independent?
    void archive_event(archived_event& archived_event, const XorgEvent& pevent) override {

#if DEBUG > 1
        auto xevent = static_cast<XorgEvent*>(pevent)->event;
        // dynamic_cast

        log("%s:%d type: %d\n", __func__, __LINE__, xevent->any.type);
        log("%s:%d keycode: %d\n", __func__, __LINE__, detail_of(pevent));
        log("%s:%d keycode via function: %d\n", __func__, __LINE__, detail_of(pevent));
#endif
        archived_event.key = detail_of(pevent);
        archived_event.time = time_of(pevent);
        archived_event.press = press_p(pevent);
    };

    virtual void relay_event(const XorgEvent& pevent) const override {
#if DEBUG
        fmt_event(__func__, pevent);
#endif
        const auto& event = pevent.event;
        PluginInstance* nextPlugin = plugin->next;
        hand_over_event_to_next_plugin(event, nextPlugin);
    };

    virtual void push_time(Time now) override {
        PluginInstance* nextPlugin = plugin->next;
#if DEBUG > 1
        ErrorF("%s: %" TIME_FMT "\n", __func__, now);
#endif
        PluginClass(nextPlugin)->ProcessTime(nextPlugin, now);
    }

    virtual void log(const char* format ...) const override {
        va_list argptr;
        va_start(argptr, format);
        VErrorF(format, argptr);
        va_end(argptr);
    }

    virtual void vlog(const char* format, va_list argptr) const override {
        VErrorF(format, argptr);
    }

    // the idea was to return a string. but who will deallocate it?
    virtual void fmt_event(const char* message, const XorgEvent& pevent) const override {
#if 1
        const KeyCode key = detail_of(pevent);
        const bool press = press_p(pevent);
        const bool release = release_p(pevent);

        log("%s(%s): %s%u %s%s\n", message, __func__,
            info_color,
            key,
            (press ? "down" : (release ) ? "up" : "??"),
            color_reset);

#if DEBUG > 1
        log("%s: trying to resolve to keysym %d through %p\n", __func__, key, keybd);
#endif
#if 0
        if (keybd && keybd->key) {
            const XkbSrvInfoPtr xkbi = keybd->key->xkbInfo;
            const KeySym *sym = XkbKeySymsPtr(xkbi->desc, key);

            if ((!sym) || (!isalpha(*(unsigned char *) sym)))
                sym = (KeySym *) " ";

            log("%s: %s%s%s %s\n", key,
                key_color, (char) *sym, color_reset,
                (press ? "down" : (release ) ? "up" : "??"));
        }
#endif
#endif
    };

#if 0
    virtual
    std::unique_ptr<forkNS::event_dumper<archived_event>> get_event_dumper() override {
        return std::make_unique<xorg_event_dumper>(keybd);
    }
#endif

#if 0
    virtual
    std::unique_ptr<event_publisher> get_event_publisher() override {
        return std::make_unique<xorg_event_publisher>(keybd);
    }
#endif


    // specific, not virtual!:
    std::unique_ptr<forkNS::event_publisher<archived_event>> get_event_publisher(ClientPtr client, PluginInstance *plugin) {
        return std::make_unique<xorg_event_publisher>(client, plugin);
    }
};



// prints into the Xorg.*.log
static void
dump_event(KeyCode key, KeyCode fork, bool press, Time event_time,
           const XkbDescPtr xkb, const XkbSrvInfoPtr xkbi, Time prev_time)
{
    if (key == 0)
        return;

    ErrorF("%s: %d %.4s\n", __func__,
           key,
           xkb->names->keys[key].name);

    // 0.1   keysym bound to the key:
    KeySym* sym= XkbKeySymsPtr(xkbi->desc,key); // mmc: is this enough ?
    [[maybe_unused]] char* sname = nullptr;

    if (sym){
#if 0
        // todo: fixme!
        sname = XkbKeysymText(*sym,XkbCFile); // doesn't work inside server !!
#endif
        // my ascii hack
        if (! isalpha(* reinterpret_cast<unsigned char *>(sym))){
            sym = (KeySym*) " ";
        } else {
            static char keysymname[15];
            sprintf(keysymname, "%c",(* (char*)sym)); // fixme!
            sname = keysymname;
        };
    };
    /*  Format:
        keycode
        press/release
        [  57 4 18500973        112
        ] 33   18502021        1048
    */

    ErrorF("%s %d (%d)",
           (press?" ]":"[ "),
           static_cast<int>(key), static_cast<int>(fork));
    ErrorF(" %.4s (%5.5s) %" TIME_FMT "\t%" TIME_FMT "\n",
           sname, sname,
           event_time,
           event_time - prev_time);
}
