/*
 * Copyright (C) 2003-2005-2010-2024 Michal Maruska <mmaruska@gmail.com>
 * License:  Creative Commons Attribution-ShareAlike 3.0 Unported License
 */

/* How we operate:
 *
 *   ProcessEvent ->                          accept_event
 *                 invoke the `machine'   -> accept_time
 *                                            step_by_force    |
 *   ProcessTime ->                                            v
 *                                       restart              hand_over_event_to_next_plugin
 *   mouse_call_back---
 *
 *   NotifyThaw: ---
 */

#include "platform.h"
extern "C" {
#include <fork_requests.h>
#undef max
#undef min
}

#include "config.h"
#include "debug.h"

#include <memory>

#include "xmachine.h"
#include "xorg.h"

#include "event_ops.h"

extern "C" {
#include <xorg/xkbsrv.h>
#include <xorg/xf86Module.h>
}



// is it available from somewhere else?
const char* event_names[] = {
    "KeyPress",
    "KeyRelease",
    "ButtonPress",
    "ButtonRelease",
    "Motion",
    "Enter",
    "Leave",
    // 9
    "FocusIn",
    "FocusOut",
    "ProximityIn",
    "ProximityOut",
    // 13
    "DeviceChanged",
    "Hierarchy",
    "DGAEvent",
    // 16
    "RawKeyPress",
    "RawKeyRelease",
    "RawButtonPress",
    "RawButtonRelease",
    "RawMotion",
    "XQuartz"
};

/* Push the event to the next plugin. Ownership is handed over! */
void
hand_over_event_to_next_plugin(const InternalEvent& event, PluginInstance* const nextPlugin)
{
    assert (!plugin_frozen(nextPlugin));
    PluginClass(nextPlugin)->ProcessEvent(nextPlugin,
                                          const_cast<InternalEvent*>(&event), FALSE); // not owner
    // we always own the event (up to now)
}

enum keycodes {
    zero = 19,
    one = 10,
    PAUSE = 127,
    key_k = 45,
    key_l = 46,
};


#define plugin_machine(plugin) ((machineRec*)(plugin->data))

/*
 * React to some `hot_keys':
 * Pause  Pause  -> dump
 */
static bool // return @true if config-mode continues.
handle_config_key(const PluginInstance *const plugin, const InternalEvent *event)
{
    // I could use optional
    static KeyCode key_to_fork = 0;         //  what key we want to configure
    machineRec* machine;

    if (press_p(event)) {
        auto keycode = detail_of(event);
        ErrorF("%s: servicing %d\n", __func__, keycode);
        switch (keycode) {
            case keycodes::PAUSE:
                machine = plugin_machine(plugin);
                machine->dump_last_events(std::make_unique<xorg_event_dumper>(plugin->device).get());
                ErrorF("%s: serviced %d\n", __func__, keycode);
                break;
#if MULTIPLE_CONFIGURATIONS
            case keycodes::zero:
                machine = plugin_machine(plugin);
                machine->switch_config(0); // current ->toggle ?
                /* fixme: but this is default! */
                machine->forkActive[keycode] = 0; /* ignore the release as well. */
                break;
            case keycodes::one:
                machine = plugin_machine(plugin);
                machine->switch_config(1); // current ->toggle ?
                machine->forkActive[keycode] = 0;
                break;
#endif
            case keycodes::key_l:
                machine = plugin_machine(plugin);
                ErrorF("%s: toggle debug\n", __func__);
                machine->set_debug(1);
                break;
            case keycodes::key_k:
                machine = plugin_machine(plugin);
                ErrorF("%s: toggle debug\n", __func__);
                machine->set_debug(0);
                break;

            default:            /* todo: remove this: */
                // so press BREAK FROM TO
                // to have FROM fork to TO
                if (key_to_fork == 0) {
                    key_to_fork = keycode;
                } else {
                    machineRec* machine = plugin_machine(plugin);
                    machine->config->fork_keycode[key_to_fork] = keycode;
                    key_to_fork = 0;
                }
        };
    }
    return true;
}

// fixme: configurable!
constexpr int MINIMUM_DURATION_MSEC = 30;

