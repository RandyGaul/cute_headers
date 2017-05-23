# tinyheaders

Various single-file cross-platform C/C++ headers implementing self-contianed libraries.

| library | description | latest version| language(s) | license
|---------|-------------|---------------|-------------|--------
**[tinyc2](tinyc2.h)** | 2D collision detection routines on primitives, boolean results and/or manifold generation | 1.02 |C/C++ | zlib
**[tinysound](tinysound.h)** | Load/play/loop/pitch/pan WAV + OGG in mono/stereo, high performance custom mixer | 1.06 | C/C++ | zlib
**[tinynet](tinynet.h)** &ast; | Netcode for multiplayer games, reliable/unreliable packets, send/recieve large chunks reliably, encryption, network simulator, flow control, serialization + data integrity checks, compression | 0.0 | C/C++ | zlib
**[tinytime](tinytime.h)** | Single function to return elapsed time delta in seconds since last call | 1.0 | C/C++ | zlib
**[tinymemfile](tinymemfile.h)** | Utility for calling fscanf-alike functions on files embedded in memory | 1.0 | C++ | zlib
**[tinyfiles](tinyfiles.h)** | Directory traversal, both recursive and manual | 1.0 | C/C++ | zlib
**[tinysid](tinysid.h)** | Compile time string hashing via preprocessing; turns strings into integers | 1.0 | C/C++ | zlib
**[tinymath](tinymath.h)** | Professional level 3D vector math via SIMD intrinsics | 1.0 | C++ | zlib
**[tinydeflate](tinydeflate.h)** | DEFLATE compliant compressor/decompressor, load/save PNG, texture atlas compiler | 1.02 | C/C++ | public domain
**[tinygl](tinygl.h)** | OpenGL wrapper with carefully designed API to foster fast iteration | 1.01 | C/C++ | zlib
**[tinyutf](tinyutf.h)** | utf-8 and utf-16 encoder/decoder | 1.0 | C/C++ | public domain
**[tinyhuff](tinyhuff.h)** | minimal static huffman encoder/decoder (compression) | 1.0 | C/C++ | zlib

&ast; Not yet hit first release

How to Use
----------

Generally these headers do not have dependencies and are intended to be included directly into your source (check each header for specific documentation at the top of the file). Each header has a LIBNAME_IMPLEMENTATION symbol; add this to a single translation unit in your code and include the header right after in order to define library symbols. Just include the header as normal otherwise.

Examples and Tests
-----------------

Some headers also have example code or demos. In this repo just look for the corresponding examples or tests folders. The example folders are particularly useful for figuring out how to use a particular header.

FAQ
---

> - *What's the point of making a single file? Why is there implementation and static functions in the headers?*

Including these headers is like including a normal header. However, to define the implementation each header looks something like this:

    #define LIBNAME_IMPLEMENTATION
    #include "libname.h"

This will turn the file into a header + c file combo, one time. The point of this is: A) handling the header or sending it to people is easy, no zip files or anything just copy and paste a single file; B) build scripts are a pain in the ass, and these single-file libs can be integrated into any project without modifying a single build script.

> - *Doesn't writing all the code in a header ruin compile times?*

The stigma that header implementations slow compile time come from inline'd code and template spam. In either case every single [translation unit](https://en.wikipedia.org/wiki/Translation_unit_(programming)) must churn through the header and place inline versions of functions, or for templates generate various type-specific functions. It gets worse once the linker kicks in and needs to coalesce translation units together, deleting duplicated symbols. Often linkers are single-threaded tasks and can really bottleneck build times.

A well constructed single-file header will not use any templates and make use of inline sparingly. Additionally well constructed single-file headers use a #define to place implementation (the function definitions and symbols) into a **single** translation unit. In this way a well crafted single-file header is pretty much the best thing a C compiler can come across, as far as build times go. Especially when the header can optionally #define out unneeded features.

> - *Aren't these header only libraries just a new fad?*

I personally don't really know if it's a fad or not, but these files aren't really just headers. They are headers with the .C file part (the implementation) attached to the end. It's two different files stuck together with the C preprocessor, but the implementation part never shows up unless the user does #define LIB_IMPLEMENTATION. This define step is the only integration step required to use these headers.

Unfortunately writing a good header library is pretty hard, so just any random header lib out there in the wild is probably not a good one. The [STB](https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=1&cad=rja&uact=8&ved=0ahUKEwihsabx0qHSAhVX0WMKHVnyAZ0QFggaMAA&url=https%3A%2F%2Fgithub.com%2Fnothings%2Fstb&usg=AFQjCNHkcM-rQ-cn3VbNhQZ3lnwpnSrCWQ&sig2=bg0yIt7IhNkQy6_nMcuYZw&bvm=bv.147448319,d.cGc) and [RJM](https://github.com/rmitton/rjm) are my favorite header libs, and are a good reference to get an idea at what a good header lib looks like.
