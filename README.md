# tinyheaders

Various single-file cross-platform C/C++ headers implementing self-contianed libraries.

| library | description | latest version| language(s) | license
|---------|-------------|---------------|-------------|--------
**[tinysound](tinysound.h)** | Load/play/loop/pitch/pan WAV + OGG in mono/stereo, high performance custom mixer | 1.05 | C/C++ | zlib
**[tinynet](tinynet.h)** &ast; | Netcode for multiplayer games, reliable/unreliable packets, send/recieve large chunks reliably, encryption, network simulator, flow control, serialization + data integrity checks, compression | 0.0 | C/C++ | zlib
**[tinytime](tinytime.h)** | Single function to return elapsed time delta in seconds since last call | 1.0 | C/C++ | zlib
**[tinymemfile](tinymemfile.h)** | Utility for calling fscanf-alike functions on files embedded in memory | 1.0 | C++ | zlib
**[tinyfiles](tinyfiles.h)** | Directory traversal, both recursive and manual | 1.0 | C/C++ | zlib
**[tinysid](tinysid.h)** | Compile time string hashing via preprocessing; turns strings into integers | 1.0 | C/C++ | zlib
**[tinymath](tinymath.h)** | Professional level 3D vector math via SIMD intrinsics | 1.0 | C++ | zlib
**[tinydeflate](tinydeflate.h)** | DEFLATE compliant compressor/decompressor, load/save PNG, texture atlas compiler | 1.0 | C/C++ | public domain
**[tinygl](tinygl.h)** | OpenGL wrapper with carefully designed API to foster fast iteration | 1.0 | C/C++ | zlib
**[tinyc2](tinyc2.h)** &ast; | 2D collision detection routines on primitives, boolean results and/or manifold generation | 0.0 | C/C++ | zlib

&ast; Not yet hit first release

How to Use
----------

Generally these headers do not have dependencies and are intended to be included directly into your source (check each header for specific documentation at the top of the file). Each header has a LIBNAME_IMPLEMENTATION symbol; add this to a single translation unit in your code and include the header right after in order to define library symbols. Just include the header as normal otherwise.

Examples and Tests
------------------

Some headers also have example code or demos. In this repo just look for the corresponding examples or tests folders. The example folders are particularly useful for figuring out how to use a particular header.
