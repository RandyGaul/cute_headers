/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	tinypath.h - v1.01

	To create implementation (the function definitions)
		#define TINYPATH_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY:

		Collection of c-string manipulation functions for dealing with common file-path
		operations. More or less a less-featuerd replacement for Shlwapi.h path functions
		on Windows.

		Performs no dynamic memory management and has no external dependencies (other than
		some crt funcs).

	Revision history:
		1.0  (11/01/2017) initial release
		1.01 (11/10/2017) tpCompact, tpPop bugfixes
*/

/*
	Contributors:
		sro5h             1.01 - tpCompact, tpPop bugfixes
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

// Shrinks the path to the desired length n, the out buffer will never be bigger than
// n + 1. Places three '.' between the last part of the path and the first part that
// will be shortened to fit. If the last part is too long to fit in a string of length n,
// the last part will be shortened to fit and three '.' will be added in front & back.
int tpCompact( const char* path, char* out, int n );

// Some useful (but not yet implemented) functions
/*
	int tpRoot( const char* path, char* out );
*/

#define TP_UNIT_TESTS 1
void tpDoUnitTests();

#define TINYPATH_H
#endif

#if defined( TINYPATH_IMPLEMENTATION )
#endif TINYPATH_IMPLEMENTATION

#ifdef _WIN32

	#if !defined( _CRT_SECURE_NO_WARNINGS )
		#define _CRT_SECURE_NO_WARNINGS
	#endif

#endif

#include <string.h> // strncpy, strncat, strlen
#define TP_STRNCPY strncpy
#define TP_STRNCAT strncat
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

	int has_period = c == '.';
	int len = (int)( period - path ) - 1 + initial_skipped_periods;
	if ( len > TP_MAX_PATH - 1 ) len = TP_MAX_PATH - 1;

	if ( out )
	{
		TP_STRNCPY( out, path - initial_skipped_periods, len );
		out[ len ] = 0;
	}

	if ( ext )
	{
		if ( has_period )
		{
			TP_STRNCPY( ext, path - initial_skipped_periods + len + 1, TP_MAX_EXT );
		}
		else
		{
			ext[ 0 ] = 0;
		}
	}
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

        // don't ignore trailing slash if it is the first character
        if (len > 1)
        {
                len -= 1;
        }

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

int tpCompact( const char* path, char* out, int n )
{
	if (n <= 6) return 0;

	const char* sep = "...";
	const int seplen = strlen( sep );

	int pathlen = strlen( path );
	out[ 0 ] = 0;

	if ( pathlen <= n )
	{
		TP_STRNCPY( out, path, pathlen );
		out[ pathlen ] = 0;
		return pathlen;
	}

	// Find last path separator
	// Ignores the last character as it could be a path separator
	int i = pathlen - 1;
	do
	{
		--i;
	} while ( !tpIsSlash( path[ i ] ) && i > 0 );

	const char* back = path + i;
	int backlen = strlen( back );

	// No path separator was found or the first character was one
	if ( pathlen == backlen )
	{
		TP_STRNCPY( out, path, n - seplen );
		out[ n - seplen ] = 0;
		TP_STRNCAT( out, sep, seplen + 1 );
		return n;
	}

	// Last path part with separators in front equals n
	if ( backlen == n - seplen )
	{
		TP_STRNCPY( out, sep, seplen + 1 );
		TP_STRNCAT( out, back, backlen );
		return n;
	}

	// Last path part with separators in front is too long
	if ( backlen > n - seplen )
	{
		TP_STRNCPY( out, sep, seplen + 1 );
		TP_STRNCAT( out, back, n - ( 2 * seplen ) );
		TP_STRNCAT( out, sep, seplen );
		return n;
	}

	int remaining = n - backlen - seplen;

	TP_STRNCPY( out, path, remaining );
	out[ remaining ] = 0;
	TP_STRNCAT( out, sep, seplen );
	TP_STRNCAT( out, back, backlen );

	return n;
}

