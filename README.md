# QEMU Extras
Old PC Games Addons for QEMU (and DOSBox)
## Content
    openglide - OpenGLide fork optimized for QEMU (Support Glide2x & Glide3x)
    g2xwrap   - GLIDE.DLL & GLIDE3X.DLL that wrap into Glide2x APIs
    dosbox    - DOSBox SVN Games essentials

# Host & Guest OpenGlide Wrappers for QEMU-3dfx
Behold, One of kjliew's other addons that adds Host Openglide support into one of your QEMU-3dfx machines. and maybe guest Openglide too?

## Building OpenGLide
    $ mkdir ~/myxtra && cd ~/myxtra
    $ git clone https://github.com/kjliew/qemu-xtra.git
    $ cd qemu-xtra/openglide
    $ bash ./bootstrap
    $ mkdir ../build && cd ../build
    $ ../openglide/configure --disable-sdl && make

## Building G2Xwrap 
Requires OpenGLide compiled first!

    $ mkdir ~/myxtra && cd ~/myxtra
    $ git clone https://github.com/kjliew/qemu-xtra.git
    $ cd qemu-xtra/openglide
    $ bash ./bootstrap
    $ mkdir ../build && cd ../build
    $ ../openglide/configure --disable-sdl && make
    $ ../g2xwrap
    $ make

# DOSbox-3dfx
This repository also contains patches that adds 3dfx passthrough support for DOSbox (requires DOSbox-SVN)