static bool   // return:  true if handled & should be skipped
filter_config_key_maybe(const PluginInstance *const plugin, const InternalEvent* const event)
{
    static bool config_mode = false; // While the Pause key is down.
    static Time last_press_time = 0;

    if (config_mode) {
        static bool latch = false;
        // [21/10/04]  I noticed, that some (non-plain ps/2) keyboard generate
        // the release event at the same time as press.
        // So, to overcome this limitation, I detect this short-lasting `down' &
        // take the `next' event as in `config_mode'   (latch)

        if ((detail_of(event) == keycodes::PAUSE) && release_p(event)) { //  fake ?
            if ( (time_of(event) - last_press_time) < MINIMUM_DURATION_MSEC) {
                ErrorF("the key seems buggy, tolerating %" TIME_FMT ": duration:%" TIME_FMT ". Latching config mode\n",
                       time_of(event),
                       time_of(event) - last_press_time);
                latch = true;
                return true;
            }
            config_mode = false;
            // fixme: key_to_fork = 0;
            ErrorF("dumping (%s) %" TIME_FMT ": %d!\n",
                   plugin->device->name,
                   time_of(event), (int)(time_of(event) - last_press_time));
            // todo: send a message to listening clients.
            plugin_machine(plugin)->dump_last_events(std::make_unique<xorg_event_dumper>(plugin->device).get());

        } else {
            last_press_time = 0;
            config_mode = handle_config_key(plugin, event);
            if (latch) {
                config_mode = latch = false;
            };
        }
    }
    if ((detail_of(event) == keycodes::PAUSE) && press_p(event))
        /* wait for the next and act ? but start w/ printing the last events: */
    {
        last_press_time = time_of(event);
#if DEBUG
        ErrorF("entering config_mode & discarding the event: %" TIME_FMT "!\n", last_press_time);
#endif
        config_mode = true;

        /* fixme: should I update the ->down bitarray? */
        return true;
    } else
        last_press_time = 0;

    return false;
}

static Time
first_non_zero(Time a, Time b)
{
    return (a==0)?b:a;
}

// set plugin->wakeup_time
static void
set_wakeup_time(PluginInstance *plugin, Time machine_time)
{
    machineRec *machine = plugin_machine(plugin);
    plugin->wakeup_time =
        // fixme:  but ZERO has certain meaning!
        // this is wrong: if machine waits, it cannot pass to the next-plugin!
        first_non_zero(machine_time, plugin->next->wakeup_time);

    if (plugin->wakeup_time != 0) {
        if (machine_time != 0) {
            machine->mdb("%s %s wakeup_time = %d, next wants: %u, we %" TIME_FMT "\n",
                FORK_PLUGIN_NAME, __func__,
                (int)plugin->wakeup_time, (int)plugin->next->wakeup_time, machine_time);
        }
    }
}


/*  This is the handler for all key events.  Here we delay pushing them forward.
    it's a trampoline for the automaton.
    Should it return some Time?
*/
extern "C" /* static*/ Bool
ForkProcessEvent(PluginInstance* plugin, InternalEvent *event, const Bool owner)
// __attribute__((visibility("default")))
{
    DB("%s: start %d %s\n", __func__, event->any.type, owner?"owner":"not owner");

    if ((event->any.type != ET_KeyPress) && (event->any.type != ET_KeyRelease)) {
        // ET_RawKeyPress
#if DEBUG > 1
        ErrorF("ignoring this type of event %d\n", event->any.type);
#endif
        goto exit_free;
    }

    // this could be a different plugin!
    if (filter_config_key_maybe(plugin, event)) {
        // should we update the XKB `down' array, to signal that the key is up/down?
        // todo: I should at least push the time of (plugin->next)!
        ErrorF("not passing this event to forking-machine\n");

        goto exit_free;
    };

    {
        const auto machine = plugin_machine(plugin);

        const XorgEvent pevent {event};
        if (owner) {
            free(event);
            event = NULL;
        }
        Time next = machine->accept_event(pevent);
        // unlocked here now!
        set_wakeup_time(plugin, next);
    }


    // by now, if owner, we consumed the event.
    goto exit;
  exit_free:
    if (owner)
        free(event);

  exit:
    return PLUGIN_NON_FROZEN;
};

static Bool
step_in_time(PluginInstance* plugin, Time now)
{
    machineRec *machine = plugin_machine(plugin);
#if DEBUG > 1
    machine->mdb("%s: %" TIME_FMT "\n", __func__, now);
#endif

    Time next = machine->accept_time(now);
    // todo: we could push the time before the first event in internal queue!
    set_wakeup_time(plugin, next);

    return PLUGIN_NON_FROZEN;
};


/**
 * Called from AllowEvents, or "next plugin" in the chain.
 */
