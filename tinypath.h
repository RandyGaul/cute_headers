/*
	tinypath.h - v1.00


	To create implementation (the function definitions)
		#define TINYPATH_IMPL
	in *one* C/CPP file (translation unit) that includes this file


	SUMMARY:

		Collection of c-string manipulation functions for dealing with common file-path
		operations. More or less a less-featuerd replacement for Shlwapi.h path functions
		on Windows.

		Performs no dynamic memory management and has no external dependencies (other than
		some crt funcs).


	Revision history:
		1.0  (11/01/2017) initial release
*/

#if !defined( TINYPATH_H )

#define TP_MAX_PATH 1024
#define TP_MAX_EXT 32

// Copies path to out, but not the extension. Places a nul terminator in out.
// Returns the length of the string in out, excluding the nul byte.
// Length of copied output can be up to TP_MAX_PATH. Can also copy the file
// extension into ext, up to TP_MAX_EXT.
#if !defined( __cplusplus )
int tpPopExt( const char* path, char* out, char* ext );
#else
int tpPopExt( const char* path, char* out = 0, char* ext = 0 );
#endif

// Copies path to out, but excludes the final file or folder from the output.
// If the final file or folder contains a period, the file or folder will
// still be appropriately popped. If the path contains only one file or folder,
// the output will contain a period representing the current directory. All
// outputs are nul terminated.
// Returns the length of the string in out, excluding the nul byte.
// Length of copied output can be up to TP_MAX_PATH.
// Optionally stores the popped filename in pop. pop can be NULL.
// out can also be NULL.
#if !defined( __cplusplus )
int tpPop( const char* path, char* out, char* pop );
#else
int tpPop( const char* path, char* out = 0, char* pop = 0 );
#endif

// Concatenates path_b onto the end of path_a. Will not write beyond max_buffer_length.
// Places a single '/' character between path_a and path_b. Does no other "intelligent"
// manipulation of path_a and path_b; it's a basic strcat kind of function.
void tpConcat( const char* path_a, const char* path_b, char* out, int max_buffer_length );

// Copies the name of the folder the file sits in (but not the entire path) to out. Will
// not write beyond max_buffer_length. Length of copied output can be up to TP_MAX_PATH.
// path contains the full path to the file in question.
// Returns 0 for inputs of "", "." or ".." as the path, 1 otherwise (success).
int tpNameOfFolderImIn( const char* path, char* out );

// Some useful (but not yet implemented) functions
/*
	int tpRoot( const char* path, char* out );
	int tpCompact( const char* path, char* out );
*/

#define TP_UNIT_TESTS 1
void tpDoUnitTests();

#define TINYPATH_H
#endif

#if defined( TINYPATH_IMPL )

#ifdef _WIN32

	#if !defined( _CRT_SECURE_NO_WARNINGS )
		#define _CRT_SECURE_NO_WARNINGS
	#endif

#endif

#include <string.h> // strncpy, strlen
#define TP_STRNCPY strncpy
#define TP_STRLEN strlen

int tpIsSlash( char c )
{
	return ( c == '/' ) | ( c == '\\' );
}

int tpPopExt( const char* path, char* out, char* ext )
{
	int initial_skipped_periods = 0;
	while ( *path == '.' )
	{
		++path;
		++initial_skipped_periods;
	}

	const char* period = path;
	char c;
	while ( ( c = *period++ ) ) if ( c == '.' ) break;
	int len = (int)( period - path ) - 1 + initial_skipped_periods;
	if ( len > TP_MAX_PATH - 1 ) len = TP_MAX_PATH - 1;
	if ( out )
	{
		TP_STRNCPY( out, path - initial_skipped_periods, len );
		out[ len ] = 0;
	}
	if ( ext )TP_STRNCPY( ext, path - initial_skipped_periods + len + 1, TP_MAX_EXT );
	return len;
}

int tpPop( const char* path, char* out, char* pop )
{
	const char* original = path;
	int total_len = 0;
	while ( *path )
	{
		++total_len;
		++path;
	}

	// ignore trailing slash from input path
	if ( tpIsSlash( *(path - 1) ) )
	{
		--path;
		total_len -= 1;
	}

	int pop_len = 0; // length of substring to be popped
	while ( !tpIsSlash( *--path ) && pop_len != total_len )
		++pop_len;
	int len = total_len - pop_len; // length to copy

	if ( len > 0 )
	{
		if ( out )
		{
			TP_STRNCPY( out, original, len );
			out[ len ] = 0;
		}

		if ( pop )
		{
			TP_STRNCPY( pop, path + 1, pop_len );
			pop[ pop_len ] = 0;
		}

		return len;
	}

	else
	{
		if ( out )
		{
			out[ 0 ] = '.';
			out[ 1 ] = 0;
		}
		if ( pop ) *pop = 0;
		return 1;
	}
}

static int tpStrnCpy( char* dst, const char* src, int n, int max )
{
	int c;
	const char* original = src;

	do
	{
		if ( n >= max - 1 )
		{
			dst[ max - 1 ] = 0;
			break;
		}
		c = *src++;
		dst[ n ] = c;
		++n;
	} while ( c );

	return n;
}

