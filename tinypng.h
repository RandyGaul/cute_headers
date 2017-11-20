/*
	tinypng.h - v1.02

	To create implementation (the function definitions)
		#define TINYPNG_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file


	SUMMARY:

		This header wraps some very nice functions by Richard Mitton from his
		tigr (Tiny Graphics) library, with some additional features and small
		bug-fixes.


	Revision history:
		1.00 (12/23/2016) initial release
		1.01 (03/08/2017) tRNS chunk support for paletted images
		1.02 (10/23/2017) support for explicitly loading paletted png images
		1.03 (11/12/2017) construct atlas in memory


	EXAMPLES:

		Loading a PNG from disk, then freeing it
			tpImage img = tpLoadPNG( "images/pic.png" );
			...
			free( img.pix );
			memset( &img, 0, sizeof( img ) );

		Loading a PNG from memory, then freeing it
			tpImage img = tpLoadPNGMem( memory, sizeof( memory ) );
			...
			free( img.pix );
			memset( &img, 0, sizeof( img ) );

		Saving a PNG to disk
			tpSavePNG( "images/example.png", &img );
			// img is just a raw RGBA buffer, and can come from anywhere,
			// not only from tpLoad*** functions

		Creating a texture atlas in memory
			int w = 1024;
			int h = 1024;
			tpAtlasImage* imgs_out = (tpAtlasImage*)malloc( sizeof( tpAtlasImage ) * my_png_count );
			tpImage atlas_img = tpMakeAtlas( w, int h, my_png_array, my_png_count, imgs_out );
			// just pass an array of pointers to images along with the image count. Make sure to also
			// provide an array of `tpAtlasImage` for `tpMakeAtlas` to output important UV info for the
			// images that fit into the atlas.

		Using the default atlas saver
			int errors = tpDefaultSaveAtlas( "atlas.png", "atlas.txt", atlas_img, atlas_imgs, img_count, names_of_all_images ? names_of_all_images : 0 );
			if ( errors ) { ... }
			// Atlas info (like uv coordinates) are in "atlas.txt", and the image was writen to "atlas.png".
			// atlas_imgs was an array of `tpAtlasImage` from the `tpMakeAtlas` function.

		Inflating a DEFLATE block (decompressing memory stored in DEFLATE format)
			tpInflate( in, in_bytes, out, out_bytes );
			// this function requires knowledge of the un-compressed size
			// does *not* do any internal realloc! Will return errors if an
			// attempt to overwrite the out buffer is made
*/

/*
	Contributors:
		Zachary Carter    1.01 - bug catch for tRNS chunk in paletted images
*/

#if !defined( TINYPNG_H )

#ifdef _WIN32

	#if !defined( _CRT_SECURE_NO_WARNINGS )
		#define _CRT_SECURE_NO_WARNINGS
	#endif

#endif

#define TP_ATLAS_MUST_FIT           1 // returns error from tpMakeAtlas if *any* input image does not fit
#define TP_ATLAS_FLIP_Y_AXIS_FOR_UV 1 // flips output uv coordinate's y. Can be useful to "flip image on load"
#define TP_ATLAS_EMPTY_COLOR        0x000000FF

#include <stdint.h>

typedef struct tpPixel tpPixel;
typedef struct tpImage tpImage;
typedef struct tpIndexedImage tpIndexedImage;
typedef struct tpAtlasImage tpAtlasImage;

// Read this in the event of errors from any function
extern const char* g_tpErrorReason;

// return 1 for success, 0 for failures
int tpInflate( void* in, int in_bytes, void* out, int out_bytes );
int tpSavePNG( const char* fileName, const tpImage* img );

// Constructs an atlas image in-memory. The atlas pixels are stored in the returned image. free the pixels
// when done with them. The user must provide an array of tpAtlasImage for the `imgs` param. `imgs` holds
// information about uv coordinates for an associated image in the `pngs` array. Output image has NULL
// pixels buffer in the event of errors.
tpImage tpMakeAtlas( int atlasWidth, int atlasHeight, const tpImage* pngs, int png_count, tpAtlasImage* imgs_out );

// A decent "default" function, ready to use out-of-the-box. Saves out an easy to parse text formatted info file
// along with an atlas image. `names` param can be optionally NULL.
int tpDefaultSaveAtlas( const char* out_path_image, const char* out_path_atlas_txt, const tpImage* atlas, const tpAtlasImage* imgs, int img_count, const char** names );

// these two functions return tpImage::pix as 0 in event of errors
// call free on tpImage::pix when done
tpImage tpLoadPNG( const char *fileName );
tpImage tpLoadPNGMem( const void *png_data, int png_length );

// loads indexed (paletted) pngs, but does not depalette the image into RGBA pixels
// these two functions return tpIndexedImage::pix as 0 in event of errors
// call free on tpIndexedImage::pix when done
tpIndexedImage tpLoadIndexedPNG( const char* fileName );
tpIndexedImage tpLoadIndexedPNGMem( const void *png_data, int png_length );

// converts paletted image into a standard RGBA image
// call free on tpImage::pix when done
tpImage tpDepaletteIndexedImage( tpIndexedImage* img );

// Pre-process the pixels to transform the image data to a premultiplied alpha format.
// Resource: http://www.essentialmath.com/GDC2015/VanVerth_Jim_DoingMathwRGB.pdf
void tpPremultiply( tpImage* img );

struct tpPixel
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct tpImage
{
	int w;
	int h;
	tpPixel* pix;
};

struct tpIndexedImage
{
	int w;
	int h;
	uint8_t* pix;
	uint8_t palette_len;
	tpPixel palette[ 256 ];
};

struct tpAtlasImage
{
	int img_index;    // index into the `imgs` array
	int w, h;         // pixel w/h of original image
	float minx, miny; // u coordinate
	float maxx, maxy; // v coordinate
	int fit;          // non-zero if image fit and was placed into the atlas
};

#define TINYPNG_H
#endif

#ifdef TINYPNG_IMPLEMENTATION
#undef TINYPNG_IMPLEMENTATION

#ifdef _WIN32

	#if !defined( _CRT_SECURE_NO_WARNINGS )
		#define _CRT_SECURE_NO_WARNINGS
	#endif

	#include <malloc.h> // alloca

#else

	#include <alloca.h> // alloca

#endif

#include <stdio.h>  // fopen, fclose, etc.
#include <stdlib.h> // malloc, free, calloc
#include <string.h> // memcpy
#include <assert.h> // assert

#define TP_ASSERT assert
#define TP_ALLOC malloc
#define TP_ALLOCA alloca
#define TP_FREE free
#define TP_MEMCPY memcpy
#define TP_CALLOC calloc

