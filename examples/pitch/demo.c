#define TS_IMPLEMENTATION
#include "tinysound.h"

// this file was written by Aaron Balint, see:
// https://forums.tigsource.com/index.php?topic=58706.0
int main( )
{
    tsContext* ctx = tsMakeContext (GetConsoleWindow(), 44100, 15, 5, 5);
    tsSpawnMixThread( ctx );
    tsLoadedSound loaded_sound = tsLoadWAV ("la.wav");
    long delay = 1000 * loaded_sound.sample_count / 44100;
    
    tsPlaySoundDef def = tsMakeDef (&loaded_sound);
    
    def.pitch = 0.5;
    tsPlaySound (ctx, def);
    Sleep (delay);
    
    def.pitch = 1.0;
    tsPlaySound (ctx, def);
    Sleep (delay);
    
    def.pitch = 1.5;
    def.looped = 1;
    tsPlayingSound* sound1 = tsPlaySound (ctx, def);
    Sleep (0.5*delay);
    
    def.pitch = 1.0;
    tsPlayingSound* sound2 = tsPlaySound (ctx, def);
    Sleep (0.5*delay);
    
    def.pitch = 0.5;
    tsPlayingSound* sound3 = tsPlaySound (ctx, def);
    Sleep (1.5*delay);
    
    for (int i=0; i<100; i++)
    {
        tsSetPitch (sound1, 1.5 - i / 100.0);
        tsSetPitch (sound2, 1.0 - 0.5 * i / 100.0);
        Sleep (delay/100);
    }
    
    tsLoopSound (sound1, 0);
    tsLoopSound (sound2, 0);
    tsLoopSound (sound3, 0);
    Sleep (delay);
        
	tsShutdownContext( ctx );
}
