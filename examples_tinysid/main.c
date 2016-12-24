#include <Windows.h>

#define TINYSID_IMPL
#define TINYFILES_IMPL
#include "tinysid.h"
#include "tinyfiles.h"

#include <string.h>
#include <stdlib.h>

char* strcatdup( const char* a, const char* b )
{
    int len_a = strlen( a );
    int len_b = strlen( b );
    char* c = malloc( len_a + len_b + 1 );
    memcpy( c, a, len_a );
    memcpy( c + len_a, b, len_b + 1 );
    return c;
}

void CB_DoPreprocess( tfFILE* file, void* udata )
{
	(void)udata;
	char* out = strcatdup( file->path, ".preprocessed" );
	tsPreprocess( file->path, out );
	free( out );
}

int main( int argc, const char** argv )
{
	if ( argc != 2 )
	{
		printf( "Incorrect parameter usage. Should only pass the path to source directory.\n" );
		return -1;
	}

	tfTraverse( argv[ 1 ], CB_DoPreprocess, 0 );
	return 0;
}