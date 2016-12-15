#if !defined( TINYMEMFILE_H )

#ifdef _DEBUG
#include <assert.h>
#include <string.h>
#endif

#include <stdio.h> // sscanf

#define TM_LOOP_0( F, N )
#define TM_LOOP_1( F, N ) F( 1 )
#define TM_LOOP_2( F, N ) TM_LOOP_1( F, N ), F( 2 )
#define TM_LOOP_3( F, N ) TM_LOOP_2( F, N ), F( 3 )
#define TM_LOOP_4( F, N ) TM_LOOP_3( F, N ), F( 4 )
#define TM_LOOP_5( F, N ) TM_LOOP_4( F, N ), F( 5 )
#define TM_LOOP_6( F, N ) TM_LOOP_5( F, N ), F( 6 )
#define TM_LOOP_7( F, N ) TM_LOOP_6( F, N ), F( 7 )
#define TM_LOOP_8( F, N ) TM_LOOP_7( F, N ), F( 8 )

#define TM_ITERATE_0( F )
#define TM_ITERATE_1( F ) F( 1 )
#define TM_ITERATE_2( F ) TM_ITERATE_1( F ) F( 2 )
#define TM_ITERATE_3( F ) TM_ITERATE_2( F ) F( 3 )
#define TM_ITERATE_4( F ) TM_ITERATE_3( F ) F( 4 )
#define TM_ITERATE_5( F ) TM_ITERATE_4( F ) F( 5 )
#define TM_ITERATE_6( F ) TM_ITERATE_5( F ) F( 6 )
#define TM_ITERATE_7( F ) TM_ITERATE_6( F ) F( 7 )
#define TM_ITERATE_8( F ) TM_ITERATE_7( F ) F( 8 )

#define TM_ITERATE( F, N ) TM_ITERATE_##N( F )

#define TM_LOOP( F, N ) TM_LOOP_##N( F, N )

// kinda sketchy, but helps compile on Windows
// feel free to remove
#ifdef WIN32
	#define inline __inline
	#if !defined( _CRT_SECURE_NO_WARNINGS )
		#define _CRT_SECURE_NO_WARNINGS
	#endif
#endif

//--------------------------------------------------------------------------------------------------
typedef struct
{
	const char* ptr;
	int bytes_read;
} tmFILE;

//--------------------------------------------------------------------------------------------------
inline void tmOpenFileInMemory( tmFILE* file, const void* data )
{
	file->ptr = (const char*)data;
	file->bytes_read = 0;
}

//--------------------------------------------------------------------------------------------------
inline void tmFormatMemfileBuffer( const char* format, char* buffer )
{
	int i;

#ifdef _DEBUG
	assert( strlen( format ) + 1 < 256 - 3 );
#endif

	for ( i = 0; format[ i ]; ++i )
	{
		buffer[ i ] = format[ i ];
	}

	buffer[ i ] = '%';
	buffer[ i + 1 ] = 'n';
	buffer[ i + 2 ] = 0;
}

//--------------------------------------------------------------------------------------------------
inline void tmseek( tmFILE* fp, int offset )
{
#ifdef _DEBUG
	assert( fp->bytes_read + offset >= 0 );
#endif

	fp->bytes_read = offset;
}

//--------------------------------------------------------------------------------------------------
#define TM_SCANF_PARAMS( N ) \
	void* argument_##N

#define TM_SCANF_ARGS( N ) \
	argument_##N

#define TM_SCANF_FUNC( N ) \
	inline int tmscanf( tmFILE* file, const char* format, TM_LOOP( TM_SCANF_PARAMS, N ) ) \
	{ \
		char buffer[ 256 ]; \
		int bytes_read; \
		int ret; \
		tmFormatMemfileBuffer( format, buffer ); \
		ret = sscanf( file->ptr + file->bytes_read, buffer, TM_LOOP( TM_SCANF_ARGS, N ), &bytes_read ); \
		file->bytes_read += bytes_read; \
		return ret; \
	}

TM_ITERATE( TM_SCANF_FUNC, 8 )

#define TINYMEMFILE_H
#endif

/*
	zlib license:
	
	Copyright (c) 2016 Randy Gaul http://www.randygaul.net
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
*/