#if TP_UNIT_TESTS

	#include <stdio.h>

	#define TP_STRCMP strcmp
	#define TP_EXPECT(X) do { if ( !(X) ) printf( "Failed tinyfiles.h unit test at line %d of file %s.\n", __LINE__, __FILE__ ); } while ( 0 )

	void tpDoUnitTests()
	{
		char out[ TP_MAX_PATH ];
                char pop[ TP_MAX_PATH ];
                char ext[ TP_MAX_PATH ];
                int n;

		const char* path = "../root/file.ext";
		tpPopExt( path, out, ext );
		TP_EXPECT( !TP_STRCMP( out, "../root/file" ) );
		TP_EXPECT( !TP_STRCMP( ext, "ext" ) );

		tpPop( path, out, pop );
		TP_EXPECT( !TP_STRCMP( out, "../root" ) );
		TP_EXPECT( !TP_STRCMP( pop, "file.ext" ) );

		path = "../root/file";
		tpPopExt( path, out, ext );
		TP_EXPECT( !TP_STRCMP( out, "../root/file" ) );
		TP_EXPECT( !TP_STRCMP( ext, "" ) );

		tpPop( path, out, pop );
		TP_EXPECT( !TP_STRCMP( out, "../root" ) );
		TP_EXPECT( !TP_STRCMP( pop, "file" ) );

		path = "../root/";
		tpPopExt( path, out, ext );
		TP_EXPECT( !TP_STRCMP( out, "../root/" ) );
		TP_EXPECT( !TP_STRCMP( ext, "" ) );

		tpPop( path, out, pop );
		TP_EXPECT( !TP_STRCMP( out, ".." ) );
		TP_EXPECT( !TP_STRCMP( pop, "root" ) );

		path = "../root";
		tpPopExt( path, out, ext );
		TP_EXPECT( !TP_STRCMP( out, "../root" ) );
		TP_EXPECT( !TP_STRCMP( ext, "" ) );

		tpPop( path, out, pop );
		TP_EXPECT( !TP_STRCMP( out, ".." ) );
		TP_EXPECT( !TP_STRCMP( pop, "root" ) );

		path = "/file";
		tpPopExt( path, out, ext );
		TP_EXPECT( !TP_STRCMP( out, "/file" ) );
		TP_EXPECT( !TP_STRCMP( ext, "" ) );

		tpPop( path, out, pop );
		TP_EXPECT( !TP_STRCMP( out, "/" ) );
		TP_EXPECT( !TP_STRCMP( pop, "file" ) );

		path = "../";
		tpPopExt( path, out, ext );
		TP_EXPECT( !TP_STRCMP( out, "../" ) );
		TP_EXPECT( !TP_STRCMP( ext, "" ) );

		tpPop( path, out, pop );
		TP_EXPECT( !TP_STRCMP( out, "." ) );
		TP_EXPECT( !TP_STRCMP( pop, "" ) );

		path = "..";
		tpPopExt( path, out, ext );
		TP_EXPECT( !TP_STRCMP( out, ".." ) );
		TP_EXPECT( !TP_STRCMP( ext, "" ) );

		tpPop( path, out, pop );
		TP_EXPECT( !TP_STRCMP( out, "." ) );
		TP_EXPECT( !TP_STRCMP( pop, "" ) );

		path = ".";
		tpPopExt( path, out, ext );
		TP_EXPECT( !TP_STRCMP( out, "." ) );
		TP_EXPECT( !TP_STRCMP( ext, "" ) );

		tpPop( path, out, pop );
		TP_EXPECT( !TP_STRCMP( out, "." ) );
		TP_EXPECT( !TP_STRCMP( pop, "" ) );

		path = "";
		tpPopExt( path, out, ext );
		TP_EXPECT( !TP_STRCMP( out, "" ) );
		TP_EXPECT( !TP_STRCMP( ext, "" ) );

		tpPop( path, out, pop );
		TP_EXPECT( !TP_STRCMP( out, "." ) );
		TP_EXPECT( !TP_STRCMP( pop, "" ) );

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

		path = "/path/to/file.vim";
		n = tpCompact( path, out, 17 );
		TP_EXPECT( !TP_STRCMP( out, "/path/to/file.vim" ) );
		TP_EXPECT( n == TP_STRLEN( out ) );

		path = "/path/to/file.vim";
		n = tpCompact( path, out, 16 );
		TP_EXPECT( !TP_STRCMP( out, "/pat.../file.vim" ) );
		TP_EXPECT( n == TP_STRLEN( out ) );

		path = "/path/to/file.vim";
		n = tpCompact( path, out, 12 );
		TP_EXPECT( !TP_STRCMP( out, ".../file.vim" ) );
		TP_EXPECT( n == TP_STRLEN( out ) );

		path = "/path/to/file.vim";
		n = tpCompact( path, out, 11 );
		TP_EXPECT( !TP_STRCMP( out, ".../file..." ) );
		TP_EXPECT( n == TP_STRLEN( out ) );

		path = "longfile.vim";
		n = tpCompact( path, out, 12 );
		TP_EXPECT( !TP_STRCMP( out, "longfile.vim" ) );
		TP_EXPECT( n == TP_STRLEN( out ) );

		path = "longfile.vim";
		n = tpCompact( path, out, 11 );
		TP_EXPECT( !TP_STRCMP( out, "longfile..." ) );
		TP_EXPECT( n == TP_STRLEN( out ) );
	}

#else

	void tpDoUnitTests()
	{
	}

#endif // TP_UNIT_TESTS

#endif // TINYPATH_IMPLEMENTATION

/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2017 Randy Gaul http://www.randygaul.net
	This software is provided 'as-is', without any express or implied warranty.
	In no event will the authors be held liable for any damages arising from
	the use of this software.
	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:
	  1. The origin of this software must not be misrepresented; you must not
	     claim that you wrote the original software. If you use this software
	     in a product, an acknowledgment in the product documentation would be
	     appreciated but is not required.
	  2. Altered source versions must be plainly marked as such, and must not
	     be misrepresented as being the original software.
	  3. This notice may not be removed or altered from any source distribution.
	------------------------------------------------------------------------------
	ALTERNATIVE B - Public Domain (www.unlicense.org)
	This is free and unencumbered software released into the public domain.
	Anyone is free to copy, modify, publish, use, compile, sell, or distribute this 
	software, either in source code form or as a compiled binary, for any purpose, 
	commercial or non-commercial, and by any means.
	In jurisdictions that recognize copyright laws, the author or authors of this 
	software dedicate any and all copyright interest in the software to the public 
	domain. We make this dedication for the benefit of the public at large and to 
	the detriment of our heirs and successors. We intend this dedication to be an 
	overt act of relinquishment in perpetuity of all present and future rights to 
	this software under copyright law.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
	AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
	ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
	------------------------------------------------------------------------------
*/
