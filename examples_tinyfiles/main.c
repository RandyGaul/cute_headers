#include <Windows.h>

#define TINYFILES_IMPLEMENTATION
#include "tinyfiles.h"

#include <stdio.h>

void PrintDir( tfFILE* file, void* udata )
{
	printf( "name: %-10s\text: %-10s\tpath: %s\n", file->name, file->ext, file->path );
	*(int*)udata += 1;
}

void TestDir( )
{
	int n = 0;
	tfTraverse( ".", PrintDir, &n );
	printf( "fFund %d files with tfTraverse\n\n", n );

	tfDIR dir;
	tfDirOpen( &dir, "a" );

	while ( dir.has_next )
	{
		tfFILE file;
		tfReadFile( &dir, &file );
		printf( "%s\n", file.name );
		tfDirNext( &dir );
	}
	
	tfDirClose( &dir );
}


int main( )
{
	TestDir( );
	return 0;
}