static void
fork_thaw_notify(PluginInstance* plugin, Time now)
{
    machineRec* machine = plugin_machine(plugin);
    machine->mdb("%s @ time %" TIME_FMT "\n", __func__, now);

    // re-entrancy:
    // if locked .... put order on shared data .... then retry to lock ... if lock -> process the
    // order. if not, we know someone will process it. ....but a memory barrier is needed.

    Time next = machine->accept_time(now);

    if (!plugin_frozen(plugin->next) && PluginClass(plugin->prev)->NotifyThaw) {
        /* thaw the previous! */
        set_wakeup_time(plugin, next);

        machine->mdb("%s -- sending thaw Notify upwards!\n", __func__);
        /* fixme:  Tail-recursion! */
        PluginClass(plugin->prev)->NotifyThaw(plugin->prev, now);
        /* I could move now to the time of our event. */
        /* accept_time(plugin); */
    } else {
        machine->mdb("%s -- NOT sending thaw Notify upwards %s!\n", __func__,
                     plugin_frozen(plugin->next)?"next is frozen":"prev has not NotifyThaw");
    }
}


/* For now this is called too many times, for different events.! */
static void
mouse_call_back(CallbackListPtr *, PluginInstance* plugin,
                DeviceEventInfoRec* dei)
{
    InternalEvent *event = dei->event;
    if (event->any.type == ET_Motion) {

        machineRec *machine = plugin_machine(plugin);
        machine->accept_confirmation();
    }
}


/**
 * We have to setup this plugin:
 * - make a (new) automaton: allocate default config,
 * - register hooks to other devices,
 *
 * returns: erorr of Success. Should attach stuff by side effect ! */
PluginInstance*
create_plugin(const DeviceIntPtr keybd, DevicePluginRec* plugin_class)
{
    DB("%s @%p\n", __func__, static_cast<void *>(keybd->name));

    assert (strcmp(plugin_class->name, FORK_PLUGIN_NAME) == 0);
    PluginInstance* plugin = (PluginInstance*) malloc(sizeof(PluginInstance));
    plugin->pclass = plugin_class;
    plugin->device = keybd;
    plugin->frozen = FALSE;
    plugin->wakeup_time = 0;

    ErrorF("%s: constructing the machine. Version %d (official release: %s)\n",
           __func__, PLUGIN_VERSION, VERSION_STRING);

    auto xorg = std::make_unique<XOrgEnvironment>(keybd, plugin);
    auto* const forking_machine = new machineRec(xorg.release());
    forking_machine->create_configs();

    plugin->data = static_cast<void *>(forking_machine);

    ErrorF("%s:keybd: next %p private %p on: %d\n", __func__, keybd->next, keybd->cpublic.devicePrivate, keybd->cpublic.on);
    ErrorF("%s:keybd: coreEvents %d, size %zd %zd\n", __func__, keybd->coreEvents, sizeof(Atom), sizeof(CARD32));
    ErrorF("%s:@%s returning %d\n", __func__, keybd->name, Success);

    AddCallback(&DeviceEventCallback, reinterpret_cast<CallbackProcPtr>(mouse_call_back), (void*) plugin);

    plugin_class->ref_count++;
    return plugin;
};


/** Configuration apis
 */
// todo: make it inline functions
inline int subtype_n_args(int t) { return  (t & 3);}
inline int type_subtype(int t) { return (t >> 2);}

/* Return a value requested, or 0 on error.*/
int
machine_configure_get(PluginInstance* plugin, int values[5], int return_config[3])
{
   assert (strcmp (PLUGIN_NAME(plugin), FORK_PLUGIN_NAME) == 0);

   machineRec *machine = plugin_machine(plugin);

   int type = values[0];

   /* fixme: why is type int?  shouldn't CARD8 be enough?
      <int type>
      <int keycode or time value>
      <keycode or time value>
      <timevalue>

      type: local & global
   */

   machine->mdb("%s: %d operands, command %d: %d %d\n", __func__, subtype_n_args(type),
                type_subtype(type), values[1], values[2]);

   switch (subtype_n_args(type)){
   case 0:
           return_config[0]= machine->configure_global(type_subtype(type), 0, 0);
           break;
   case 1:
           return_config[0]= machine->configure_key(type_subtype(type), values[1], 0, 0);
           break;
   case 2:
           return_config[0]= machine->configure_twins(type_subtype(type), values[1], values[2], 0, 0);
           break;
   case 3:
           return 0;
        default:
      machine->mdb("%s: invalid option %d\n", __func__, subtype_n_args(type));
   }
   return 0;
}

/* Scan the DATA (of given length), and translate into configuration commands,
   and execute on plugin's machine */
