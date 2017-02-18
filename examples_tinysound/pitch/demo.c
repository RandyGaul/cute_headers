#define TS_IMPLEMENTATION
#include "../../tinysound.h"

// this file was originally written by Aaron Balint, see:
// https://forums.tigsource.com/index.php?topic=58706.0
int main( )
{
#if TS_PLATFORM == TS_WINDOWS
    tsContext* ctx = tsMakeContext ( GetConsoleWindow(), 44100, 15, 5, 5 );
#else
    tsContext* ctx = tsMakeContext ( 0, 44100, 15, 5, 5 );
#endif
    tsThreadSleepDelay( ctx, 5 );
    tsLoadedSound loaded_sound = tsLoadWAV ("la.wav");
    int delay = 1000 * loaded_sound.sample_count / 44100;

    tsSpawnMixThread( ctx );
    tsPlaySoundDef def = tsMakeDef (&loaded_sound);
    
    def.pitch = 0.5;
    tsPlaySound (ctx, def);
    tsSleep (delay);
    
    def.pitch = 1.0;
    tsPlaySound (ctx, def);
    tsSleep (delay);
    
    def.pitch = 1.5;
    def.looped = 1;
    tsPlayingSound* sound1 = tsPlaySound (ctx, def);
    tsSleep (0.5*delay);
    
    def.pitch = 1.0;
    tsPlayingSound* sound2 = tsPlaySound (ctx, def);
    tsSleep (0.5*delay);
    
    def.pitch = 0.5;
    tsPlayingSound* sound3 = tsPlaySound (ctx, def);
    tsSleep (1.5*delay);
    
    for (int i=0; i<100; i++)
    {
        tsSetPitch (sound1, 1.5 - i / 100.0);
        tsSetPitch (sound2, 1.0 - 0.5 * i / 100.0);
        tsSleep (delay/100);
    }
    
    tsLoopSound (sound1, 0);
    tsLoopSound (sound2, 0);
    tsLoopSound (sound3, 0);
    tsSleep (delay);
        
	tsShutdownContext( ctx );
}
