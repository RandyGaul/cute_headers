tinysound is a single-header C API for manipulating and playing sounds. tinysound is mainly intended for use in games.

Features:
- Load WAV and OGG files from disk or from memory (stb_vorbis for OGG support)
- Loop/Play/Pause/Stop sounds, also a delayed play feature
- Mono/stereo panning/volume support. Users can layer onto this for 3D sound synthesis, fading, or attenuation as desired
- Music and sound effects are all treated the same way; no special cases
- Internal memory pool for playing sound instances (also explicit memory handling alternative)
- Readable error reporting through `g_tsErrorReason`
- High performance custom mixer using SIMD intrinsics (roughly 3.5x speadup over well-implemented scalar mixers)
- All playing sounds are instanced, so the same sound file can be played any number of times simultaneously
- Gapless looping
- Can spawn separate thread to run tsMix with tsSpawnMixThread
- Real-time pitch shifting (without adjusting sound length)

Currently tinysound only supports DirectSound, which implies Windows support only. Why only Windows? Since I last checked the Steam survey over 95% of users were running a Windows operating system, and I haven't yet had time to do a CoreAudio port for OSX/iOS. Please see the Issues tab if anyone is interested in contributing a CoreAudio port -- not much code would need to be mofidied as tinysound is port-ready!

tinysound has no dependencies other than the C-standard library (memset, memcmp, malloc, free, sqrtf, fopen, fclose) and dsound.dll/dsound.lib. tinysound does not even have to include dsound.h on Windows (details below)!

Please view the header tinysound.h for detailed documentation. For now here's a quick example of loading in some sounds and playing them:

```c++
    #define TS_IMPLEMENTATION
    #include "tinysound.h"
    
    void LowLevelAPI( tsContext* ctx )
    {
        // load a couple sounds
        tsLoadedSound airlock = tsLoadWAV( "airlock.wav" );
        tsLoadedSound jump = tsLoadWAV( "jump.wav" );
    
        // make playable instances
        tsPlayingSound s0 = tsMakePlayingSound( &airlock );
        tsPlayingSound s1 = tsMakePlayingSound( &jump );
    
        // setup a loop and play it
        tsLoopSound( &s0, 1 );
        tsInsertSound( ctx, &s0 );
    
        while ( 1 )
        {
            if ( GetAsyncKeyState( VK_ESCAPE ) )
                break;
    
            // play the sound
            if ( GetAsyncKeyState( VK_SPACE ) )
                tsInsertSound( ctx, &s1 );
    
            tsMix( ctx );
        }
    }
    
    int main( )
    {
        HWND hwnd = GetConsoleWindow( );
        tsContext* ctx = tsMakeContext( hwnd, 48100, 15, 1, 0 );
        LowLevelAPI( );
        tsShutdownContext( ctx );
        return 0;
    }
```

Just include tinysound.h into your project and you're good to go! Be sure to do this somewhere in your project to define the function definitions inside of tinysound.h:

```c++
    #define TS_IMPLEMENTATION
    #include "tinysound.h"
```

Sometimes it is very annoying to have to include dsound.h, and in the event users do *not* want to include this file (for any reason, like compile times, or just preference) feel free to define TS_USE_DSOUND_HEADER as 0 inside of tinysound.h. This will remove the dsound.h header dependency by pulling in only the minimal necessary header symbols from dsound.h and windows.h.
