# tinyheaders

Various single-file cross-platform C/C++ headers implementing self-contianed libraries.

| library | description | latest version| language(s)
|---------|-------------|---------------|------------
**[tinysound](tinysound.h)** | Load/play/loop/pitch/pan WAV + OGG in mono/stereo, high performance custom mixer | 1.05 | C/C++
**[tinynet](tinynet.h)** &ast; | Netcode for multiplayer games, reliable/unreliable packets, send/recieve large chunks reliably, encryption, network simulator, flow control | 0.0 | C/C++
**[tinytime](tinytime.h)** | Single function to return elapsed time delta in seconds since last call | 1.0 | C/C++
**[tinymemfile](tinymemfile.h)** | Utility for calling fscanf-alike functions on files embedded in memory | 1.0 | C++

&ast; Not yet hit first release

How to Use
----------

Generally these headers do not have dependencies and are intended to be included directly into your source (check each header for specific documentation at the top of the file). Each header has a LIBNAME_IMPLEMENTATION symbol; add this to a single translation unit in your code and include the header right after in order to define library symbols. Just include the header as normal otherwise.

Examples and Tests
------------------

Some headers also have example code or demos. In this repo just look for the corresponding examples or tests folders. The example folders are particularly useful for figuring out how to use a particular header.