static tpPixel tpMakePixelA( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
{
	tpPixel p = { r, g, b, a };
	return p;
}

static tpPixel tpMakePixel( uint8_t r, uint8_t g, uint8_t b )
{
	tpPixel p = { r, g, b, 0xFF };
	return p;
}

const char* g_tpErrorReason;
#define TP_FAIL( ) do { goto tp_err; } while ( 0 )
#define TP_CHECK( X, Y ) do { if ( !(X) ) { g_tpErrorReason = Y; TP_FAIL( ); } } while ( 0 )
#define TP_CALL( X ) do { if ( !(X) ) goto tp_err; } while ( 0 )
#define TP_LOOKUP_BITS 9
#define TP_LOOKUP_COUNT (1 << TP_LOOKUP_BITS)
#define TP_LOOKUP_MASK (TP_LOOKUP_COUNT - 1)
#define TP_DEFLATE_MAX_BITLEN 15

// DEFLATE tables from RFC 1951
uint8_t g_tpFixed[ 288 + 32 ] = {
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
}; // 3.2.6
uint8_t g_tpPermutationOrder[ 19 ] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 }; // 3.2.7
uint8_t g_tpLenExtraBits[ 29 + 2 ] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,  0,0 }; // 3.2.5
uint32_t g_tpLenBase[ 29 + 2 ] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258,  0,0 }; // 3.2.5
uint8_t g_tpDistExtraBits[ 30 + 2 ] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,  0,0 }; // 3.2.5
uint32_t g_tpDistBase[ 30 + 2 ] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577, 0,0 }; // 3.2.5

typedef struct
{
	uint64_t bits;
	int count;
	uint32_t* words;
	int word_count;
	int word_index;
	int bits_left;

	char* final_bytes;
	int last_bits;

	char* out;
	char* out_end;
	char* begin;

	uint16_t lookup[ TP_LOOKUP_COUNT ];
	uint32_t lit[ 288 ];
	uint32_t dst[ 32 ];
	uint32_t len[ 19 ];
	uint32_t nlit;
	uint32_t ndst;
	uint32_t nlen;
} tpState;

static int tpWouldOverflow( int64_t bits_left, int num_bits )
{
	return bits_left - num_bits < 0;
}

static char* tpPtr( tpState* s )
{
	TP_ASSERT( !(s->bits_left & 7) );
	return (char*)(s->words + s->word_index) - (s->count / 8);
}

static uint64_t tpPeakBits( tpState* s, int num_bits_to_read )
{
	if ( s->count < num_bits_to_read )
	{
		if ( s->bits_left > s->last_bits )
		{
			s->bits |= (uint64_t)s->words[ s->word_index ] << s->count;
			s->count += 32;
			s->word_index += 1;
		}

		else
		{
			TP_ASSERT( s->bits_left <= 3 * 8 );
			int bytes = s->bits_left / 8;
			for ( int i = 0; i < bytes; ++i )
				s->bits |= (uint64_t)(s->final_bytes[ i ]) << (i * 8);
			s->count += s->bits_left;
		}
	}

	return s->bits;
}

static uint32_t tpConsumeBits( tpState* s, int num_bits_to_read )
{
	TP_ASSERT( s->count >= num_bits_to_read );
	uint32_t bits = s->bits & (((uint64_t)1 << num_bits_to_read) - 1);
	s->bits >>= num_bits_to_read;
	s->count -= num_bits_to_read;
	s->bits_left -= num_bits_to_read;
	return bits;
}

static uint32_t tpReadBits( tpState* s, int num_bits_to_read )
{
	TP_ASSERT( num_bits_to_read <= 32 );
	TP_ASSERT( num_bits_to_read >= 0 );
	TP_ASSERT( s->bits_left > 0 );
	TP_ASSERT( s->count <= 64 );
	TP_ASSERT( !tpWouldOverflow( s->bits_left, num_bits_to_read ) );
	tpPeakBits( s, num_bits_to_read );
	uint32_t bits = tpConsumeBits( s, num_bits_to_read );
	return bits;
}

static char* tpReadFileToMemory( const char* path, int* size )
{
	char* data = 0;
	FILE* fp = fopen( path, "rb" );
	int sizeNum = 0;

	if ( fp )
	{
		fseek( fp, 0, SEEK_END );
		sizeNum = ftell( fp );
		fseek( fp, 0, SEEK_SET );
		data = (char*)TP_ALLOC( sizeNum + 1 );
		fread( data, sizeNum, 1, fp );
		data[ sizeNum ] = 0;
		fclose( fp );
	}

	if ( size ) *size = sizeNum;
	return data;
}

static uint32_t tpRev16( uint32_t a )
{
	a = ((a & 0xAAAA) >>  1) | ((a & 0x5555) << 1);
	a = ((a & 0xCCCC) >>  2) | ((a & 0x3333) << 2);
	a = ((a & 0xF0F0) >>  4) | ((a & 0x0F0F) << 4);
	a = ((a & 0xFF00) >>  8) | ((a & 0x00FF) << 8);
	return a;
}

// RFC 1951 section 3.2.2
static int tpBuild( tpState* s, uint32_t* tree, uint8_t* lens, int sym_count )
{
	int n, codes[ 16 ], first[ 16 ], counts[ 16 ] = { 0 };

	// Frequency count
	for ( n = 0; n < sym_count; n++ ) counts[ lens[ n ] ]++;

	// Distribute codes
	counts[ 0 ] = codes[ 0 ] = first[ 0 ] = 0;
	for ( n = 1; n <= 15; ++n )
	{
		codes[ n ] = (codes[ n - 1 ] + counts[ n - 1 ]) << 1;
		first[ n ] = first[ n - 1 ] + counts[ n - 1 ];
	}

	if ( s ) memset( s->lookup, 0, sizeof( 512 * sizeof( uint32_t ) ) );
	for ( int i = 0; i < sym_count; ++i )
	{
		int len = lens[ i ];

		if ( len != 0 )
		{
			TP_ASSERT( len < 16 );
			uint32_t code = codes[ len ]++;
			uint32_t slot = first[ len ]++;
			tree[ slot ] = (code << (32 - len)) | (i << 4) | len;

			if ( s && len <= TP_LOOKUP_BITS )
			{
				int j = tpRev16( code ) >> (16 - len);
				while ( j < (1 << TP_LOOKUP_BITS) )
				{
					s->lookup[ j ] = (uint16_t)((len << TP_LOOKUP_BITS) | i);
					j += (1 << len);
				}
			}
		}
	}

	int max_index = first[ 15 ];
	return max_index;
}

