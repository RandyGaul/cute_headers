# tinyheaders

Various single-file C/C++ headers implementing self-contianed libraries.

| library | description | latest version
|---------|-------------|---------------
**[tinysound](tinysound.h)** | Load/play/loop/pitch/pan WAV + OGG in mono/stereo, high performance custom mixer | 1.05
**[tinynet](tinynet.h)** | Netcode for multiplayer games, reliable/unreliable packets, send/recieve large chunks reliably, encryption, network simulator | 0.0
**[tinytime](tinytime.h)** | Single function to return elapsed time delta in seconds since last call | 1.0

How to Use
----------

Generally these headers do not have dependencies and are intended to be included directly into your source (check each header for specific documentation at the top of the file). Each header has a LIBNAME_IMPLEMENTATION symbol; add this to a single translation unit in your code and include the header right after in order to define library symbols. Just include the header as normal otherwise.

Examples and Tests
------------------

Some headers also have example code or demos. In this repo just look for the corresponding examples or tests folders. The example folders are particularly useful for figuring out how to use a particular header.
