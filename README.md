tinysound is a single-header C API for manipulating and playing sounds through DirectSound. tinysound is mainly intended for use in games. DirectSound implies Windows support only. Why only Windows? Since I last checked the Steam survey over 95% of users were running a Windows operating system.

tinysound has no dependencies other than the C-standard library (memset, memcmp, malloc, free, sqrtf, fopen, fclose) and dsound.dll/dsound.lib. tinysound does not even have to include dsound.h (details below)!

Please view the header tinysound.h for detailed documentation. For now here's a quick example of loading in some sounds and playing them:

    void LowLevelAPI( tsContext* ctx )
    {
        tsLoadedound airlock = tsLoadWAV( "airlock.wav" );
        tsLoadedound jump = tsLoadWAV( "jump.wav" );
        tsPlayingSound s0 = tsMakePlayingSound( &airlock );
        tsPlayingSound s1 = tsMakePlayingSound( &jump );
        tsLoopSound( &s0, 1 );
        tsInsertSound( ctx, &s0 );
    
        while ( 1 )
        {
            if ( GetAsyncKeyState( VK_ESCAPE ) )
                break;
    
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
        return 0;
    }

Just include tinysound.h into your project and you're good to go! Be sure to do this somewhere in your project to define the function definitions inside of tinysound.h:

    #define TS_IMPLEMENTATION
    #include "tinysound.h"

Sometimes it is very annoying to have to include dsound.h, and in the event users do *not* want to include this file (for any reason, like compile times, or just preference) feel free to define TS_USE_DSOUND_HEADER as 0 inside of tinysound.h. This will remove the dsound.h header dependency by pulling in only the minimal necessary header symbols from dsound.h and windows.h.