static int tpStored( tpState* s )
{
	// 3.2.3
	// skip any remaining bits in current partially processed byte
	tpReadBits( s, s->count & 7 );

	// 3.2.4
	// read LEN and NLEN, should complement each other
	uint16_t LEN = (uint16_t)tpReadBits( s, 16 );
	uint16_t NLEN = (uint16_t)tpReadBits( s, 16 );
	TP_CHECK( LEN == (uint16_t)(~NLEN), "Failed to find LEN and NLEN as complements within stored (uncompressed) stream." );
	TP_CHECK( s->bits_left / 8 <= (int)LEN, "Stored block extends beyond end of input stream." );
	char* p = tpPtr( s );
	TP_MEMCPY( s->out, p, LEN );
	s->out += LEN;
	return 1;

tp_err:
	return 0;
}

// 3.2.6
static int tpFixed( tpState* s )
{
	s->nlit = tpBuild( s, s->lit, g_tpFixed, 288 );
	s->ndst = tpBuild( 0, s->dst, g_tpFixed + 288, 32 );
	return 1;
}

static int tpDecode( tpState* s, uint32_t* tree, int hi )
{
	uint64_t bits = tpPeakBits( s, 16 );
	uint32_t search = (tpRev16( (uint32_t)bits ) << 16) | 0xFFFF;
	int lo = 0;
	while ( lo < hi )
	{
		int guess = (lo + hi) >> 1;
		if ( search < tree[ guess ] ) hi = guess;
		else lo = guess + 1;
	}

	uint32_t key = tree[ lo - 1 ];
	uint32_t len = (32 - (key & 0xF));
	TP_ASSERT( (search >> len) == (key >> len) );

	int code = tpConsumeBits( s, key & 0xF );
	(void)code;
	return (key >> 4) & 0xFFF;
}

// 3.2.7
static int tpDynamic( tpState* s )
{
	uint8_t lenlens[ 19 ] = { 0 };

	int nlit = 257 + tpReadBits( s, 5 );
	int ndst = 1 + tpReadBits( s, 5 );
	int nlen = 4 + tpReadBits( s, 4 );

	for ( int i = 0 ; i < nlen; ++i )
		lenlens[ g_tpPermutationOrder[ i ] ] = (uint8_t)tpReadBits( s, 3 );

	// Build the tree for decoding code lengths
	s->nlen = tpBuild( 0, s->len, lenlens, 19 );
	uint8_t lens[ 288 + 32 ];

	for ( int n = 0; n < nlit + ndst; )
	{
		int sym = tpDecode( s, s->len, s->nlen );
		switch ( sym )
		{
		case 16: for ( int i =  3 + tpReadBits( s, 2 ); i; --i, ++n ) lens[ n ] = lens[ n - 1 ]; break;
		case 17: for ( int i =  3 + tpReadBits( s, 3 ); i; --i, ++n ) lens[ n ] = 0; break;
		case 18: for ( int i = 11 + tpReadBits( s, 7 ); i; --i, ++n ) lens[ n ] = 0; break;
		default: lens[ n++ ] = (uint8_t)sym; break;
		}
	}

	s->nlit = tpBuild( s, s->lit, lens, nlit );
	s->ndst = tpBuild( 0, s->dst, lens + nlit, ndst );
	return 1;
}

// 3.2.3
static int tpBlock( tpState* s )
{
	while ( 1 )
	{
		int symbol = tpDecode( s, s->lit, s->nlit );

		if ( symbol < 256 )
		{
			TP_CHECK( s->out + 1 <= s->out_end, "Attempted to overwrite out buffer while outputting a symbol." );
			*s->out = (char)symbol;
			s->out += 1;
		}

		else if ( symbol > 256 )
		{
			symbol -= 257;
			int length = tpReadBits( s, g_tpLenExtraBits[ symbol ] ) + g_tpLenBase[ symbol ];
			int distance_symbol = tpDecode( s, s->dst, s->ndst );
			int backwards_distance = tpReadBits( s, g_tpDistExtraBits[ distance_symbol ] ) + g_tpDistBase[ distance_symbol ];
			TP_CHECK( s->out - backwards_distance >= s->begin, "Attempted to write before out buffer (invalid backwards distance)." );
			TP_CHECK( s->out + length <= s->out_end, "Attempted to overwrite out buffer while outputting a string." );
			char* src = s->out - backwards_distance;
			char* dst = s->out;
			s->out += length;

			switch ( backwards_distance )
			{
			case 1: // very common in images
				memset( dst, *src, length );
				break;
			default: while ( length-- ) *dst++ = *src++;
			}
		}

		else break;
	}

	return 1;

tp_err:
	return 0;
}

// 3.2.3
int tpInflate( void* in, int in_bytes, void* out, int out_bytes )
{
	tpState* s = (tpState*)TP_CALLOC( 1, sizeof( tpState ) );
	s->bits = 0;
	s->count = 0;
	s->word_count = in_bytes / 4;
	s->word_index = 0;
	s->bits_left = in_bytes * 8;

	int first_bytes = (int)((size_t)in & 3);
	s->words = (uint32_t*)((char*)in + first_bytes);
	s->last_bits = ((in_bytes - first_bytes) & 3) * 8;
	s->final_bytes = (char*)in + in_bytes - s->last_bits;

	for ( int i = 0; i < first_bytes; ++i )
		s->bits |= (uint64_t)(((uint8_t*)in)[ i ]) << (i * 8);
	s->count = first_bytes * 8;

	s->out = (char*)out;
	s->out_end = s->out + out_bytes;
	s->begin = (char*)out;

	int count = 0;
	int bfinal;
	do
	{
		bfinal = tpReadBits( s, 1 );
		int btype = tpReadBits( s, 2 );

		switch ( btype )
		{
		case 0: TP_CALL( tpStored( s ) ); break;
		case 1: tpFixed( s ); TP_CALL( tpBlock( s ) ); break;
		case 2: tpDynamic( s ); TP_CALL( tpBlock( s ) ); break;
		case 3: TP_CHECK( 0, "Detected unknown block type within input stream." );
		}

		++count;
	}
	while ( !bfinal );

	TP_FREE( s );
	return 1;

tp_err:
	TP_FREE( s );
	return 0;
}

