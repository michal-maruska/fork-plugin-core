# Keyboard filter driver

This is a filter which scans the keyboard events and detects simultaneous key-presses to reinterpret
specific use of selected keys as modifiers, instead of their regular function.

Packaged as a plugin for X server, Weston plugin, and a Windows 10+ filter kernel driver.

## How to install?
* Windows ...

* Linux -- patched (up-to-date) sources and Debian (Sid) packages are available:
  debian packages in [my reprepro apt repository](https://github.com/MichalMaruska/michalmaruska.github.io)

- Xorg server -- a patch is needed to enable plugins:
  [xserver git repo](https://github.com/MichalMaruska/xserver/commits/mmc-all)

- Weston
  patch is needed for [libinput](https://github.com/MichalMaruska/libinput/commits/main/)
  and [weston](https://github.com/MichalMaruska/weston/commits/main/)