void tpConcat( const char* path_a, const char* path_b, char* out, int max_buffer_length )
{
	int n = tpStrnCpy( out, path_a, 0, max_buffer_length );
	n = tpStrnCpy( out, "/", n - 1, max_buffer_length );
	tpStrnCpy( out, path_b, n - 1, max_buffer_length );
}

int tpNameOfFolderImIn( const char* path, char* out )
{
	// return failure for empty strings and "." or ".."
	if ( !*path || *path == '.' && TP_STRLEN( path ) < 3 ) return 0;
	int len = tpPop( path, out, NULL );
	int has_slash = 0;
	for ( int i = 0; out[ i ]; ++i )
	{
		if ( tpIsSlash( out[ i ] ) )
		{
			has_slash = 1;
			break;
		}
	}

	if ( has_slash )
	{
		int n = tpPop( out, out, NULL ) + 1;
		len -= n;
		TP_STRNCPY( out, path + n, len );
	}

	else TP_STRNCPY( out, path, len );
	out[ len ] = 0;
	return 1;
}

#if TP_UNIT_TESTS

	#include <stdio.h>

	#define TP_STRCMP strcmp
	#define TP_EXPECT(X) do { if ( !(X) ) printf( "Failed tinyfiles.h unit test at line %d of file %s.\n", __LINE__, __FILE__ ); } while ( 0 )

	void tpDoUnitTests()
	{
		char out[ TP_MAX_PATH ];

		const char* path = "../root/file.ext";
		tpPopExt( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "../root/file" ) );

		tpPop( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "../root" ) );

		path = "../root/file";
		tpPopExt( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "../root/file" ) );

		tpPop( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "../root" ) );

		path = "../root/";
		tpPopExt( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "../root/" ) );

		tpPop( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, ".." ) );

		path = "../root";
		tpPopExt( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "../root" ) );

		tpPop( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, ".." ) );

		path = "../";
		tpPopExt( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "../" ) );

		tpPop( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "." ) );

		path = "..";
		tpPopExt( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, ".." ) );

		tpPop( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "." ) );

		path = ".";
		tpPopExt( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "." ) );

		tpPop( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "." ) );

		path = "";
		tpPopExt( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "" ) );

		tpPop( path, out, NULL );
		TP_EXPECT( !TP_STRCMP( out, "." ) );

		path = "asdf/file.ext";
		tpNameOfFolderImIn( path, out );
		TP_EXPECT( !TP_STRCMP( out, "asdf" ) );

		path = "asdf/lkjh/file.ext";
		tpNameOfFolderImIn( path, out );
		TP_EXPECT( !TP_STRCMP( out, "lkjh" ) );

		path = "poiu/asdf/lkjh/file.ext";
		tpNameOfFolderImIn( path, out );
		TP_EXPECT( !TP_STRCMP( out, "lkjh" ) );

		path = "poiu/asdf/lkjhqwer/file.ext";
		tpNameOfFolderImIn( path, out );
		TP_EXPECT( !TP_STRCMP( out, "lkjhqwer" ) );

		path = "../file.ext";
		tpNameOfFolderImIn( path, out );
		TP_EXPECT( !TP_STRCMP( out, ".." ) );

		path = "./file.ext";
		tpNameOfFolderImIn( path, out );
		TP_EXPECT( !TP_STRCMP( out, "." ) );

		path = "..";
		TP_EXPECT( !tpNameOfFolderImIn( path, out ) );

		path = ".";
		TP_EXPECT( !tpNameOfFolderImIn( path, out ) );

		path = "";
		TP_EXPECT( !tpNameOfFolderImIn( path, out ) );

		const char* path_a = "asdf";
		const char* path_b = "qwerzxcv";
		tpConcat( path_a, path_b, out, TP_MAX_PATH );
		TP_EXPECT( !TP_STRCMP( out, "asdf/qwerzxcv" ) );

		path_a = "path/owoasf.as.f.q.e.a";
		path_b = "..";
		tpConcat( path_a, path_b, out, TP_MAX_PATH );
		TP_EXPECT( !TP_STRCMP( out, "path/owoasf.as.f.q.e.a/.." ) );

		path_a = "a/b/c";
		path_b = "d/e/f/g/h/i";
		tpConcat( path_a, path_b, out, TP_MAX_PATH );
		TP_EXPECT( !TP_STRCMP( out, "a/b/c/d/e/f/g/h/i" ) );
	}

#else

	void tpDoUnitTests()
	{
	}

#endif // TP_UNIT_TESTS

#endif

/*
	This is free and unencumbered software released into the public domain.

	Our intent is that anyone is free to copy and use this software,
	for any purpose, in any form, and by any means.

	The authors dedicate any and all copyright interest in the software
	to the public domain, at their own expense for the betterment of mankind.

	The software is provided "as is", without any kind of warranty, including
	any implied warranty. If it breaks, you get to keep both pieces.
*/
