// create header + implementation
#define TS_IMPLEMENTATION
#include "tinysound.h"

#include <stdio.h>

int main( )
{
	HWND hwnd = GetConsoleWindow( );
	tsContext* ctx = tsMakeContext( hwnd, 44000, 15, 5, 0 );
	tsLoadedSound loaded = tsLoadWAV( "../jump.wav" );
	tsPlayingSound jump = tsMakePlayingSound( &loaded );

	printf( "Press space!\n" );
	tsInsertSound( ctx, &jump );

	while ( 1 )
	{
		if ( GetAsyncKeyState( VK_ESCAPE ) )
			break;

		if ( GetAsyncKeyState( VK_SPACE ) )
			tsInsertSound( ctx, &jump );

		tsMix( ctx );
	}

	tsFreeSound( &loaded );
}
