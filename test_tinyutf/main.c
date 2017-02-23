#define TINY_UTF_IMPLEMENTATION
#include "../tinyutf.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define CHECK( X ) do { if ( !(X) ) { printf( "FAILED (line %d): %s\n", __LINE__, #X ); } } while ( 0 )

char* tdReadFileToMemory( const char* path, int* size )
{
	char* data = 0;
	FILE* fp = fopen( path, "rb" );
	int sizeNum = 0;

	if ( fp )
	{
		fseek( fp, 0, SEEK_END );
		sizeNum = ftell( fp );
		fseek( fp, 0, SEEK_SET );
		data = (char*)malloc( sizeNum + 1 );
		fread( data, sizeNum, 1, fp );
		data[ sizeNum ] = 0;
		fclose( fp );
	}

	if ( size ) *size = sizeNum;
	return data;
}

int main( )
{
	int size;
	const char* utf8_text = tdReadFileToMemory( "utf8.txt", &size );
	const char* original = utf8_text;

	// just size for worst case and convert
	int* utf32_text = (int*)malloc( size * 4 );
	int i = 0;
	while ( utf8_text < original + size )
	{
		int cp;
		utf8_text = tuDecode8( utf8_text, &cp );
		utf32_text[ i++ ] = cp;

		wchar_t wide[ 2 ];
		tuEncode16( wide, cp );
		int cp2;
		tuDecode16( wide, &cp2 );
		CHECK( cp == cp2 );
	}
	int processed_size = (int)(utf8_text - original);
	CHECK( processed_size == size );
	utf8_text = original;

	char* utf8_processed = (char*)malloc( size );
	original = utf8_processed;
	for ( int j = 0; j < i; ++j )
		utf8_processed = tuEncode8( utf8_processed, utf32_text[ j ] );
	processed_size = (int)(utf8_processed - original);
	CHECK( processed_size == size );
	utf8_processed = (char*)original;

	wchar_t* utf16_text = (wchar_t*)malloc( size * 2 );
	wchar_t* original_wide = utf16_text;
	for ( int j = 0; j < i; ++j )
		utf16_text = tuEncode16( utf16_text, utf32_text[ j ] );
	processed_size = (int)(utf16_text - original_wide);
	utf16_text = original_wide;

	for ( int j = 0; j < processed_size; ++j )
	{
		int cp;
		utf16_text = (wchar_t*)tuDecode16( utf16_text, &cp );
		utf8_processed = tuEncode8( utf8_processed, cp );
	}
	processed_size = (int)(utf8_processed - original);
	CHECK( processed_size == size );
	utf16_text = original_wide;

	tuWiden( utf8_text, size, utf16_text );
	tuShorten( utf16_text, size, utf8_processed );
	CHECK( !memcmp( utf8_text, utf8_processed, size ) );

	return 0;
}