int
machine_configure(PluginInstance* plugin, int values[5])
{
    assert (strcmp (PLUGIN_NAME(plugin), FORK_PLUGIN_NAME) == 0);

    machineRec *machine = plugin_machine(plugin);

    int type = values[0];
    machine->mdb("%s: %d operands, command %d: %d %d %d\n",
                 __func__,
                 subtype_n_args(type), type_subtype(type),
                 values[1], values[2],values[3]);

    switch (subtype_n_args(type)) {
        case 0:
            machine->configure_global(type_subtype(type), values[1], true);
            break;

        case 1:
            machine->configure_key(type_subtype(type), values[1], values[2], true);
            break;

        case 2:
            machine->configure_twins(type_subtype(type), values[1], values[2], values[3], true);
        case 3:
            // special requests ....
            break;
        default:
            machine->mdb("%s: invalid option %d\n", __func__, subtype_n_args(type));
    }
    machine->mdb("%s: done", __func__);
    return 0;
    /* return client->noClientException; */
}


/*todo: int */
void
machine_command(ClientPtr client, PluginInstance* plugin, int cmd, int data1,
                int data2, int data3, int data4)
{
  DB("%s cmd %d, data %d ...\n", __func__, cmd, data1);
  machineRec *machine = plugin_machine(plugin);
  auto env = dynamic_cast<XOrgEnvironment*>(machine->environment.get());

  switch (cmd) {
      case fork_client_dump_keys:
      {
          auto dumper = env->get_event_publisher(client, plugin);
          machine->dump_last_events_to_client(dumper.get(), data1);
          break;
      }
      default:
          DB("%s Unknown command!\n", __func__);
  }
}

/* fixme!
   This is a wrong API: there is no guarantee we can do this.
   The pipeline can get frozen, and we have to wait on thaw.
   So, it's better to have a callback.

the plugin should not pass more events.
   */
static int
stop_and_exhaust_machine(PluginInstance* plugin)
{
    const auto machine = plugin_machine(plugin);
    machine->mdb("%s: what to do?\n", __func__);
    // free all the stuff, and then:
    machine->stop();

    xkb_remove_plugin(plugin); // will this lead to destroy_plugin()?
    return 1;
}


static int
destroy_plugin(PluginInstance* plugin)
{
    machineRec *machine = plugin_machine(plugin);
    // should be locked from the STOP call?
    DeleteCallback(&DeviceEventCallback, (CallbackProcPtr) mouse_call_back,
                   (void*) plugin);
    // still dangerous? We need to wait for the callback to finish, but it's in this thread!
    machine->stop();
    delete machine;
    return 1;
}


// This macro helps with providing
// initial value of struct- the member name is along the value.
#if __GNUC__
#define _B(name, value) value
#else
#define _B(name, value) name : value
#endif

extern "C" {

static void* /*DevicePluginRec* */
fork_plug(void          *options,
          int		*errmaj,
          int		*errmin,
          void* dynamic_module)
{
  ErrorF("%s: %s version %d, built %s\n", __func__, FORK_PLUGIN_NAME, PLUGIN_VERSION, __TIMESTAMP__);

  static struct _DevicePluginRec plugin_class = {
    _B(name, FORK_PLUGIN_NAME),
    _B(instantiate, &create_plugin),
    _B(ProcessEvent, ForkProcessEvent),
    _B(ProcessTime, step_in_time),
    _B(NotifyThaw, fork_thaw_notify),
    _B(config,    machine_configure),
    _B(getconfig, machine_configure_get),
    _B(client_command, machine_command),
    _B(module, NULL),
    _B(ref_count, 0),
    _B(stop,       stop_and_exhaust_machine),
    _B(terminate,  destroy_plugin)
  };
  plugin_class.ref_count = 0;
  ErrorF("assigning %p\n", dynamic_module);

  plugin_class.module = dynamic_module;
  xkb_add_plugin_class(&plugin_class);

  return &plugin_class;
}

static void*
SetupProc(void* module, pointer options, int *errmaj, int *errmin)
{
    ErrorF("%s: %s %p\n", __func__, __TIMESTAMP__, module);

    fork_plug(NULL,NULL,NULL, module);
    // on_init();
    return module;
}

#define PACKAGE_VERSION_MAJOR 1
#define PACKAGE_VERSION_MINOR 0
#define PACKAGE_VERSION_PATCHLEVEL 0

static XF86ModuleVersionInfo VersionRec = {
    "fork",
    MODULEVENDORSTRING,
    MODINFOSTRING1,
    MODINFOSTRING2,
    XORG_VERSION_CURRENT,
    PACKAGE_VERSION_MAJOR, PACKAGE_VERSION_MINOR, PACKAGE_VERSION_PATCHLEVEL,
    //
    ABI_CLASS_INPUT,
    ABI_INPUT_VERSION,
    MOD_CLASS_INPUT,
    {0, 0, 0, 0}
};

_X_EXPORT XF86ModuleData forkModuleData = {
    &VersionRec,
    &SetupProc,
    // teardown:
    NULL
};

}