static uint8_t tpPaeth( uint8_t a, uint8_t b, uint8_t c )
{
	int p = a + b - c;
	int pa = abs( p - a );
	int pb = abs( p - b );
	int pc = abs( p - c );
	return (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
}

typedef struct
{
	uint32_t crc;
	uint32_t adler;
	uint32_t bits;
	uint32_t prev;
	uint32_t runlen;
	FILE *fp;
} tpSavePngData;

uint32_t tpCRC_TABLE[] = {
	0, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
	0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

static void tpPut8( tpSavePngData* s, uint32_t a )
{
	fputc( a, s->fp );
	s->crc = (s->crc >> 4) ^ tpCRC_TABLE[ (s->crc & 15) ^ (a & 15) ];
	s->crc = (s->crc >> 4) ^ tpCRC_TABLE[ (s->crc & 15) ^ (a >> 4) ];
}

static void tpUpdateAdler( tpSavePngData* s, uint32_t v )
{
	uint32_t s1 = s->adler & 0xFFFF;
	uint32_t s2 = (s->adler >> 16) & 0xFFFF;
	s1 = (s1 + v) % 65521;
	s2 = (s2 + s1) % 65521;
	s->adler = (s2 << 16) + s1;
}

static void tpPut32( tpSavePngData* s, uint32_t v )
{
	tpPut8( s, (v >> 24) & 0xFF );
	tpPut8( s, (v >> 16) & 0xFF );
	tpPut8( s, (v >> 8) & 0xFF );
	tpPut8( s, v & 0xFF );
}

static void tpPutBits( tpSavePngData* s, uint32_t data, uint32_t bitcount )
{
	while ( bitcount-- )
	{
		uint32_t prev = s->bits;
		s->bits = (s->bits >> 1) | ((data & 1) << 7);
		data >>= 1;

		if ( prev & 1 )
		{
			tpPut8( s, s->bits );
			s->bits = 0x80;
		}
	}
}

static void tpPutBitsr( tpSavePngData* s, uint32_t data, uint32_t bitcount )
{
	while ( bitcount-- )
		tpPutBits( s, data >> bitcount, 1 );
}

static void tpBeginChunk( tpSavePngData* s, const char* id, uint32_t len )
{
	tpPut32( s, len );
	s->crc = 0xFFFFFFFF;
	tpPut8( s, id[ 0 ] );
	tpPut8( s, id[ 1 ] );
	tpPut8( s, id[ 2 ] );
	tpPut8( s, id[ 3 ] );
}

static void tpEncodeLiteral( tpSavePngData* s, uint32_t v )
{
	// Encode a literal/length using the built-in tables.
	// Could do better with a custom table but whatever.
	     if (v < 144) tpPutBitsr( s, 0x030 + v -   0, 8 );
	else if (v < 256) tpPutBitsr( s, 0x190 + v - 144, 9 );
	else if (v < 280) tpPutBitsr( s, 0x000 + v - 256, 7 );
	else              tpPutBitsr( s, 0x0c0 + v - 280, 8 );
}

static void tpEncodeLen( tpSavePngData* s, uint32_t code, uint32_t bits, uint32_t len )
{
	tpEncodeLiteral( s, code + (len >> bits) );
	tpPutBits( s, len, bits );
	tpPutBits( s, 0, 5 );
}

static void tpEndRun( tpSavePngData* s )
{
	s->runlen--;
	tpEncodeLiteral( s, s->prev );

	if ( s->runlen >= 67 ) tpEncodeLen( s, 277, 4, s->runlen - 67 );
	else if ( s->runlen >= 35 ) tpEncodeLen( s, 273, 3, s->runlen - 35 );
	else if ( s->runlen >= 19 ) tpEncodeLen( s, 269, 2, s->runlen - 19 );
	else if ( s->runlen >= 11 ) tpEncodeLen( s, 265, 1, s->runlen - 11 );
	else if ( s->runlen >= 3 ) tpEncodeLen( s, 257, 0, s->runlen - 3 );
	else while ( s->runlen-- ) tpEncodeLiteral( s, s->prev );
}

static void tpEncodeByte( tpSavePngData *s, uint8_t v )
{
	tpUpdateAdler( s, v );

	// Simple RLE compression. We could do better by doing a search
	// to find matches, but this works pretty well TBH.
	if ( s->prev == v && s->runlen < 115 ) s->runlen++;

	else
	{
		if ( s->runlen ) tpEndRun( s );

		s->prev = v;
		s->runlen = 1;
	}
}

static void tpSaveHeader( tpSavePngData* s, tpImage* img )
{
	fwrite( "\211PNG\r\n\032\n", 8, 1, s->fp );
	tpBeginChunk( s, "IHDR", 13 );
	tpPut32( s, img->w );
	tpPut32( s, img->h );
	tpPut8( s, 8 ); // bit depth
	tpPut8( s, 6 ); // RGBA
	tpPut8( s, 0 ); // compression (deflate)
	tpPut8( s, 0 ); // filter (standard)
	tpPut8( s, 0 ); // interlace off
	tpPut32( s, ~s->crc );
}

static long tpSaveData( tpSavePngData* s, tpImage* img, long dataPos )
{
	tpBeginChunk( s, "IDAT", 0 );
	tpPut8( s, 0x08 ); // zlib compression method
	tpPut8( s, 0x1D ); // zlib compression flags
	tpPutBits( s, 3, 3 ); // zlib last block + fixed dictionary

	for ( int y = 0; y < img->h; ++y )
	{
		tpPixel *row = &img->pix[ y * img->w ];
		tpPixel prev = tpMakePixelA( 0, 0, 0, 0 );

		tpEncodeByte( s, 1 ); // sub filter
		for ( int x = 0; x < img->w; ++x )
		{
			tpEncodeByte( s, row[ x ].r - prev.r );
			tpEncodeByte( s, row[ x ].g - prev.g );
			tpEncodeByte( s, row[ x ].b - prev.b );
			tpEncodeByte( s, row[ x ].a - prev.a );
			prev = row[ x ];
		}
	}

	tpEndRun( s );
	tpEncodeLiteral( s, 256 ); // terminator
	while ( s->bits != 0x80 ) tpPutBits( s, 0, 1 );
	tpPut32( s, s->adler );
	long dataSize = (ftell( s->fp ) - dataPos) - 8;
	tpPut32( s, ~s->crc );

	return dataSize;
}

int tpSavePNG( const char* fileName, const tpImage* img )
{
	tpSavePngData s;
	long dataPos, dataSize, err;

	FILE* fp = fopen( fileName, "wb" );
	if ( !fp ) return 1;

	s.fp = fp;
	s.adler = 1;
	s.bits = 0x80;
	s.prev = 0xFFFF;
	s.runlen = 0;

	tpSaveHeader( &s, (tpImage*)img );
	dataPos = ftell( s.fp );
	dataSize = tpSaveData( &s, (tpImage*)img, dataPos );

	// End chunk.
	tpBeginChunk( &s, "IEND", 0 );
	tpPut32( &s, ~s.crc );

	// Write back payload size.
	fseek( fp, dataPos, SEEK_SET );
	tpPut32( &s, dataSize );

	err = ferror( fp );
	fclose( fp );
	return !err;
}

typedef struct
{
	const uint8_t* p;
	const uint8_t* end;
} tpRawPNG;

static uint32_t tpMake32( const uint8_t* s )
{
	return (s[ 0 ] << 24) | (s[ 1 ] << 16) | (s[ 2 ] << 8) | s[ 3 ];
}

static const uint8_t* tpChunk( tpRawPNG* png, const char* chunk, uint32_t minlen )
{
	uint32_t len = tpMake32( png->p );
	const uint8_t* start = png->p;

	if ( !memcmp( start + 4, chunk, 4 ) && len >= minlen )
	{
		int offset = len + 12;

		if ( png->p + offset <= png->end )
		{
			png->p += offset;
			return start + 8;
		}
	}

	return 0;
}

static const uint8_t* tpFind( tpRawPNG* png, const char* chunk, uint32_t minlen )
{
	const uint8_t *start;
	while ( png->p < png->end )
	{
		uint32_t len = tpMake32( png->p );
		start = png->p;
		png->p += len + 12;

		if ( !memcmp( start+4, chunk, 4 ) && len >= minlen && png->p <= png->end )
			return start + 8;
	}

	return 0;
}

static int tpUnfilter( int w, int h, int bpp, uint8_t* raw )
{
	int len = w * bpp;
	uint8_t *prev = raw;
	int x;

	for ( int y = 0; y < h; y++, prev = raw, raw += len )
	{
#define FILTER_LOOP( A, B ) for ( x = 0 ; x < bpp; x++ ) raw[ x ] += A; for ( ; x < len; x++ ) raw[ x ] += B; break
		switch ( *raw++ )
		{
		case 0: break;
		case 1: FILTER_LOOP( 0            , raw[ x - bpp ]  );
		case 2: FILTER_LOOP( prev[ x ]    , prev[ x ] );
		case 3: FILTER_LOOP( prev[ x ] / 2, (raw[ x - bpp ] + prev[ x ]) / 2 );
		case 4: FILTER_LOOP( prev[ x ]    , tpPaeth( raw[ x - bpp], prev[ x ], prev[ x -bpp ] ) );
		default: return 0;
		}
#undef FILTER_LOOP
	}

	return 1;
}

static void tpConvert( int bpp, int w, int h, uint8_t* src, tpPixel* dst )
{
	for ( int y = 0; y < h; y++ )
	{
		// skip filter byte
		src++;

		for ( int x = 0; x < w; x++, src += bpp )
		{
			switch ( bpp )
			{
				case 1: *dst++ = tpMakePixel( src[ 0 ], src[ 0 ], src[ 0 ] ); break;
				case 2: *dst++ = tpMakePixelA( src[ 0 ], src[ 0 ], src[ 0 ], src[ 1 ] ); break;
				case 3: *dst++ = tpMakePixel( src[ 0 ], src[ 1 ], src[ 2 ] ); break;
				case 4: *dst++ = tpMakePixelA( src[ 0 ], src[ 1 ], src[ 2 ], src[ 3 ] ); break;
			}
		}
	}
}

// http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html#C.tRNS
static uint8_t tpGetAlphaForIndexedImage( int index, const uint8_t* trns, uint32_t trns_len )
{
	if ( !trns ) return 255;
	else if ( (uint32_t)index >= trns_len ) return 255;
	else return trns[ index ];
}

static void tpDepalette( int w, int h, uint8_t* src, tpPixel* dst, const uint8_t* plte, const uint8_t* trns, uint32_t trns_len )
{
	for ( int y = 0; y < h; ++y )
	{
		// skip filter byte
		++src;

		for ( int x = 0; x < w; ++x, ++src )
		{
			int c = *src;
			uint8_t r = plte[ c * 3 ];
			uint8_t g = plte[ c * 3 + 1 ];
			uint8_t b = plte[ c * 3 + 2 ];
			uint8_t a = tpGetAlphaForIndexedImage( c, trns, trns_len );
			*dst++ = tpMakePixelA( r, g, b, a );
		}
	}
}

static uint32_t tpGetChunkByteLength( const uint8_t* chunk )
{
	return tpMake32( chunk - 8 );
}

static int tpOutSize( tpImage* img, int bpp )
{
	return (img->w + 1) * img->h * bpp;
}

tpImage tpLoadPNGMem( const void* png_data, int png_length )
{
	const char* sig = "\211PNG\r\n\032\n";
	const uint8_t* ihdr, *first, *plte, *trns;
	int bit_depth, color_type, bpp, w, h, pix_bytes;
	int compression, filter, interlace;
	int datalen, offset;
	uint8_t* out;
	tpImage img = { 0 };
	uint8_t* data = 0;
	tpRawPNG png;
	png.p = (uint8_t*)png_data;
	png.end = (uint8_t*)png_data + png_length;

	TP_CHECK( !memcmp( png.p, sig, 8 ), "incorrect file signature (is this a png file?)" );
	png.p += 8;

	ihdr = tpChunk( &png, "IHDR", 13 );
	TP_CHECK( ihdr, "unable to find IHDR chunk" );
	bit_depth = ihdr[ 8 ];
	color_type = ihdr[ 9 ];
	TP_CHECK( bit_depth == 8, "only bit-depth of 8 is supported" );

	switch ( color_type )
	{
		case 0: bpp = 1; break; // greyscale
		case 2: bpp = 3; break; // RGB
		case 3: bpp = 1; break; // paletted
		case 4: bpp = 2; break; // grey+alpha
		case 6: bpp = 4; break; // RGBA
		default: TP_CHECK( 0, "unknown color type" );
	}

	// +1 for filter byte (which is dumb! just stick this at file header...)
	w = tpMake32( ihdr ) + 1;
	h = tpMake32( ihdr + 4 );
	pix_bytes = w * h * sizeof( tpPixel );
	img.w = w - 1;
	img.h = h;
	img.pix = (tpPixel*)TP_ALLOC( pix_bytes );
	TP_CHECK( img.pix, "unable to allocate raw image space" );

	compression = ihdr[ 10 ];
	filter = ihdr[ 11 ];
	interlace = ihdr[ 12 ];
	TP_CHECK( !compression, "only standard compression DEFLATE is supported" );
	TP_CHECK( !filter, "only standard adaptive filtering is supported" );
	TP_CHECK( !interlace, "interlacing is not supported" );

	// PLTE must come before any IDAT chunk
	first = png.p;
	plte = tpFind( &png, "PLTE", 0 );
	if ( !plte ) png.p = first;
	else first = png.p;

	// tRNS can come after PLTE
	trns = tpFind( &png, "tRNS", 0 );
	if ( !trns ) png.p = first;
	else first = png.p;

	// Compute length of the DEFLATE stream through IDAT chunk data sizes
	datalen = 0;
	for ( const uint8_t* idat = tpFind( &png, "IDAT", 0 ); idat; idat = tpChunk( &png, "IDAT", 0 ) )
	{
		uint32_t len = tpGetChunkByteLength( idat );
		datalen += len;
	}

	// Copy in IDAT chunk data sections to form the compressed DEFLATE stream
	png.p = first;
	data = (uint8_t*)TP_ALLOC( datalen );
	offset = 0;
	for ( const uint8_t* idat = tpFind( &png, "IDAT", 0 ); idat; idat = tpChunk( &png, "IDAT", 0 ) )
	{
		uint32_t len = tpGetChunkByteLength( idat );
		TP_MEMCPY( data + offset, idat, len );
		offset += len;
	}

	// check for proper zlib structure in DEFLATE stream
	TP_CHECK( data && datalen >= 6, "corrupt zlib structure in DEFLATE stream" );
	TP_CHECK( (data[ 0 ] & 0x0f) == 0x08, "only zlib compression method (RFC 1950) is supported" );
	TP_CHECK( (data[ 0 ] & 0xf0) <= 0x70, "innapropriate window size detected" );
	TP_CHECK( !(data[ 1 ] & 0x20), "preset dictionary is present and not supported" );

	out = (uint8_t*)img.pix + tpOutSize( &img, 4 ) - tpOutSize( &img, bpp );
	TP_CHECK( tpInflate( data + 2, datalen - 6, out, pix_bytes ), "DEFLATE algorithm failed" );
	TP_CHECK( tpUnfilter( img.w, img.h, bpp, out ), "invalid filter byte found" );

	if ( color_type == 3 )
	{
		TP_CHECK( plte, "color type of indexed requires a PLTE chunk" );
		uint32_t trns_len = tpGetChunkByteLength( trns );
		tpDepalette( img.w, img.h, out, img.pix, plte, trns, trns_len );
	}
	else tpConvert( bpp, img.w, img.h, out, img.pix );

	TP_FREE( data );
	return img;

tp_err:
	TP_FREE( data );
	TP_FREE( img.pix );
	img.pix = 0;

	return img;
}

tpImage tpLoadPNG( const char *fileName )
{
	tpImage img = { 0 };
	int len;
	void* data = tpReadFileToMemory( fileName, &len );
	if ( !data ) return img;
	img = tpLoadPNGMem( data, len );
	TP_FREE( data );
	return img;
}

tpIndexedImage tpLoadIndexedPNG( const char* fileName )
{
	tpIndexedImage img = { 0 };
	int len;
	void* data = tpReadFileToMemory( fileName, &len );
	if ( !data ) return img;
	img = tpLoadIndexedPNGMem( data, len );
	TP_FREE( data );
	return img;
}

static void tpUnpackIndexedRows( int w, int h, uint8_t* src, uint8_t* dst )
{
	for ( int y = 0; y < h; ++y )
	{
		// skip filter byte
		++src;

		for ( int x = 0; x < w; ++x, ++src )
		{
			*dst++ = *src;
		}
	}
}

void tpUnpackPalette( tpPixel* dst, const uint8_t* plte, int plte_len, const uint8_t* trns, int trns_len )
{
	for ( int i = 0; i < plte_len * 3; i += 3 )
	{
		unsigned char r = plte[ i ];
		unsigned char g = plte[ i + 1 ];
		unsigned char b = plte[ i + 2 ];
		unsigned char a = tpGetAlphaForIndexedImage( i / 3, trns, trns_len );
		tpPixel p = tpMakePixelA( r, g, b, a );
		*dst++ = p;
	}
}

tpIndexedImage tpLoadIndexedPNGMem( const void *png_data, int png_length )
{
	const char* sig = "\211PNG\r\n\032\n";
	const uint8_t* ihdr, *first, *plte, *trns;
	int bit_depth, color_type, bpp, w, h, pix_bytes;
	int compression, filter, interlace;
	int datalen, offset;
	uint8_t* out;
	tpIndexedImage img = { 0 };
	uint8_t* data = 0;
	tpRawPNG png;
	png.p = (uint8_t*)png_data;
	png.end = (uint8_t*)png_data + png_length;

	TP_CHECK( !memcmp( png.p, sig, 8 ), "incorrect file signature (is this a png file?)" );
	png.p += 8;

	ihdr = tpChunk( &png, "IHDR", 13 );
	TP_CHECK( ihdr, "unable to find IHDR chunk" );
	bit_depth = ihdr[ 8 ];
	color_type = ihdr[ 9 ];
	bpp = 1; // bytes per pixel
	TP_CHECK( bit_depth == 8, "only bit-depth of 8 is supported" );
	TP_CHECK( color_type == 3, "only indexed png images (images with a palette) are valid for tpLoadIndexedPNGMem" );

	// +1 for filter byte (which is dumb! just stick this at file header...)
	w = tpMake32( ihdr ) + 1;
	h = tpMake32( ihdr + 4 );
	pix_bytes = w * h * sizeof( uint8_t );
	img.w = w - 1;
	img.h = h;
	img.pix = (uint8_t*)TP_ALLOC( pix_bytes );
	TP_CHECK( img.pix, "unable to allocate raw image space" );

	compression = ihdr[ 10 ];
	filter = ihdr[ 11 ];
	interlace = ihdr[ 12 ];
	TP_CHECK( !compression, "only standard compression DEFLATE is supported" );
	TP_CHECK( !filter, "only standard adaptive filtering is supported" );
	TP_CHECK( !interlace, "interlacing is not supported" );

	// PLTE must come before any IDAT chunk
	first = png.p;
	plte = tpFind( &png, "PLTE", 0 );
	if ( !plte ) png.p = first;
	else first = png.p;

	// tRNS can come after PLTE
	trns = tpFind( &png, "tRNS", 0 );
	if ( !trns ) png.p = first;
	else first = png.p;

	// Compute length of the DEFLATE stream through IDAT chunk data sizes
	datalen = 0;
	for ( const uint8_t* idat = tpFind( &png, "IDAT", 0 ); idat; idat = tpChunk( &png, "IDAT", 0 ) )
	{
		uint32_t len = tpGetChunkByteLength( idat );
		datalen += len;
	}

	// Copy in IDAT chunk data sections to form the compressed DEFLATE stream
	png.p = first;
	data = (uint8_t*)TP_ALLOC( datalen );
	offset = 0;
	for ( const uint8_t* idat = tpFind( &png, "IDAT", 0 ); idat; idat = tpChunk( &png, "IDAT", 0 ) )
	{
		uint32_t len = tpGetChunkByteLength( idat );
		TP_MEMCPY( data + offset, idat, len );
		offset += len;
	}

	// check for proper zlib structure in DEFLATE stream
	TP_CHECK( data && datalen >= 6, "corrupt zlib structure in DEFLATE stream" );
	TP_CHECK( (data[ 0 ] & 0x0f) == 0x08, "only zlib compression method (RFC 1950) is supported" );
	TP_CHECK( (data[ 0 ] & 0xf0) <= 0x70, "innapropriate window size detected" );
	TP_CHECK( !(data[ 1 ] & 0x20), "preset dictionary is present and not supported" );

	out = img.pix;
	TP_CHECK( tpInflate( data + 2, datalen - 6, out, pix_bytes ), "DEFLATE algorithm failed" );
	TP_CHECK( tpUnfilter( img.w, img.h, bpp, out ), "invalid filter byte found" );
	tpUnpackIndexedRows( img.w, img.h, out, img.pix );

	int plte_len = tpGetChunkByteLength( plte ) / 3;
	tpUnpackPalette( img.palette, plte, plte_len, trns, tpGetChunkByteLength( trns ) );
	img.palette_len = (uint8_t)plte_len;

	TP_FREE( data );
	return img;

tp_err:
	TP_FREE( data );
	TP_FREE( img.pix );
	img.pix = 0;

	return img;
}

tpImage tpDepaletteIndexedImage( tpIndexedImage* img )
{
	tpImage out = { 0 };
	out.w = img->w;
	out.h = img->h;
	out.pix = (tpPixel*)TP_ALLOC( sizeof( tpPixel ) * out.w * out.h );

	tpPixel* dst = out.pix;
	uint8_t* src = img->pix;

	for (int y = 0; y < out.h; ++y)
	{
		for (int x = 0; x < out.w; ++x)
		{
			int index = *src++;
			tpPixel p = img->palette[index];
			*dst++ = p;
		}
	}

	return out;
}

typedef struct
{
	int x;
	int y;
} tpv2i;

typedef struct
{
	int img_index;
	tpv2i size;
	tpv2i min;
	tpv2i max;
	int fit;
} tpIntegerImage;

static tpv2i tpV2I( int x, int y )
{
	tpv2i v;
	v.x = x;
	v.y = y;
	return v;
}

static tpv2i tpSub( tpv2i a, tpv2i b )
{
	tpv2i v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	return v;
}

static tpv2i tpAdd( tpv2i a, tpv2i b )
{
	tpv2i v;
	v.x = a.x + b.x;
	v.y = a.y + b.y;
	return v;
}

typedef struct
{
	tpv2i size;
	tpv2i min;
	tpv2i max;
} tpAtlasNode;

static tpAtlasNode* tpBestFit( int sp, const tpImage* png, tpAtlasNode* nodes )
{
	int bestVolume = INT_MAX;
	tpAtlasNode *bestNode = 0;
	int width = png->w;
	int height = png->h;
	int pngVolume = width * height;

	for ( int i = 0; i < sp; ++i )
	{
		tpAtlasNode *node = nodes + i;
		int canContain = node->size.x >= width && node->size.y >= height;
		if ( canContain )
		{
			int nodeVolume = node->size.x * node->size.y;
			if ( nodeVolume == pngVolume ) return node;
			if ( nodeVolume < bestVolume )
			{
				bestVolume = nodeVolume;
				bestNode = node;
			}
		}
	}

	return bestNode;
}

static int tpPerimeterPred( tpIntegerImage* a, tpIntegerImage* b )
{
	int perimeterA = 2 * (a->size.x + a->size.y);
	int perimeterB = 2 * (b->size.x + b->size.y);
	return perimeterB < perimeterA;
}

void tpPremultiply( tpImage* img )
{
	int w = img->w;
	int h = img->h;
	int stride = w * sizeof( tpPixel );
	uint8_t* data = (uint8_t*)img->pix;

	for( int i = 0; i < (int)stride * h; i += sizeof( tpPixel ) )
	{
		float a = (float)data[ i + 3 ] / 255.0f;
		float r = (float)data[ i + 0 ] / 255.0f;
		float g = (float)data[ i + 1 ] / 255.0f;
		float b = (float)data[ i + 2 ] / 255.0f;
		r *= a;
		g *= a;
		b *= a;
		data[ i + 0 ] = (uint8_t)(r * 255.0f);
		data[ i + 1 ] = (uint8_t)(g * 255.0f);
		data[ i + 2 ] = (uint8_t)(b * 255.0f);
	}
}

static void tpQSort( tpIntegerImage* items, int count )
{
	if ( count <= 1 ) return;

	tpIntegerImage pivot = items[ count - 1 ];
	int low = 0;
	for ( int i = 0; i < count - 1; ++i )
	{
		if ( tpPerimeterPred( items + i, &pivot ) )
		{
			tpIntegerImage tmp = items[ i ];
			items[ i ] = items[ low ];
			items[ low ] = tmp;
			low++;
		}
	}

	items[ count - 1 ] = items[ low ];
	items[ low ] = pivot;
	tpQSort( items, low );
	tpQSort( items + low + 1, count - 1 - low );
}

tpImage tpMakeAtlas( int atlas_width, int atlas_height, const tpImage* pngs, int png_count, tpAtlasImage* imgs_out )
{
	float w0, h0, div, wTol, hTol;
	int atlas_image_size, atlas_stride, sp;
	void* atlas_pixels = 0;
	int atlas_node_capacity = png_count * 2;
	tpImage atlas_image;
	tpIntegerImage* images = 0;
	tpAtlasNode* nodes = 0;

	atlas_image.w = atlas_width;
	atlas_image.h = atlas_height;
	atlas_image.pix = 0;

	TP_CHECK( pngs, "pngs array was NULL" );
	TP_CHECK( imgs_out, "imgs_out array was NULL" );

	images = (tpIntegerImage*)TP_ALLOCA( sizeof( tpIntegerImage ) * png_count );
	nodes = (tpAtlasNode*)TP_ALLOC( sizeof( tpAtlasNode ) * atlas_node_capacity );
	TP_CHECK( images, "out of mem" );
	TP_CHECK( nodes, "out of mem" );

	for ( int i = 0; i < png_count; ++i )
	{
		const tpImage* png = pngs + i;
		tpIntegerImage* image = images + i;
		image->fit = 0;
		image->size = tpV2I( png->w, png->h );
		image->img_index = i;
	}

	// Sort PNGs from largest to smallest
	tpQSort( images, png_count );

	// stack pointer, the stack is the nodes array which we will
	// allocate nodes from as necessary.
	sp = 1;

	nodes[ 0 ].min = tpV2I( 0, 0 );
	nodes[ 0 ].max = tpV2I( atlas_width, atlas_height );
	nodes[ 0 ].size = tpV2I( atlas_width, atlas_height );

	// Nodes represent empty space in the atlas. Placing a texture into the
	// atlas involves splitting a node into two smaller pieces (or, if a
	// perfect fit is found, deleting the node).
	for ( int i = 0; i < png_count; ++i )
	{
		tpIntegerImage* image = images + i;
		const tpImage* png = pngs + image->img_index;
		int width = png->w;
		int height = png->h;
		tpAtlasNode *best_fit = tpBestFit( sp, png, nodes );
		if ( TP_ATLAS_MUST_FIT ) TP_CHECK( best_fit, "Not enough room to place image in atlas." );

		image->min = best_fit->min;
		image->max = tpAdd( image->min, image->size );

		if ( best_fit->size.x == width && best_fit->size.y == height )
		{
			tpAtlasNode* last_node = nodes + --sp;
			*best_fit = *last_node;
			image->fit = 1;

			continue;
		}

		image->fit = 1;

		if ( sp == atlas_node_capacity )
		{
			int new_capacity = atlas_node_capacity * 2;
			tpAtlasNode* new_nodes = (tpAtlasNode*)TP_ALLOC( sizeof( tpAtlasNode ) * new_capacity );
			TP_CHECK( new_nodes, "out of mem" );
			memcpy( new_nodes, nodes, sizeof( tpAtlasNode ) * sp );
			TP_FREE( nodes );
			nodes = new_nodes;
			atlas_node_capacity = new_capacity;
		}

		tpAtlasNode* new_node = nodes + sp++;
		new_node->min = best_fit->min;

		// Split bestFit along x or y, whichever minimizes
		// fragmentation of empty space
		tpv2i d = tpSub( best_fit->size, tpV2I( width, height ) );
		if ( d.x < d.y )
		{
			new_node->size.x = d.x;
			new_node->size.y = height;
			new_node->min.x += width;

			best_fit->size.y = d.y;
			best_fit->min.y += height;
		}

		else
		{
			new_node->size.x = width;
			new_node->size.y = d.y;
			new_node->min.y += height;

			best_fit->size.x = d.x;
			best_fit->min.x += width;
		}

		new_node->max = tpAdd( new_node->min, new_node->size );
	}

	// Write the final atlas image, use TP_ATLAS_EMPTY_COLOR as base color
	atlas_stride = atlas_width * sizeof( tpPixel );
	atlas_image_size = atlas_width * atlas_height * sizeof( tpPixel );
	atlas_pixels = TP_ALLOC( atlas_image_size );
	TP_CHECK( atlas_image_size, "out of mem" );
	memset( atlas_pixels, TP_ATLAS_EMPTY_COLOR, atlas_image_size );

	for ( int i = 0; i < png_count; ++i )
	{
		tpIntegerImage* image = images + i;

		if ( image->fit )
		{
			const tpImage* png = pngs + image->img_index;
			char* pixels = (char*)png->pix;
			tpv2i min = image->min;
			tpv2i max = image->max;
			int atlas_offset = min.x * sizeof( tpPixel );
			int tex_stride = png->w * sizeof( tpPixel );

			for ( int row = min.y, y = 0; row < max.y; ++row, ++y )
			{
				void* row_ptr = (char*)atlas_pixels + (row * atlas_stride + atlas_offset);
				TP_MEMCPY( row_ptr, pixels + y * tex_stride, tex_stride );
			}
		}
	}

	atlas_image.pix = (tpPixel*)atlas_pixels;

	// squeeze UVs inward by 128th of a pixel
	// this prevents atlas bleeding. tune as necessary for good results.
	w0 = 1.0f / (float)(atlas_width);
	h0 = 1.0f / (float)(atlas_height);
	div = 1.0f / 128.0f;
	wTol = w0 * div;
	hTol = h0 * div;

	for ( int i = 0; i < png_count; ++i )
	{
		tpIntegerImage* image = images + i;
		tpAtlasImage* img_out = imgs_out + i;

		img_out->img_index = image->img_index;
		img_out->w = image->size.x;
		img_out->h = image->size.y;
		img_out->fit = image->fit;

		if ( image->fit )
		{
			tpv2i min = image->min;
			tpv2i max = image->max;

			float min_x = (float)min.x * w0 + wTol;
			float min_y = (float)min.y * h0 + hTol;
			float max_x = (float)max.x * w0 - wTol;
			float max_y = (float)max.y * h0 - hTol;

			// flip image on y axis
			if ( TP_ATLAS_FLIP_Y_AXIS_FOR_UV )
			{
				float tmp = min_y;
				min_y = max_y;
				max_y = tmp;
			}

			img_out->minx = min_x;
			img_out->miny = min_y;
			img_out->maxx = max_x;
			img_out->maxy = max_y;
		}
	}

	TP_FREE( nodes );
	return atlas_image;

tp_err:
	TP_FREE( atlas_pixels );
	TP_FREE( nodes );
	atlas_image.pix = 0;
	return atlas_image;
}

int tpDefaultSaveAtlas( const char* out_path_image, const char* out_path_atlas_txt, const tpImage* atlas, const tpAtlasImage* imgs, int img_count, const char** names )
{
	FILE* fp = fopen( out_path_atlas_txt, "wt" );
	TP_CHECK( fp, "unable to open out_path_atlas_txt in tpDefaultSaveAtlas" );

	fprintf( fp, "%s\n%d\n\n", out_path_image, img_count );

	for ( int i = 0; i < img_count; ++i )
	{
		const tpAtlasImage* image = imgs + i;
		const char* name = names ? names[ i ] : 0;

		if ( image->fit )
		{
			int width = image->w;
			int height = image->h;
			float min_x = image->minx;
			float min_y = image->miny;
			float max_x = image->maxx;
			float max_y = image->maxy;

			if ( name ) fprintf( fp, "{ \"%s\", w = %d, h = %d, u = { %.10f, %.10f }, v = { %.10f, %.10f } }\n", names[ i ], width, height, min_x, min_y, max_x, max_y );
			else fprintf( fp, "{ w = %d, h = %d, u = { %.10f, %.10f }, v = { %.10f, %.10f } }\n", width, height, min_x, min_y, max_x, max_y );
		}
	}

	// Save atlas image PNG to disk
	TP_CHECK( tpSavePNG( out_path_image, atlas ), "failed to save atlas image to disk" );

tp_err:
	fclose( fp );
	return 0;
}

#endif // TINYPNG_IMPLEMENTATION

/*
	This is free and unencumbered software released into the public domain.

	Our intent is that anyone is free to copy and use this software,
	for any purpose, in any form, and by any means.

	The authors dedicate any and all copyright interest in the software
	to the public domain, at their own expense for the betterment of mankind.

	The software is provided "as is", without any kind of warranty, including
	any implied warranty. If it breaks, you get to keep both pieces.
*/
