# How to install the Fork Extension ---- OLD VERSION

   Beware: this is old document. For interested people I recommend using precompiled packages

   Other pages related to "forking" are Fork homepage, fork machine. See also medved documentation.

   Explanation of the pipeline from 2005 on the XFree86 ML.

  Summary:

     o linux kernel patch
     o download Xfree86 CVS sources & my patch
     o configure & build
     o build the special Forking plugin
     o start testing & using it.

   After following these steps, you will be able to use "forking".

   In the following code, I use zsh as shell.

  you need a kernel patch.

   Here's an explanation http://lkml.org/lkml/2005/10/6/92

   Maybe netbsd has evdev equivalent: see "nanotime(&ev->time);" in wskbd.c. Documentation is in wsconsio.h

     o patches:

   2.6.12.5 patch

   2.6.14-rc3 http://lkml.org/lkml/diff/2005/10/6/92/1

   2.6.15-rc7 patch

   2.6.16.9 patch (no more need for patching kernel/posix-timers.c)

 cd /usr/src/linux;  patch -p1 < the-saved-file.patch
 make menuconfig
        -> enable   Device Drivers > Input > Event Interface  (as Module)
                   and                        > USB  > USB Human Interface Device (full HID) support
           (or just ps/2 keyboard)

   Unfortunately, current kernel "core" does not export do_posix_clock_monotonic_gettime, so modules cannot use that
   function to obtain the monotonic time, so you will have to (re)boot a kernel with my patch applied. If it were
   exported you could simply reload the patched evdev module.


   note: Besides installing modules & bzImage, you need to install the patched header file before building XFree86:

 cp -v /usr/src/linux/include/linux/input.h  /usr/include/linux/input.h

   You can compile all the following programs without running the new kernel. All that is needed is aforementioned
   /usr/include/linux/input.h.

  get the CVS XFree86:

   (official instruction http://www.xfree86.org/cvs/)

   Checkout the CVS source tree, and create a new directory to build in. Let's leave the source directory tree intact.

   mkdir xfree86
   cd xfree86
   cvs -d anoncvs@anoncvs.xfree86.org:/cvs login
        password "anoncvs"
   cvs -d anoncvs@anoncvs.xfree86.org:/cvs co xc

   (maybe read the xc/README)
   To build in a separate directory:

   mkdir build
   cd build
   lndir ../xc

  apply my patches for XFree86 and then build:

   Get the patch file and apply it inside the symlink tree, without overwriting original files:

 $ ls
 xc build

   wget http://maruska.dyndns.org/comp/packages/medved-pipeline.patch

 cd build
 patch --backup -p0 < ../medved-pipeline.patch

   I do insted:

 - cp -a --remove-destination  -v ../xpatches/medved-plugin/*  .

   Then get the configuration and adapt to your needs:
   If you have your config file, just add:

 #define ExtraXInputDrivers medved
 #define BuildXKBPlugin   YES
 #define HasMonotonicTime YES


  ...othwerwise you need to create it:

   wget http://maruska.dyndns.org/comp/packages/host.def -O build/config/cf/host.def

   edit config/cf/host.def , in particular:

     o change the gcc flags .. CPU etc.
     o add your graphics card's driver to the list of desired backends
     o notice, that the stuff will be installed into /usr/xfree-4.6. In the end you will probably need to create
       /usr/X11R6 (& /usr/bin/X11 & /usr/lib/X11) symlinks.

 cd build
 (time make World >& world.log) &> time.log &
 or simply:
 make world

   Then install:

 make install
 make install.man
 make -k install.sdk     (seems problematic, hence the "-k"; not my fault though)

    the problem:

 make[5]: Entering directory `/x/cvs/xfree/build/programs/Xserver/mfb/module'
 install -c libmfb.a /usr/xfree86-4.6/lib/Server/modules/.
 install -c -m 0444 mfb.h /usr/xfree86-4.6/lib/Server/include
 install: cannot stat `mfb.h': No such file or directory
 make[5]: *** [install.sdk] Error 1
 make[5]: Leaving directory `/x/cvs/xfree/build/programs/Xserver/mfb/module'

   mfb.h is in /x/build/programs/Xserver/mfb/mfb.h and other 7 such problems!

   or


 installing driver SDK in programs/Xserver/hw/xfree86/os-support/linux...
 make[7]: Entering directory `/x/xfree86/build/programs/Xserver/hw/xfree86/os-support/linux'
 install -c -m 0444 agpgart.h /usr/xfree86-4.6/lib/Server/include/linux
 install: impossibile fare stat di `agpgart.h': No such file or directory

 <example>


 #plugin
 ** Building the plugin & tools

 The patched XFree86 build will now use a different (more flexible) key handling, and the library is able to
 configure it.
 Now we build a plugin, which changes integrates with the new key handling system, and 2 packages which use the
 library calls to activate the plugin and to configure specifically that plugin.


 If you have portage (gentoo), the 3 packages can be built by "emerging" 2 packages:

 See how to include my ebuilds with [[portage]]:
 <example>
 gensync mmc
 (btw. I have alias emd="FEATURES=digest  emerge")
 $ FEATURES=digest  emerge  xkb-plugin xkb-tools

   if you don't have gentoo, do manually what the ebuilds would do:

    1 Build the "fork" plugin:

 wget http://maruska.dyndns.org/comp/packages/xkb-plugin-2.7.tar.gz
 tar -xzf xkb-plugin-2.7.tar.gz
 cd xkb-plugin
 xmkmf
 make
 make install

   This will install:

     o /usr/xfree86-4.6/lib/modules/xkb-plugins/fork.so
     o /usr/xfree86-4.6/include/X11/extensions/fork_requests.h

    2 Build "xplugin" program.

   and

    3 Build "libfork": a library for configuring the fork plugin

   wget http://maruska.dyndns.org/comp/packages/xkb-tools-1.7.tar.gz

 tar -xzf xkb-tools-1.7.tar.gz
 cd xkb-tools
 cd libfork
 xmkmf && make && make install
 cd ../xplugin;
 xmkmf && make && make install


   note: i wrote the Imakefiles with the help of xc/config/cf/Imake.rules

  How to configure the Fork plugin

   Again, using gentoo/portage is a big plus:

   The libX11.so built contains 4 new calls, XkbSetPlugin and XkbPluginConfigure XkbPluginGetConfigur and
   XkbPluginCommand.

 (nm -g /usr/xfree86-4.6/lib/libX11.so |grep 'xkb.*plugin')

   Libfork translates higher-level requests for the "Fork" plugin into request for the XkbPluginConfigure. But to
   create a sufficiently expressive configuration language I used a scheme interpreter.

   I use gauche (a scheme interpreter) and my gauche-xlib. So, you have to install Gauche. And my gauche-xlib
   extension.

   emerge \=gauche-mmc-2 \=gauche-xlib-1.6

   People without gentoo first have to install Gauche, then

   wget http://maruska.dyndns.org/comp/packages/gauche-mmc-2.tar.gz
   wget http://maruska.dyndns.org/comp/packages/gauche-xlib-1.6.tar.gz

   untar ...

 cd gauche-mmc;    make install
 cd ../gauche-xlib;  ./auto; make ; make install

   note: gauche-mmc is full of crap. and gauche-xlib is binding to Xlib calls related to XKB, no drawing primitives!

  and the final step!

   wget http://maruska.dyndns.org/comp/activity/gauche/xlib/xkb/config-fork.scm
   and run it config-fork.scm

   But before runnning you should read that file :( It's my exact configuration: all keys on the "home row" fork! And
   the modifiers are on both left & right sides (not symetrically though).

   Also, to take advantage of the Group+1 and Group+2 modifiers I have a lot of keysyms bound to letter keys.

   Such configuration is not provided!


the text bellow is .... under (re) construction

   ÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍÍ

   To fine tune the time parameters, i use a GUI program. And I also have a visualizer to see how I typed the recent
   keys.

problems with installing XFree86 under gentoo:

     o freetype2 ?
     o gauche-gtk pango over cairo unexpected!

How to start a new X session:

     o XFree86 -ac :1
       or export DISPLAY=:1; XFree86 -ac :1 & sleep 3;xterm
     o better, startx -- :1 -ac
       This runs ~/.xinitrc -> WM is available.
     o xinit ???

Bugs:

  to 2GB log & crash:

   (EE) Incomplete event read: No such device

  amd optim on p4 ?

  ** kbdraw

   http://developer.gnome.org/tools/svn.html
