#include <Windows.h>

#define _CRT_SECURE_NO_WARNINGS
#include "SDL2/SDL.h"

#define TINYSOUND_IMPLEMENTATION
#define TINYSOUND_FORCE_SDL
#include "../../tinysound.h"

int main( int argc, char *args[] )
{
	tsContext* ctx = tsMakeContext( 0, 44100, 15, 5, 0 );
	tsLoadedSound loaded = tsLoadWAV( "../jump.wav" );

	// Demo of loading through a SDL_RWops object.
	//SDL_RWops* file = SDL_RWFromFile( "../jump.wav", "rb");
	//tsLoadedSound loaded = tsLoadWAVRW(file);

	tsPlayingSound jump = tsMakePlayingSound( &loaded );
	tsSpawnMixThread( ctx );

	printf( "Jump ten times...\n" );
	tsSleep( 500 );

	int count = 10;
	while ( count-- )
	{
		tsSleep( 500 );
		tsInsertSound( ctx, &jump );
		printf( "Jump!\n" );
	}

	tsSleep( 500 );
	tsFreeSound( &loaded );

	return 0;
}
