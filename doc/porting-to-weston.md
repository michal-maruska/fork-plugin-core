# How I enabled forking in [Weston](https://wayland.pages.freedesktop.org/weston/#)

It took a week.

## From Weston to libinput
I started to look where I could interrupt the flow of key-events processing and insert the plugin (forking machine).

I wanted to target [libinput](https://wayland.freedesktop.org/libinput/doc/latest/index.html) because it does [emulate middle-button](https://wayland.freedesktop.org/libinput/doc/latest/configuration.html#middle-button-emulation) (by postponing 2 mouse clicks), so
this is similar.

* I found `libinput_udev_create_context` which seems the point when Weston starts to use libinput.
Then it customizes the log priority and log-handler.

So I added         `libinput_setup_fork()` call.

https://github.com/MichalMaruska/weston/blob/mmc/libweston/libinput-seat.c#L373

### The libinput side....
I found out how events are put onto a circular-buffer.

That is, Weston upon select(2), makes 2 api calls into libinput...:

* to read from device nodes and populate the circular buffer
* to get the events (from the buffer) and process them (into focus or grab client).

So I needed to act before keyboard event is put into that buffer:
https://github.com/MichalMaruska/libinput/blob/mmc/src/libinput.c#L2579

But to get the plugin loaded, I needed some [dlopen()](https://man7.org/linux/man-pages/man3/dlopen.3.html) and somehow register.
I got a module loading from Weston, since it's under my sight, and maybe license permits such a remix.

Result is this: `weston_load_module` and my  `libinput_setup_fork`
https://github.com/MichalMaruska/libinput/blob/9ad50bb391b559d37aa600a80a14c8763fe3ac74/src/libinput.c#L1940

## Create the **plugin** itself!!!
So... I need something to do the registration (to interpose in the flow), and create the forkingMachine.
And then the platform-abstraction class specific for the libinput environment.
* https://github.com/MichalMaruska/fork-plugin/blob/clion/libinput/fork-plugin.cpp
* https://github.com/MichalMaruska/fork-plugin/blob/clion/libinput/libinput_environment.h

The plugin itself hardcoded a forking for Space key, to act as Shift. And voila!

## Activating XKB -- my XKB keymap
To get a working weston-terminal, I need much more than Space-as-shift. And certainly my XKB keymap,
at least Control on right Alt key. (it's all [static](https://unix.stackexchange.com/questions/309580/does-wayland-use-xkb-for-keyboard-layouts) btw)

But Weston uses libxkbcommon, and only the
[RMLVO](https://who-t.blogspot.com/2020/02/user-specific-xkb-configuration-part-1.html) specs. But I use my assembled xkb
file. So

* I changed Weston the use the other Api -- `xkb_keymap_new_from_file`:
 https://github.com/MichalMaruska/weston/blob/mmc/libweston/input.c#L4061

then I could change the hardcoded forking and got the same features as under Xorg!

Then I build all the resulting Debian (sid) packages in:
https://github.com/MichalMaruska/michalmaruska.github.io

