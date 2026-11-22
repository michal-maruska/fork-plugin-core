# Fork — home-row modifiers on ordinary keyboards

Fork turns ordinary keys into modifiers based on *how* you press them. Tap a key
and you get the letter. Hold it for a moment, or press it together with another
key, and it becomes Shift, Ctrl, Alt, Super or Hyper. It works with the keyboard
you already have — no firmware flashing and no custom hardware.

**Example:** hold `a` and `u i o / j k l / m , .` become the numeric keypad
`7 8 9 / 4 5 6 / 1 2 3`. Release `a` and everything is normal again.

Fork can also switch to the modifier on a mouse click, so you can Shift-click,
Ctrl-drag or resize windows with a forked modifier.

## What you get

- Home-row modifiers (and any other key-to-modifier mapping) on a normal keyboard.
- Trigger by holding a key, by pressing it in parallel with another key, or by
  clicking the mouse.
- A quick tap still types the original key; auto-repeat keeps working for keys
  that are not forked.
- Works system-wide, so every application sees the resulting modifiers.
- Same core behaviour on Xorg, Wayland/Weston and Windows.

## Platform support

| Platform | Status | Integration |
| --- | --- | --- |
| Linux — Xorg | supported | Xorg server plugin (patched Xorg) |
| Linux — Wayland (Weston) | supported | patched libinput + Weston |
| Windows 10/11 | supported | usermode filter in separate repo |
| macOS | not yet | DriverKit/kext planned |

## Install

### Linux — Debian Sid / Unstable (recommended)

Packages are published in the author's APT repository. It also carries the
patched Xorg server (from [https://github.com/michal-maruska/xserver](https://github.com/michal-maruska/xserver)
under TAGs) and the patched libinput/Weston that the plugin needs.
The Weston and libinput patches are at
[https://github.com/michal-maruska/weston](https://github.com/michal-maruska/weston) and
[https://github.com/michal-maruska/libinput](https://github.com/michal-maruska/libinput).

```sh
sudo wget -O /etc/apt/trusted.gpg.d/maruska.asc \
  https://michal-maruska.github.io/apt-repo/maruska.asc
echo 'deb [signed-by=/etc/apt/trusted.gpg.d/maruska.asc] https://michal-maruska.github.io/apt-repo sid main' \
  | sudo tee /etc/apt/sources.list.d/maruska.list
sudo apt update
sudo apt install fork-plugin xplugin xkb-reset
```

See [apt-repo](https://github.com/michal-maruska/apt-repo) for the repository
details and caveats.

To load the plugin into a running Xorg server:

```sh
xplugin fork
```

For Wayland, install the patched `weston` (and `libinput10`) from the same
repository and start Weston as usual.

### Linux — build from source

Build dependencies on Debian/Ubuntu:

```sh
sudo apt install cmake ninja-build g++ pkg-config libx11-dev \
  xserver-xorg-dev libinput-dev libboost-dev libgtest-dev libgmock-dev
```

On other distributions, install the equivalent development packages.

Build, test and install:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
sudo cmake --install build
```

Xorg support requires an X server with the plugin pipeline patch; Weston support
requires patched libinput and Weston. See
[porting-to-weston.md](doc/porting-to-weston.md) and the legacy
[fork-install.md](doc/fork-install.md).

### Windows 10/11

Windows support is a usermode filter in the separate repository
[fork-plugin-win-hook](https://github.com/michal-maruska/fork-plugin-win-hook).
Its own documentation and installation instructions are available there.

### macOS

Not supported yet. A DriverKit/kext implementation is planned.

## Fork vs QMK

Fork is closest to QMK's
[Mod-Tap](https://github.com/qmk/qmk_firmware/blob/master/docs/mod_tap.md) and
combos, but it runs in the operating system's input stack instead of keyboard
firmware.

| | Fork | QMK |
| --- | --- | --- |
| Runs in | OS input layer (Xorg, Weston/libinput, Windows driver) | keyboard firmware |
| Hardware | any keyboard, including laptop keyboards | QMK-supported keyboard |
| Installation | install a package/driver; patched Xorg or Weston on Linux | flash firmware |
| Tap vs hold | yes | yes (Mod-Tap) |
| Chords / simultaneous keys | yes | yes (Combos) |
| Modifier triggered by mouse | yes (click) | no |
| Layers, macros, split keyboards, RGB | not the goal | yes |
| OS support | needs a frontend per OS | works on any USB host |
| Configuration | plugin/XKB configuration | `keymap.c` |

Use Fork when you want home-row modifiers on a keyboard you cannot reflash. Use
QMK when you have supported hardware and want the full firmware feature set.

## Documentation

- [What forking is and why](doc/fork.md)
- [Interaction with auto-repeat](doc/KeyRepetition.md)
- [The forking state machine](doc/fork-machine.html)
- [API notes](doc/api.md)
- [Porting to Weston / libinput](doc/porting-to-weston.md)
- [Legacy XFree86 install notes](doc/fork-install.md)
- [Known problems / notes](doc/problems)
- [License background](doc/xlicense.md)

## License

Fork is released under the [Creative Commons Attribution-ShareAlike 3.0
Unported license](https://creativecommons.org/licenses/by-sa/3.0/). See
[xlicense.md](doc/xlicense.md) for background.