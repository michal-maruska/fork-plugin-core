# Keyboard filter driver

## Summary:  Use "[home row](https://en.wikipedia.org/wiki/Touch_typing#Home_row)" keys as modifiers (shift, hyper, super, kana,roya)

This is a filter which parses the stream of keyboard events and detects _simultaneous_
key-presses to reinterpret specific use of selected keys as modifiers, instead
of their regular function. Also the _timing_ is significant.

**Example**: use "a" key to activate numeric keypad on 'uio jkl m-." keys.

Packaged as a plugin for X server, Weston plugin, and a Windows 10+ filter kernel driver.

## How to install?
* Windows ...
  [see instructions](doc/windows-client-install.md)

* Linux -- either build from source, or use Debian (Sid) packages from
 [my reprepro apt repository](https://github.com/MichalMaruska/michalmaruska.github.io)

  - Xorg server -- a patch is needed to enable plugins:
      [xserver](https://github.com/MichalMaruska/xserver/commits/mmc-all)

  - Weston --
      a patch is needed for [libinput](https://github.com/MichalMaruska/libinput/commits/main/)
      and [weston](https://github.com/MichalMaruska/weston/commits/main/)

