# tinyheaders

Various single-file cross-platform C/C++ headers implementing self-contianed libraries.

| library | description | latest version| language(s) 
|---------|-------------|---------------|-------------
**[tinyc2](tinyc2.h)** | 2D collision detection routines on primitives, boolean results and/or manifold generation | 1.04 | C/C++
**[tinysound](tinysound.h)** | Load/play/loop/pitch/pan WAV + OGG (stb_vorbis wrapper for OGG) in mono/stereo, high performance custom mixer, decent performance custom pitch shifter (does not stretch time) | 1.08 | C/C++
**[tinynet](tinynet.h)** &ast; | Netcode for multiplayer games, reliable/unreliable packets, send/recieve large chunks reliably, encryption, network simulator, flow control, serialization + data integrity checks, compression | 0.0 | C/C++
**[tinytime](tinytime.h)** | Quick and dirty "main loop" timer function, along with utilities for integer-based high resolution timing | 1.0 | C/C++
**[tinymemfile](tinymemfile.h)** | Utility for calling fscanf-alike functions on files embedded in memory | 1.0 | C++
**[tinyfiles](tinyfiles.h)** | Directory traversal, both recursive and manual | 1.0 | C/C++
**[tinysid](tinysid.h)** | Compile time string hashing via preprocessing; turns strings into integers | 1.0 | C/C++
**[tinymath](tinymath.h)** | Professional level 3D vector math via SIMD intrinsics | 1.1 | C++
**[tinypng](tinypng.h)** | load/save PNG, texture atlas compiler, DEFLATE compliant decompressor | 1.03 | C/C++
**[tinygl](tinygl.h)** | OpenGL ES 3.0+ wrapper with carefully designed API to foster fast iteration | 1.02 | C/C++
**[tinyutf](tinyutf.h)** | utf-8 and utf-16 encoder/decoder | 1.0 | C/C++ | public domain
**[tinyhuff](tinyhuff.h)** | minimal static huffman encoder/decoder (compression) | 1.0 | C/C++ | zlib
**[tinyspheremesh](tinyspheremesh.h)** | Generates beautiful vertices (triangles) of a sphere | 1.0 | C/C++
**[tinypath](tinypath.h)** | c-string utility functions for Shlwapi.h style path manipulation | 1.01 | C/C++
**[tinyalloc](tinyalloc.h)** | straight-forward but useful allocator collection | 1.01 | C/C++
**[tinymath2d](tinymath2d.h)** | 2d vector math and shape routines | 1.0 | C++
**[tinyspritebatch](tinyspritebatch.h)** | run-time 2d sprite batcher | 1.0 | C/C++
**[tinytiled](tinytiled.h)** | Very efficient loader for Tiled maps exported to JSON format | 1.0 | C/C++

&ast; Not yet hit first release

How to Use
----------

Generally these headers do not have dependencies and are intended to be included directly into your source (check each header for specific documentation at the top of the file). Each header has a LIBNAME_IMPLEMENTATION symbol; add this to a single translation unit in your code and include the header right after in order to define library symbols. Just include the header as normal otherwise.

Examples and Tests
------------------

Some headers also have example code or demos. In this repo just look for the corresponding examples or tests folders. The example folders are particularly useful for figuring out how to use a particular header.

Contact
-------

Here's a link to the discord chat for tinyheaders. Feel free to pop in and ask questions, make suggestions, or have a discussion. If anyone has used tinyheaders it would be great to hear your experience! https://discord.gg/2DFHRmX

Another easy way to get a hold of me is on twitter [@randypgaul](https://twitter.com/RandyPGaul).

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

Unfortunately writing a good header library is pretty hard, so just any random header lib out there in the wild is probably not a good one. The [STB](https://www.google.com/url?sa=t&rct=j&q=&esrc=s&source=web&cd=1&cad=rja&uact=8&ved=0ahUKEwihsabx0qHSAhVX0WMKHVnyAZ0QFggaMAA&url=https%3A%2F%2Fgithub.com%2Fnothings%2Fstb&usg=AFQjCNHkcM-rQ-cn3VbNhQZ3lnwpnSrCWQ&sig2=bg0yIt7IhNkQy6_nMcuYZw&bvm=bv.147448319,d.cGc) and [RJM](https://github.com/rmitton/rjm) are my favorite header libs, and are a good reference to get an idea at what a good header lib looks like. [Mattias Gustavsson](https://github.com/mattiasgustavsson/libs) also has some nice libraries. [miniz](https://github.com/richgel999/miniz) is a little odd in the repository, but the releases are packed into a nice .c and .h combo -- a very useful library for creating wrapper libs!

> - *What is the license?*

Each lib contains license info at the end of the file. There is a choice between public domain, and zlib.
