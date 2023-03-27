# cute_headers

Various single-file cross-platform C/C++ headers implementing self-contained libraries.

| library | description | latest version| language(s)
|---------|-------------|---------------|-------------
**[cute_c2](cute_c2.h)** | 2D collision detection routines on primitives, boolean results and/or manifold generation, shape cast/sweep test, raycasts | 1.10 | C/C++
**[cute_net](cute_net.h)** | Networking library for games requiring an optional reliability layer over UDP with a baked in security scheme | 1.03 | C/C++
**[cute_tiled](cute_tiled.h)** | Very efficient loader for Tiled maps exported to JSON format | 1.07 | C/C++
**[cute_aseprite](cute_aseprite.h)** | Parses .ase/.aseprite files into a compact and convenient struct collection | 1.02 | C/C++
**[cute_sound](cute_sound.h)** | Load/play/loop (with plugin)/pan WAV + OGG (stb_vorbis wrapper for OGG) in mono/stereo, high performance custom mixer, music + crossfade support | 2.04 | C/C++
**[cute_math](cute_math.h)** | Professional level 3D vector math via SSE intrinsics | 1.02 | C++
**[cute_png](cute_png.h)** | load/save PNG, texture atlas compiler, DEFLATE compliant decompressor | 1.05 | C/C++
**[cute_spritebatch](cute_spritebatch.h)** | Run-time 2d sprite batcher. Builds atlases on-the-fly in-memory. Useful to implement a sprite batcher for any purpose (like 2D games) for high-performance rendering, without the need to precompile texture atlases on-disk. | 1.04 | C/C++
**[cute_sync](cute_sync.h)** | Collection of practical synchronization primitives, including read/write lock and threadpool/task system | 1.01 | C/C++

How to Use
----------

Generally these headers do not have dependencies and are intended to be included directly into your source (check each header for specific documentation at the top of the file). Each header has a LIBNAME_IMPLEMENTATION symbol; add this to a single translation unit in your code and include the header right after in order to define library symbols. Just include the header as normal otherwise.

Examples and Tests
------------------

Some headers also have example code or demos. In this repo just look for the corresponding examples or tests folders. The example folders are particularly useful for figuring out how to use a particular header.

Contact
-------

Here's a link to the discord chat for cute_headers. Feel free to pop in and ask questions, make suggestions, or have a discussion. If anyone has used cute_headers it would be great to hear your experience! https://discord.gg/2DFHRmX

Another easy way to get a hold of me is on twitter [@randypgaul](https://twitter.com/RandyPGaul).

FAQ
---

> - *What's the point of making a single file? Why is there implementation and static functions in the headers?*

Including these headers is like including a normal header. However, to define the implementation each header looks something like this:

```c
// Do this ONCE in a .c/.cpp file
#define LIBNAME_IMPLEMENTATION
#include "libname.h"

// Everywhere else, just include like a typical header
#include "libname.h"
```

This will turn the file into a header + c file combo, one time. The point of this is: A) handling the header or sending it to people is easy, no zip files or anything just copy and paste a single file; B) build scripts are a pain in the ass, and these single-file libs can be integrated into any project without modifying a single build script.

> - *Doesn't writing all the code in a header ruin compile times?*

The stigma that header implementations slow compile time come from inline'd code and template spam. In either case every single [translation unit](https://en.wikipedia.org/wiki/Translation_unit_(programming)) must churn through the header and place inline versions of functions, or for templates generate various type-specific functions. It gets worse once the linker kicks in and needs to coalesce translation units together, deleting duplicated symbols. Often linkers are single-threaded tasks and can really bottleneck build times.

A well constructed single-file header will not use any templates and make use of inline sparingly. Additionally well constructed single-file headers use a #define to place implementation (the function definitions and symbols) into a **single** translation unit. In this way a well crafted single-file header is pretty much the best thing a C compiler can come across, as far as build times go. Especially when the header can optionally #define out unneeded features.

> - *Aren't these header only libraries just a new fad?*

I personally don't really know if it's a fad or not, but these files aren't really just headers. They are headers with the .C file part (the implementation) attached to the end. It's two different files stuck together with the C preprocessor, but the implementation part never shows up unless the user does #define LIB_IMPLEMENTATION. This define step is the only integration step required to use these headers.

Unfortunately writing a good header library is pretty hard, so just any random header lib out there in the wild is probably not a good one. The [STB](https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=1&cad=rja&uact=8&ved=0ahUKEwihsabx0qHSAhVX0WMKHVnyAZ0QFggaMAA&url=https%3A%2F%2Fgithub.com%2Fnothings%2Fstb&usg=AFQjCNHkcM-rQ-cn3VbNhQZ3lnwpnSrCWQ&sig2=bg0yIt7IhNkQy6_nMcuYZw&bvm=bv.147448319,d.cGc) and [RJM](https://github.com/rmitton/rjm) are pretty good header libs, and are a good reference to get an idea at what a good header lib looks like. [Mattias Gustavsson](https://github.com/mattiasgustavsson/libs) has my favorite collection of headers.

> - *What is the license?*

Each lib contains license info at the end of the file. There is a choice between public domain, and zlib.

> - *I was looking for a header I've seen before, but it's missing. Where did it go?*

Some of the unpopular or not so useful headers became deprecated, and [live here now](https://github.com/RandyGaul/cute_headers_deprecated).

> - *Do you have any higher level libraries? These seem a bit too low-level.

The cute headers are indeed rather low-level. They solve specific problems. If you're looking for a higher level game creation framework I suggestion trying out [Cute Framework](https://github.com/RandyGaul/cute_framework), a 2D game creation framework built largely on-top of the various low-level cute headers seen here.
