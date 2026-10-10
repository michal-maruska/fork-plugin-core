#pragma once

extern "C" {
#include "fork_requests.h"
}

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


class XorgEvent {
    friend class XOrgEnvironment;
private:
    InternalEvent event;
public:
    // take ownership:  -- no
    XorgEvent(const InternalEvent* event) : event(*event) {};
    // so why not UniquePointer?

    XorgEvent() {
        event = {{0}};
    };
};

class XOrgEnvironment {
public:
    using Keycode         = KeyCode;
    using Time            = ::Time;
    using PlatformArchive = archived_event;
    using PlatformEvent   = XorgEvent;

private:
    const DeviceIntPtr keybd; // reference
    PluginInstance* const plugin;

public:
    XOrgEnvironment(const DeviceIntPtr keybd, PluginInstance* plugin): keybd(keybd), plugin(plugin){};
    // should I just assert(keybd)

    ~XOrgEnvironment() = default;

    bool output_frozen() {
        const PluginInstance* const nextPlugin = plugin->next;
        return plugin_frozen(nextPlugin);
    };

    /* certain keys might be emulating a different device. */
    bool ignore_event(const XorgEvent &pevent) {
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



    KeyCode detail_of(const XorgEvent& pevent) const {
        return pevent.event.device_event.detail.key;
    };

    void rewrite_event(XorgEvent& pevent, KeyCode code) {
        auto& event = pevent.event;
        event.device_event.detail.key = code;
    }

    bool press_p(const XorgEvent& pevent) const {
        auto& event = static_cast<const XorgEvent&>(pevent).event;
        return (event.any.type == ET_KeyPress);
    }
    bool release_p(const XorgEvent& pevent) const {
        auto& event = static_cast<const XorgEvent&>(pevent).event;
        return (event.any.type == ET_KeyRelease);
    }
    Time time_of(const XorgEvent& pevent) const {
        auto& event = static_cast<const XorgEvent&>(pevent).event;
        return event.any.time;
    }

    void free_event(XorgEvent* pevent) const {
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
    void archive_event(archived_event& archived_event, const XorgEvent& pevent) {

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

    void relay_event(const XorgEvent& pevent) const {
#if DEBUG
        fmt_event(__func__, pevent);
#endif
        const auto& event = pevent.event;
        PluginInstance* nextPlugin = plugin->next;
        hand_over_event_to_next_plugin(event, nextPlugin);
    };

    void push_time(Time now) {
        PluginInstance* nextPlugin = plugin->next;
#if DEBUG > 1
        ErrorF("%s: %" TIME_FMT "\n", __func__, now);
#endif
        PluginClass(nextPlugin)->ProcessTime(nextPlugin, now);
    }

    void log(const char* format ...) const {
        va_list argptr;
        va_start(argptr, format);
        VErrorF(format, argptr);
        va_end(argptr);
    }

    void vlog(const char* format, va_list argptr) const {
        va_list ap2;
        va_copy(ap2, argptr);       // copy existing va_list
        VErrorF(format, ap2);
        va_end(ap2);
    }

    // the idea was to return a string. but who will deallocate it?
    void fmt_event(const char* message, const XorgEvent& pevent) const {
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

};



using archive_info = archive_entry<ForkInfo,archived_event>;
class XorgDumper
{
public:
    void operator() (const archive_info& info) {
        // , keybd->name
        ErrorF("%s: dumping event %d\n", __func__,
               info.second.key);
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
