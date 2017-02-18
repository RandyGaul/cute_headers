/*
	tinydeflate - v1.0

	SUMMARY:

		This header is a conglomeration of a DEFLATE compliant decompressor, and some
		functions for dealing with PNGs (load, save and atlas creation).


	EXAMPLES:

		Loading a PNG from disk, then freeing it
			tdImage img = tdLoadPNG( "images/pic.png" );
			...
			free( img.pix );
			memset( &img, 0, sizeof( img ) );

		Loading a PNG from memory, then freeing it
			tdImage img = tdLoadPNGMem( memory, sizeof( memory ) );
			...
			free( img.pix );
			memset( &img, 0, sizeof( img ) );

		Saving a PNG to disk
			tdSavePNG( "images/example.png", &img );
			// img is just a raw RGBA buffer, and can come from anywhere,
			// not only from tdLoad*** functions

		Creating a texture atlas
			tdMakeAtlas( "atlas.png", "atlas.txt", 256, 256, imgs, imgs_count, image_names );
			// just pass an array of pointers to images, and the array count
			// outputs a png along with a txt file. The txt contains UV info in
			// a very easy to parse format. The final parameter is optional.

		Inflating a DEFLATE block
			tdInflate( in, in_bytes, out, out_bytes );
			// this function requires knowledge of the un-compressed size
			// does *not* do any internal realloc! Will return errors if an
			// attempt to overwrite the out buffer is made

	LONG WINDED INFO:

		Most of this code is either a direct port of Richard Mitton's tigr library. A
		big thanks to Mitton for placing his code into public domain. Randy Gaul, the
		author of this header, has made some modifications to Mitton's original code,
		namely much faster Huffman decoding, some all new atlas creation code, some
		optimized read/write functions, reduced memory allocations, much more error
		reporting, etc. Also, a couple tricks were referenced from stb_image by Sean
		Barrett. One was the lookup table for decoding, and I don't recall the other
		tricks, though I am sure a few are laying around in the source. Cheers to Sean
		for his great public domain library!


	ENCODING IMAGES:

		Currently saving images uses Mitton's very straight-forward RLE compression
		scheme. This works out fairly well, though it will be replaced soon enough
		with a GZIP-style compressor. The new compressor should be able to compress
		any piece of memory, unlike the current specialized code coupled to the PNG
		stuff.
*/

#if !defined( TINYDEFLATE_H )

#ifdef _WIN32

	#define TD_INLINE __forceinline
	#define _CRT_SECURE_NO_WARNINGS FUCK_YOU
	#include <malloc.h> // alloca

#else

	#define TD_INLINE __attribute__((always_inline))
	#include <alloca.h> // alloca

#endif

#ifdef __GNUC__

	#undef TD_INLINE
	#define TD_INLINE __attribute__((always_inline)) inline
	
#endif

// turn these off to reduce compile time and compile size for un-needed features
// or to control various settings
#define TD_PNG 1                      // png load/save
#define TD_ATLAS 1                    // atlas creation
#define TD_ATLAS_MUST_FIT 1           // returns error from tdMakeAtlas if *any* input image does not fit
#define TD_ATLAS_FLIP_Y_AXIS_FOR_UV 1 // flips output uv coordinate's y. Can be useful to "flip image on load"

#if TD_ATLAS && !TD_PNG
#error TD_ATLAS requires TD_PNG to be set to 1
#endif

#include <stdint.h>

#if TD_PNG

	typedef struct
	{
		uint8_t r;
		uint8_t g;
		uint8_t b;
		uint8_t a;
	} tdPixel;

	#define TD_BPP sizeof( tdPixel )

	typedef struct
	{
		int w;
		int h;
		tdPixel* pix;
	} tdImage;

	TD_INLINE tdPixel tdMakePixelA( uint8_t r, uint8_t g, uint8_t b, uint8_t a )
	{
		tdPixel p = { r, g, b, a };
		return p;
	}

	TD_INLINE tdPixel tdMakePixel( uint8_t r, uint8_t g, uint8_t b )
	{
		tdPixel p = { r, g, b, 0xFF };
		return p;
	}

#endif // TD_PNG

// return 1 for success, 0 for failures
int tdInflate( void* in, int in_bytes, void* out, int out_bytes );
int tdSavePNG( const char* fileName, tdImage* img );
int tdMakeAtlas( const char* out_path_image, const char* out_path_atlas_txt, int atlasWidth, int atlasHeight, tdImage* pngs, int png_count, const char** names );

// these two functions return tdImage::pix as 0 in event of errors
tdImage tdLoadPNG( const char *fileName );
tdImage tdLoadPNGMem( const void *png_data, int png_length );

// read this whenever one of the above functions returns an error
extern const char* g_tdDeflateErrorReason;

#define TD_DEBUG_CHECKS 1
#if TD_DEBUG_CHECKS

	#include <assert.h>
	#define TD_ASSERT assert

	#if defined( _WIN32 )
		#include <crtdbg.h>
		#define TD_CHECK_MEM( ) \
			do \
			{ \
				if ( !_CrtCheckMemory( ) ) \
				{ \
					_CrtDbgBreak( ); \
				} \
			} while( 0 )
	#endif

	#define TD_LOG printf

#else

	#define TD_ASSERT( ... )
	#define TD_LOG( ... )

#endif

#if !defined( TD_CHECK_MEM )
#define TD_CHECK_MEM( )
#endif

#define TINYDEFLATE_H
#endif

#ifdef TINYDEFLATE_IMPL

#include <stdio.h>  // fopen, fclose, etc.
#include <stdlib.h> // malloc, free, calloc
#include <string.h> // memcpy

const char* g_tdDeflateErrorReason;
#define TD_FAIL( ) do { goto td_err; } while ( 0 )
#define TD_CHECK( X, Y ) do { if ( !(X) ) { g_tdDeflateErrorReason = Y; TD_FAIL( ); } } while ( 0 )
#define TD_CALL( X ) do { if ( !(X) ) goto td_err; } while ( 0 )
#define TD_LOOKUP_BITS 9
#define TD_LOOKUP_COUNT (1 << TD_LOOKUP_BITS)
#define TD_LOOKUP_MASK (TD_LOOKUP_COUNT - 1)
#define TD_DEFLATE_MAX_BITLEN 15

// DEFLATE tables from RFC 1951
uint8_t g_tdFixed[ 288 + 32 ] = {
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
}; // 3.2.6
uint8_t g_tdPermutationOrder[ 19 ] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 }; // 3.2.7
uint8_t g_tdLenExtraBits[ 29 + 2 ] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,  0,0 }; // 3.2.5
uint32_t g_tdLenBase[ 29 + 2 ] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258,  0,0 }; // 3.2.5
uint8_t g_tdDistExtraBits[ 30 + 2 ] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,  0,0 }; // 3.2.5
uint32_t g_tdDistBase[ 30 + 2 ] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577, 0,0 }; // 3.2.5

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

	uint16_t lookup[ TD_LOOKUP_COUNT ];
	uint32_t lit[ 288 ];
	uint32_t dst[ 32 ];
	uint32_t len[ 19 ];
	uint32_t nlit;
	uint32_t ndst;
	uint32_t nlen;
} tdIState;

TD_INLINE static int tdWouldOverflow( int64_t bits_left, int num_bits )
{
	return bits_left - num_bits < 0;
}

TD_INLINE static char* tdPtr( tdIState* s )
{
	TD_ASSERT( !(s->bits_left & 7) );
	return (char*)(s->words + s->word_index) - (s->count / 8);
}

TD_INLINE static uint64_t tdPeakBits( tdIState* s, int num_bits_to_read )
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
			TD_ASSERT( s->bits_left <= 3 * 8 );
			int bytes = s->bits_left / 8;
			for ( int i = 0; i < bytes; ++i )
				s->bits |= (uint64_t)(s->final_bytes[ i ]) << (i * 8);
			s->count += s->bits_left;
		}
	}

	return s->bits;
}

TD_INLINE static uint32_t tdConsumeBits( tdIState* s, int num_bits_to_read )
{
	TD_ASSERT( s->count >= num_bits_to_read );
	uint32_t bits = s->bits & (((uint64_t)1 << num_bits_to_read) - 1);
	s->bits >>= num_bits_to_read;
	s->count -= num_bits_to_read;
	s->bits_left -= num_bits_to_read;
	TD_LOG( "read %d %d\n", bits, num_bits_to_read );
	return bits;
}

TD_INLINE static uint32_t tdReadBits( tdIState* s, int num_bits_to_read )
{
	TD_ASSERT( num_bits_to_read <= 32 );
	TD_ASSERT( num_bits_to_read >= 0 );
	TD_ASSERT( s->bits_left > 0 );
	TD_ASSERT( s->count <= 64 );
	TD_ASSERT( !tdWouldOverflow( s->bits_left, num_bits_to_read ) );
	tdPeakBits( s, num_bits_to_read );
	uint32_t bits = tdConsumeBits( s, num_bits_to_read );
	return bits;
}

static char* tdReadFileToMemory( const char* path, int* size )
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

TD_INLINE static uint32_t tdRev16( uint32_t a )
{
	a = ((a & 0xAAAA) >>  1) | ((a & 0x5555) << 1);
	a = ((a & 0xCCCC) >>  2) | ((a & 0x3333) << 2);
	a = ((a & 0xF0F0) >>  4) | ((a & 0x0F0F) << 4);
	a = ((a & 0xFF00) >>  8) | ((a & 0x00FF) << 8);
	return a;
}

TD_INLINE static uint32_t tdRev( uint32_t a, uint32_t len )
{
	return tdRev16( a ) >> (16 - len);
}

// RFC 1951 section 3.2.2
// Slots arrays starts as "bl_count"
static int tdBuild( tdIState* s, uint32_t* tree, uint8_t* lens, int sym_count )
{
	int n, codes[ 16 ], first[ 16 ], counts[ 16 ] = { 0 };

	// Frequency count.
	for ( n = 0; n < sym_count; n++ ) counts[ lens[ n ] ]++;

	// Distribute codes.
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
			TD_ASSERT( len < 16 );
			uint32_t code = codes[ len ]++;
			uint32_t slot = first[ len ]++;
			tree[ slot ] = (code << (32 - len)) | (i << 4) | len;

			if ( s && len <= TD_LOOKUP_BITS )
			{
				int j = tdRev16( code ) >> (16 - len);
				while ( j < (1 << TD_LOOKUP_BITS) )
				{
					s->lookup[ j ] = (uint16_t)((len << TD_LOOKUP_BITS) | i);
					j += (1 << len);
				}
			}
		}
	}

	int max_index = first[ 15 ];
	return max_index;
}

static int tdStored( tdIState* s )
{
	// 3.2.3
	// skip any remaining bits in current partially processed byte
	tdReadBits( s, s->count & 7 );

	// 3.2.4
	// read LEN and NLEN, should complement each other
	uint16_t LEN = tdReadBits( s, 16 );
	uint16_t NLEN = tdReadBits( s, 16 );
	TD_CHECK( LEN == (uint16_t)(~NLEN), "Failed to find LEN and NLEN as complements within stored (uncompressed) stream." );
	TD_CHECK( s->bits_left / 8 <= (int)LEN, "Stored block extends beyond end of input stream." );
	char* p = tdPtr( s );
	memcpy( s->out, p, LEN );
	s->out += LEN;
	return 1;

td_err:
	return 0;
}

// 3.2.6
TD_INLINE static int tdFixed( tdIState* s )
{
	s->nlit = tdBuild( s, s->lit, g_tdFixed, 288 );
	s->ndst = tdBuild( 0, s->dst, g_tdFixed + 288, 32 );
	return 1;
}

static int tdDecode( tdIState* s, uint32_t* tree, int hi )
{
	uint64_t bits = tdPeakBits( s, 16 );
	uint32_t search = (tdRev16( (uint32_t)bits ) << 16) | 0xFFFF;
	int lo = 0;
	while ( lo < hi )
	{
		int guess = (lo + hi) >> 1;
		if ( search < tree[ guess ] ) hi = guess;
		else lo = guess + 1;
	}

	uint32_t key = tree[ lo - 1 ];
	if ( TD_DEBUG_CHECKS )
	{
		uint32_t len = (32 - (key & 0xF));
		TD_ASSERT( (search >> len) == (key >> len) );
	}

	int code = tdConsumeBits( s, key & 0xF );
	(void)code;
	return (key >> 4) & 0xFFF;
}

static int tdTryLookup( tdIState* s, uint32_t* tree, int hi )
{
	//uint64_t bits = tdPeakBits( s, 16 );
	//int index = bits & TD_LOOKUP_MASK;
	//uint32_t code = s->lookup[ index ];

	//if ( code )
	//{
	//	tdConsumeBits( s, code >> 9 );
	//	return code & TD_LOOKUP_MASK;
	//}

	return tdDecode( s, tree, hi );
}

// 3.2.7
static int tdDynamic( tdIState* s )
{
	uint8_t lenlens[ 19 ] = { 0 };

	int nlit = 257 + tdReadBits( s, 5 );
	int ndst = 1 + tdReadBits( s, 5 );
	int nlen = 4 + tdReadBits( s, 4 );

	for ( int i = 0 ; i < nlen; ++i )
		lenlens[ g_tdPermutationOrder[ i ] ] = (uint8_t)tdReadBits( s, 3 );

	// Build the tree for decoding code lengths
	int lenlens2[ 19 ] = { 0 };
	for ( int i = 0; i < 19; ++i ) lenlens2[ i ] = lenlens[ i ];
	s->nlen = tdBuild( 0, s->len, lenlens, 19 );
	uint8_t lens[ 288 + 32 ];

	for ( int n = 0; n < nlit + ndst; )
	{
		int sym = tdDecode( s, s->len, s->nlen );
		switch ( sym )
		{
		case 16: for ( int i =  3 + tdReadBits( s, 2 ); i; --i, ++n ) lens[ n ] = lens[ n - 1 ]; break;
		case 17: for ( int i =  3 + tdReadBits( s, 3 ); i; --i, ++n ) lens[ n ] = 0; break;
		case 18: for ( int i = 11 + tdReadBits( s, 7 ); i; --i, ++n ) lens[ n ] = 0; break;
		default: lens[ n++ ] = (uint8_t)sym; break;
		}
	}

	s->nlit = tdBuild( s, s->lit, lens, nlit );
	s->ndst = tdBuild( 0, s->dst, lens + nlit, ndst );
	return 1;
}

// 3.2.3
static int tdBlock( tdIState* s )
{
	for (;;)
	{
		int symbol = tdTryLookup( s, s->lit, s->nlit );

		if ( symbol < 256 )
		{
			TD_CHECK( s->out + 1 <= s->out_end, "Attempted to overwrite out buffer while outputting a symbol." );
			*s->out = (char)symbol;
			s->out += 1;
		}

		else if ( symbol > 256 )
		{
			symbol -= 257;
			int length = tdReadBits( s, g_tdLenExtraBits[ symbol ] ) + g_tdLenBase[ symbol ];
			int distance_symbol = tdDecode( s, s->dst, s->ndst );
			int backwards_distance = tdReadBits( s, g_tdDistExtraBits[ distance_symbol ] ) + g_tdDistBase[ distance_symbol ];
			TD_CHECK( s->out - backwards_distance >= s->begin, "Attempted to write before out buffer (invalid backwards distance)." );
			TD_CHECK( s->out + length <= s->out_end, "Attempted to overwrite out buffer while outputting a string." );
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

td_err:
	return 0;
}

// 3.2.3
int tdInflate( void* in, int in_bytes, void* out, int out_bytes )
{
	tdIState* s = (tdIState*)calloc( 1, sizeof( tdIState ) );
	s->bits = 0;
	s->count = 0;
	s->word_count = in_bytes / 4;
	s->word_index = 0;
	s->bits_left = in_bytes * 8;

	int first_bytes = (int)((int)in & 3);
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
		bfinal = tdReadBits( s, 1 );
		int btype = tdReadBits( s, 2 );

		switch ( btype )
		{
		case 0: TD_CALL( tdStored( s ) ); break;
		case 1: tdFixed( s ); TD_CALL( tdBlock( s ) ); break;
		case 2: tdDynamic( s ); TD_CALL( tdBlock( s ) ); break;
		case 3: TD_CHECK( 0, "Detected unknown block type within input stream." );
		}

		++count;
	}
	while ( !bfinal );

	free( s );
	return 1;

td_err:
	free( s );
	return 0;
}

#define TD_WINDOW_SIZE       (1024 * 32)
#define TD_HASH_COUNT        (TD_WINDOW_SIZE)
#define TD_ENTRY_BUFFER_SIZE (TD_WINDOW_SIZE / 16)

#if TD_HASH_COUNT > 4294967294
#error (uint32_t)~0) - 1 is the max size allowed
#endif

TD_INLINE static uint32_t djb2( char* str, char* end )
{
	uint32_t h = 5381;
	uint32_t c;

	while ( str != end )
	{
		c = *str;
		h = ((h << 5) + h) + c;
		++str;
	}

	return h;
}

typedef struct tdHash
{
	uint32_t h;
	unsigned char* start;
	struct tdHash* next;
	struct tdHash* prev;
} tdHash;

typedef struct
{
	unsigned char base_len;
	unsigned char base_dst;
	uint16_t symbol_index;
	uint16_t len;
	int dst;
} tdEntry;

typedef struct
{
	uint16_t freq;
	uint16_t code;
	uint16_t len;
} tdLeaf;

typedef struct
{
	unsigned char* in;
	unsigned char* in_end;
	unsigned char* out;
	unsigned char* out_end;
	unsigned char* window;
	tdLeaf len[ 286 ];
	tdLeaf dst[ 30 ];

	int processed_length;
	uint64_t bits;
	int count;
	uint32_t* words;
	int word_count;
	int word_index;
	int64_t bits_left;

	int max_chain_len;
	int do_lazy_search;

	uint32_t hash_rolling;
	tdHash* buckets[ TD_HASH_COUNT ];
	tdHash hashes[ TD_HASH_COUNT ];

	int entry_count;
	tdEntry entries[ TD_ENTRY_BUFFER_SIZE ];
} tdDState;

typedef struct
{
	int max_chain_len;
	int do_lazy_search;
} tdDeflateOptions;

static void tdWriteBits( tdDState* s, uint32_t value, uint32_t num_bits_to_write )
{
	TD_ASSERT( num_bits_to_write <= 32 );
	TD_ASSERT( s->bits_left > 0 );
	TD_ASSERT( s->count <= 32 );
	TD_ASSERT( !tdWouldOverflow( s->bits_left, num_bits_to_write ) );
	TD_LOG( "write %d %d\n", value, num_bits_to_write );
	if ( value == 255 )
	{
		int x;
		x = 10;
	}

	s->bits |= (uint64_t)(value & (((uint64_t)1 << num_bits_to_write) - 1)) << s->count;
	s->count += num_bits_to_write;
	s->bits_left -= num_bits_to_write;

	if ( s->count >= 32 )
	{
		TD_ASSERT( s->word_index < s->word_count );
		s->words[ s->word_index ] = (uint32_t)(s->bits & ((uint32_t)~0));
		s->bits >>= 32;
		s->count -= 32;
		s->word_index += 1;
	}
}

static void tdWriteBitsRev( tdDState* s, uint32_t value, uint32_t num_bits_to_write )
{
	tdWriteBits( s, tdRev( value, num_bits_to_write ), num_bits_to_write );
}

TD_INLINE static void tdFlush( tdDState* s )
{
	TD_ASSERT( s->count <= 32 );
	if ( s->count ) s->words[ s->word_index ] = (uint32_t)(s->bits & ((uint32_t)~0));
}

TD_INLINE static void tdMatchIndices( int len, int dst, int* base_len, int* base_dst )
{
	TD_ASSERT( dst >= 0 && dst <= 32768 );
	int dst_index = 0;
	for ( int i = 0; i < 30; ++i )
	{
		int base = g_tdDistBase[ i ];
		if ( base <= dst ) dst_index = i;
		else break;
	}
	int len_index = 0;
	for ( int i = 0; i < 29; ++i )
	{
		int base = g_tdLenBase[ i ];
		if ( base <= len ) len_index = i;
		else break;
	}
	TD_ASSERT( len_index >=0 && len_index < 29 );
	TD_ASSERT( dst_index >=0 && dst_index < 30 );
	*base_len = len_index;
	*base_dst = dst_index;
}

TD_INLINE static int tdMatchCost( int len, int len_bits, int dst_bits )
{
	int match_bits = len * 8;
	return (len_bits + dst_bits) - match_bits;
}

TD_INLINE static int tdLiteral( tdDState* s, int symbol )
{
	if ( s->entry_count + 1 == TD_ENTRY_BUFFER_SIZE )
		return 0;

	tdEntry entry = { 0 };
	TD_ASSERT( symbol >= 0 && symbol <= 256 );
	entry.symbol_index = symbol;
	s->window++;
	s->entries[ s->entry_count++ ] = entry;
	s->len[ symbol ].freq++;
	s->processed_length += 1;

	return 1;
}

typedef struct tdNode tdNode;
struct tdNode
{
	int weight;   // Total weight (symbol count) of this chain.
	tdNode* tail; // Previous node(s) of this chain, or 0 if none.
	int count;    // Leaf symbol index, or number of leaves before this chain.
};

typedef struct
{
	tdNode* A;
	tdNode* B;
} tdNodePair;

TD_INLINE static void tdInitNode( tdNode* node, int weight, int count, tdNode* tail )
{
  node->weight = weight;
  node->count = count;
  node->tail = tail;
}

// Performs a Boundary Package-Merge step. Puts a new chain in the given list. The
// new chain is, depending on the weights, a leaf or a combination of two chains
// from the previous list. From the paper "A Fast and Space-Economical Algorithm
// for Length-Limited Coding" by Katajainen et al.
static void tdBoundaryPM( tdNodePair* lists, tdNode* leaves, int numsymbols, tdNode** pool, int index )
{
	tdNode* newchain;
	tdNode* oldchain;
	int lastcount = lists[ index ].B->count;
	if ( index == 0 && lastcount >= numsymbols ) return;

	newchain = (*pool)++;
	oldchain = lists[ index ].B;

	// These are set up before the recursive calls below, so that there is a list
	// pointing to the new node, to let the garbage collection know it's in use
	lists[ index ].A = oldchain;
	lists[ index ].B = newchain;

	if ( !index )
	{
		// New leaf node in list 0
		tdInitNode( newchain, leaves[ lastcount ].weight, lastcount + 1, 0 );
	}

	else
	{
		int sum = lists[ index - 1 ].A->weight + lists[ index - 1 ].B->weight;
		if ( lastcount < numsymbols && sum > leaves[ lastcount ].weight )
		{
			// New leaf inserted in list, so count is incremented
			tdInitNode( newchain, leaves[lastcount].weight, lastcount + 1, oldchain->tail );
		}

		else
		{
			tdInitNode( newchain, sum, lastcount, lists[ index - 1 ].B );
			// Two lookahead chains of previous list used up, create new ones
			tdBoundaryPM( lists, leaves, numsymbols, pool, index - 1 );
			tdBoundaryPM( lists, leaves, numsymbols, pool, index - 1 );
		}
	}
}

TD_INLINE static void tdBoundaryPMFinal( tdNodePair* lists, tdNode* leaves, int numsymbols, tdNode** pool, int index )
{
	int lastcount = lists[ index ].B->count;
	int sum = lists[ index - 1 ].A->weight + lists[ index - 1 ].B->weight;

	if ( lastcount < numsymbols && sum > leaves[ lastcount ].weight )
	{
		tdNode* newchain = (*pool);
		tdNode* oldchain = lists[ index ].B->tail;

		lists[ index ].B = newchain;
		newchain->count = lastcount + 1;
		newchain->tail = oldchain;
	}
	else lists[ index ].B->tail = lists[ index - 1 ].B;
}

// Initializes each list with as lookahead chains the two leaves with lowest weights
TD_INLINE static void tdInitLists( tdNode** pool, const tdNode* leaves, int maxbits, tdNodePair* lists )
{
	tdNode* node0 = (*pool)++;
	tdNode* node1 = (*pool)++;
	tdInitNode( node0, leaves[ 0 ].weight, 1, 0 );
	tdInitNode( node1, leaves[ 1 ].weight, 2, 0 );
	for ( int i = 0; i < maxbits; i++ )
	{
		lists[ i ].A = node0;
		lists[ i ].B = node1;
	}
}

TD_INLINE static void tdExtractBitLengths( tdNode* chain, tdNode* leaves, unsigned* bitlengths ) 
{
	int counts[ 16 ] = { 0 };
	unsigned end = 16;
	unsigned ptr = 15;
	unsigned value = 1;

	for ( tdNode* node = chain; node; node = node->tail )
		counts[ --end ] = node->count;

	int val = counts[ 15 ];
	while ( ptr >= end )
	{
		for ( ; val > counts[ ptr - 1 ]; --val )
			bitlengths[ leaves[ val - 1 ].count ] = value;
		ptr--;
		value++;
	}
}

TD_INLINE static int tdLeafPred( tdNode* a, tdNode* b )
{
  return a->weight - b->weight;
}

TD_INLINE static void tdQSortNodes( tdNode* items, int count )
{
	if ( count <= 1 ) return;

	tdNode pivot = items[ count - 1 ];
	int low = 0;
	for ( int i = 0; i < count - 1; ++i )
	{
		if ( tdLeafPred( items + i, &pivot ) )
		{
			tdNode tmp = items[ i ];
			items[ i ] = items[ low ];
			items[ low ] = tmp;
			low++;
		}
	}

	items[ count - 1 ] = items[ low ];
	items[ low ] = pivot;
	tdQSortNodes( items, low );
	tdQSortNodes( items + low + 1, count - 1 - low );
}

int tdCodeLengths( int* freq, int count, int maxbits, int* bitlengths )
{
	int numsymbols = 0;
	int numBoundaryPMRuns;
	tdNode* nodes = 0;
	tdNodePair* lists = 0;

	// One leaf per symbol. Only numsymbols leaves will be used
	tdNode* leaves = (tdNode*)malloc( sizeof( tdNode ) * count );
	memset( bitlengths, 0, sizeof( int ) * count );

	for ( int i = 0; i < count; i++ )
	{
		if ( freq[ i ] )
		{
			leaves[ numsymbols ].weight = freq[ i ];
			leaves[ numsymbols ].count = i;
			numsymbols++;
		}
	}

	TD_CHECK( (1 << maxbits) >= numsymbols, "Not enough maxbits for this symbol count." );

	if ( !numsymbols ) goto td_ok;
	// Only one symbol, give it bitlength 1, not 0. OK.
	if ( numsymbols == 1 )
	{
		bitlengths[ leaves[ 0 ].count ] = 1;
		goto td_ok;
	}
	if ( numsymbols == 2 )
	{
		bitlengths[ leaves[ 0 ].count ]++;
		bitlengths[ leaves[ 1 ].count ]++;
		goto td_ok;
	}

	// Sort the leaves from lightest to heaviest. Add count into the same variable for stable sorting.
	for ( int i = 0; i < numsymbols; i++ )
	{
		TD_CHECK( leaves[ i ].weight < ((int)1 << (sizeof( leaves[ 0 ].weight ) * CHAR_BIT - 9)), "9 bits needed for the count." );
		leaves[ i ].weight = (leaves[ i ].weight << 9) | leaves[ i ].count;
	}

	tdQSortNodes( leaves, numsymbols );

	for ( int i = 0; i < numsymbols; i++ )
		leaves[ i ].weight >>= 9;

	if ( numsymbols - 1 < maxbits )
		maxbits = numsymbols - 1;

	nodes = (tdNode*)malloc( maxbits * 2 * numsymbols * sizeof( tdNode ) );
	tdNode* pool = nodes;

	// Array of lists of chains. Each list requires only two lookahead chains at a time,
	// so each list is a array of two Node*'s.
	lists = (tdNodePair*)malloc( sizeof( tdNodePair ) * maxbits );
	tdInitLists( &pool, leaves, maxbits, lists );

	// In the last list, 2 * numsymbols - 2 active chains need to be created. Two are
	// already created in the initialization. Each BoundaryPM run creates one.
	numBoundaryPMRuns = 2 * numsymbols - 4;
	for ( int i = 0; i < numBoundaryPMRuns - 1; ++i )
		tdBoundaryPM( lists, leaves, numsymbols, &pool, maxbits - 1 );
	tdBoundaryPMFinal( lists, leaves, numsymbols, &pool, maxbits - 1 );

	tdExtractBitLengths( lists[ maxbits - 1 ].B, leaves, bitlengths );

td_ok:
	free( lists );
	free( leaves );
	free( nodes );
	return 1;

td_err:
	free( lists );
	free( leaves );
	free( nodes );
	return 0;
}

static void tdLengthsToSymbols( unsigned* lengths, int count, int maxbits, unsigned* symbols )
{
	int bl_count[ 16 ] = { 0 };
	int next_code[ 16 ];
	memset( symbols, 0, sizeof( unsigned ) * count );

	// 1)
	//Count the number of codes for each code length. Let bl_count[ N ] be the
	// number of codes of length N, N >= 1
	for ( int i = 0; i < count; i++ )
	{
		TD_ASSERT( lengths[ i ] <= (unsigned)maxbits );
		bl_count[ lengths[ i ] ]++;
	}

	// 2)
	// Find the numerical value of the smallest code for each code length
	int code = 0;
	bl_count[ 0 ] = 0;
	for ( int bits = 1; bits <= maxbits; bits++ )
	{
		code = (code + bl_count[ bits - 1 ]) << 1;
		next_code[ bits ] = code;
	}

	// 3)
	// Assign numerical values to all codes, using consecutive values for all
	// codes of the same length with the base values determined at step 2
	for ( int i = 0;  i < count; i++ )
	{
		unsigned len = lengths[ i ];
		if ( len )
		{
			symbols[ i ] = next_code[ len ];
			next_code[ len ]++;
		}
	}
}

int tdMakeTree( tdLeaf* tree, int count, int max_bits )
{
	int freq[ 288 ] = { 0 };
	int lens[ 288 ] = { 0 };
	int code[ 288 ] = { 0 };
	for ( int i = 0; i < count; ++i ) freq[ i ] = tree[ i ].freq;
	TD_CALL( tdCodeLengths( freq, count, max_bits, lens ) );
	tdLengthsToSymbols( lens, count, max_bits, code );
	for ( int i = 0; i < count; ++i )
	{
		tree[ i ].code = code[ i ];
		tree[ i ].freq = freq[ i ];
		tree[ i ].len = lens[ i ];
	}

	return 1;

td_err:
	return 0;
}

/* Gets the amount of extra bits for the given dist, cfr. the DEFLATE spec. */
static int ZopfliGetDistExtraBits(int dist) {
#ifdef ZOPFLI_HAS_BUILTIN_CLZ
  if (dist < 5) return 0;
  return (31 ^ __builtin_clz(dist - 1)) - 1; /* log2(dist - 1) - 1 */
#else
  if (dist < 5) return 0;
  else if (dist < 9) return 1;
  else if (dist < 17) return 2;
  else if (dist < 33) return 3;
  else if (dist < 65) return 4;
  else if (dist < 129) return 5;
  else if (dist < 257) return 6;
  else if (dist < 513) return 7;
  else if (dist < 1025) return 8;
  else if (dist < 2049) return 9;
  else if (dist < 4097) return 10;
  else if (dist < 8193) return 11;
  else if (dist < 16385) return 12;
  else return 13;
#endif
}

/* Gets value of the extra bits for the given dist, cfr. the DEFLATE spec. */
static int ZopfliGetDistExtraBitsValue(int dist) {
#ifdef ZOPFLI_HAS_BUILTIN_CLZ
  if (dist < 5) {
    return 0;
  } else {
    int l = 31 ^ __builtin_clz(dist - 1); /* log2(dist - 1) */
    return (dist - (1 + (1 << l))) & ((1 << (l - 1)) - 1);
  }
#else
  if (dist < 5) return 0;
  else if (dist < 9) return (dist - 5) & 1;
  else if (dist < 17) return (dist - 9) & 3;
  else if (dist < 33) return (dist - 17) & 7;
  else if (dist < 65) return (dist - 33) & 15;
  else if (dist < 129) return (dist - 65) & 31;
  else if (dist < 257) return (dist - 129) & 63;
  else if (dist < 513) return (dist - 257) & 127;
  else if (dist < 1025) return (dist - 513) & 255;
  else if (dist < 2049) return (dist - 1025) & 511;
  else if (dist < 4097) return (dist - 2049) & 1023;
  else if (dist < 8193) return (dist - 4097) & 2047;
  else if (dist < 16385) return (dist - 8193) & 4095;
  else return (dist - 16385) & 8191;
#endif
}

/* Gets the symbol for the given dist, cfr. the DEFLATE spec. */
static int ZopfliGetDistSymbol(int dist) {
#ifdef ZOPFLI_HAS_BUILTIN_CLZ
  if (dist < 5) {
    return dist - 1;
  } else {
    int l = (31 ^ __builtin_clz(dist - 1)); /* log2(dist - 1) */
    int r = ((dist - 1) >> (l - 1)) & 1;
    return l * 2 + r;
  }
#else
  if (dist < 193) {
    if (dist < 13) {  /* dist 0..13. */
      if (dist < 5) return dist - 1;
      else if (dist < 7) return 4;
      else if (dist < 9) return 5;
      else return 6;
    } else {  /* dist 13..193. */
      if (dist < 17) return 7;
      else if (dist < 25) return 8;
      else if (dist < 33) return 9;
      else if (dist < 49) return 10;
      else if (dist < 65) return 11;
      else if (dist < 97) return 12;
      else if (dist < 129) return 13;
      else return 14;
    }
  } else {
    if (dist < 2049) {  /* dist 193..2049. */
      if (dist < 257) return 15;
      else if (dist < 385) return 16;
      else if (dist < 513) return 17;
      else if (dist < 769) return 18;
      else if (dist < 1025) return 19;
      else if (dist < 1537) return 20;
      else return 21;
    } else {  /* dist 2049..32768. */
      if (dist < 3073) return 22;
      else if (dist < 4097) return 23;
      else if (dist < 6145) return 24;
      else if (dist < 8193) return 25;
      else if (dist < 12289) return 26;
      else if (dist < 16385) return 27;
      else if (dist < 24577) return 28;
      else return 29;
    }
  }
#endif
}

/* Gets the amount of extra bits for the given length, cfr. the DEFLATE spec. */
static int ZopfliGetLengthExtraBits(int l) {
  static const int table[259] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 0
  };
  return table[l];
}

/* Gets value of the extra bits for the given length, cfr. the DEFLATE spec. */
static int ZopfliGetLengthExtraBitsValue(int l) {
  static const int table[259] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 0,
    1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5,
    6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6,
    7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12,
    13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2,
    3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28,
    29, 30, 31, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
    18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 0, 1, 2, 3, 4, 5, 6,
    7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26,
    27, 28, 29, 30, 31, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 0
  };
  return table[l];
}

/*
Gets the symbol for the given length, cfr. the DEFLATE spec.
Returns the symbol in the range [257-285] (inclusive)
*/
static int ZopfliGetLengthSymbol(int l) {
  static const int table[259] = {
    0, 0, 0, 257, 258, 259, 260, 261, 262, 263, 264,
    265, 265, 266, 266, 267, 267, 268, 268,
    269, 269, 269, 269, 270, 270, 270, 270,
    271, 271, 271, 271, 272, 272, 272, 272,
    273, 273, 273, 273, 273, 273, 273, 273,
    274, 274, 274, 274, 274, 274, 274, 274,
    275, 275, 275, 275, 275, 275, 275, 275,
    276, 276, 276, 276, 276, 276, 276, 276,
    277, 277, 277, 277, 277, 277, 277, 277,
    277, 277, 277, 277, 277, 277, 277, 277,
    278, 278, 278, 278, 278, 278, 278, 278,
    278, 278, 278, 278, 278, 278, 278, 278,
    279, 279, 279, 279, 279, 279, 279, 279,
    279, 279, 279, 279, 279, 279, 279, 279,
    280, 280, 280, 280, 280, 280, 280, 280,
    280, 280, 280, 280, 280, 280, 280, 280,
    281, 281, 281, 281, 281, 281, 281, 281,
    281, 281, 281, 281, 281, 281, 281, 281,
    281, 281, 281, 281, 281, 281, 281, 281,
    281, 281, 281, 281, 281, 281, 281, 281,
    282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282,
    282, 282, 282, 282, 282, 282, 282, 282,
    283, 283, 283, 283, 283, 283, 283, 283,
    283, 283, 283, 283, 283, 283, 283, 283,
    283, 283, 283, 283, 283, 283, 283, 283,
    283, 283, 283, 283, 283, 283, 283, 283,
    284, 284, 284, 284, 284, 284, 284, 284,
    284, 284, 284, 284, 284, 284, 284, 284,
    284, 284, 284, 284, 284, 284, 284, 284,
    284, 284, 284, 284, 284, 284, 284, 285
  };
  return table[l];
}

/* Gets the amount of extra bits for the given length symbol. */
static int ZopfliGetLengthSymbolExtraBits(int s) {
  static const int table[29] = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
    3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
  };
  return table[s - 257];
}

/* Gets the amount of extra bits for the given distance symbol. */
static int ZopfliGetDistSymbolExtraBits(int s) {
  static const int table[30] = {
    0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8,
    9, 9, 10, 10, 11, 11, 12, 12, 13, 13
  };
  return table[s];
}

static int tdEncodeDynamic( tdDState* s, unsigned* ll_lengths, unsigned* d_lengths )
{
	int rle[ 286 + 32 ];
	int rle_bits[ 286 + 32 ];
	int rle_index = 0;
	int hlit = 29; // 286 - 257
	int hdist = 29; // 32 - 1, but gzip does not like hdist > 29
	int clcounts[ 19 ] = { 0 };
	unsigned clcl[ 19 ];
	unsigned clsymbols[ 19 ];

	// trim zeros trailing
	while ( hlit > 0 && !ll_lengths[ 257 + hlit - 1 ] ) hlit--;
	while ( hdist > 0 && !d_lengths[ 1 + hdist - 1 ] ) hdist--;
	int hlit2 = hlit + 257;
	int lld_total = hlit2 + hdist + 1;

	// run RLE from RFC-1951 section 3.2.7
	for ( int i = 0; i < lld_total; i++ )
	{
		unsigned char symbol = i < hlit2 ? ll_lengths[ i ] : d_lengths[ i - hlit2 ];
		unsigned count = 1;

		for ( int j = i + 1;
			j < lld_total && symbol == (j < hlit2 ? ll_lengths[ j ] : d_lengths[ j - hlit2 ]);
			j++ )
		{
			count++;
		}
		i += count - 1;

		if ( !symbol && count >= 3 )
		{
			while ( count >= 11 )
			{
				unsigned count2 = count > 138 ? 138 : count;
				rle[ rle_index ] = 18;
				rle_bits[ rle_index++ ] = count2 - 11;
				clcounts[ 18 ]++;
				count -= count2;
			}

			while ( count >= 3 ) {
				unsigned count2 = count > 10 ? 10 : count;
				rle[ rle_index ] = 17;
				rle_bits[ rle_index++ ] = count2 - 3;
				clcounts[ 17 ]++;
				count -= count2;
			}
		}

		if ( count >= 4 )
		{
			--count;
			clcounts[ symbol ]++;
			rle[ rle_index ] = symbol;
			rle_bits[ rle_index++ ] = 0;

			while ( count >= 3 )
			{
				unsigned count2 = count > 6 ? 6 : count;
				rle[ rle_index ] = 16;
				rle_bits[ rle_index++ ] = count2 - 3;
				clcounts[ 16 ]++;
				count -= count2;
			}
		}

		clcounts[ symbol ] += count;
		while ( count > 0 )
		{
			rle[ rle_index ] = symbol;
			rle_bits[ rle_index++ ] = 0;
			count--;
		}
	}

	TD_ASSERT( rle_index <= 288 + 32 ) ;
	tdCodeLengths( clcounts, 19, 7, clcl );
	tdLengthsToSymbols( clcl, 19, 7, clsymbols );

	// trim trailing zeroes
	int hclen = 15;
	while ( hclen > 0 && !clcounts[ g_tdPermutationOrder[ hclen + 4 - 1 ] ] ) hclen--;

	// RFC-1951 section 3.2.7
	int64_t bits_used = s->bits_left;
	tdWriteBits( s, hlit, 5 );
	tdWriteBits( s, hdist, 5 );
	tdWriteBits( s, hclen, 4 );
	int done = hclen + 4;
	for ( int i = 0; i < done; ++i )
		tdWriteBits( s, clcl[ g_tdPermutationOrder[ i ] ], 3 );
	for ( int i = 0; i < rle_index; ++i )
	{
		int symbol = rle[ i ];
		int code = clsymbols[symbol];
		int len = clcl[ symbol ];
		TD_ASSERT( !(code & ~((1 << len) - 1)) );
		tdWriteBitsRev( s, code, len );
		switch ( symbol )
		{
		case 16: tdWriteBits( s, rle_bits[ i ], 2 ); break;
		case 17: tdWriteBits( s, rle_bits[ i ], 3 ); break;
		case 18: tdWriteBits( s, rle_bits[ i ], 7 ); break;
		}
	}
	bits_used = bits_used - s->bits_left;

	int tree_size = 14;
	tree_size += (hclen + 4) * 3;
	for ( int i = 0; i < 19; ++i ) tree_size += clcl[ i ] * clcounts[ i ];
	tree_size += clcounts[ 16 ] * 2;
	tree_size += clcounts[ 17 ] * 3;
	tree_size += clcounts[ 18 ] * 7;
	TD_ASSERT( bits_used == tree_size );
	return tree_size;
}

void tdFlushEntries( tdDState* s, int final )
{
	TD_LOG( "--> lz77 data\n" );
	// manually insert end-of-block entry
	{
		tdEntry entry = { 0 };
		entry.symbol_index = 256;
		TD_ASSERT( s->entry_count < TD_ENTRY_BUFFER_SIZE );
		s->entries[ s->entry_count++ ] = entry;
		s->len[ 256 ].freq++;
	}

	if ( final ) tdWriteBits( s, 1, 1 );
	else tdWriteBits( s, 0, 1 );
	tdWriteBits( s, 2, 2 ); // dynamic

	tdMakeTree( s->len, 286, 15 );
	tdMakeTree( s->dst, 30, 15 );

	uint32_t llens[ 286 ];
	uint32_t dlens[ 30 ];

	for ( int i = 0; i < 286; ++i ) llens[ i ] = s->len[ i ].len;
	for ( int i = 0; i < 30; ++i ) dlens[ i ] = s->dst[ i ].len;
	tdEncodeDynamic( s, llens, dlens );

	int length = 0;
	for ( int i = 0; i < s->entry_count; ++i )
	{
		tdEntry* entry = s->entries + i;
		if ( entry->dst )
		{
			int base_len = entry->base_len;
			tdLeaf* len = s->len + entry->symbol_index;

			TD_ASSERT( entry->len >= 3 && entry->len <= 288 );
			int zopfli_len_index = ZopfliGetLengthSymbol( entry->len );
			int zopfli_dst_index = ZopfliGetDistSymbol( entry->dst );

			TD_ASSERT( zopfli_len_index == entry->symbol_index );
			TD_ASSERT( zopfli_dst_index == entry->base_dst );

			TD_ASSERT( len->code == s->len[ zopfli_len_index ].code );
			TD_ASSERT( len->len == s->len[ zopfli_len_index ].len );

			int ll_base_len = ZopfliGetLengthExtraBitsValue( entry->len );
			int ll_extra_len = ZopfliGetLengthExtraBits( entry->len );
			int ll_base_dst = ZopfliGetDistExtraBitsValue( entry->dst );
			int ll_extra_dst = ZopfliGetDistExtraBits( entry->dst );

			TD_ASSERT( entry->len - g_tdLenBase[ base_len ] == ll_base_len );
			TD_ASSERT( g_tdLenExtraBits[ base_len ] == ll_extra_len );

			TD_ASSERT( len->len > 0 );
			tdWriteBitsRev( s, len->code, len->len );
			tdWriteBits( s, entry->len - g_tdLenBase[ base_len ], g_tdLenExtraBits[ base_len ] );
			int base_dst = entry->base_dst;
			tdLeaf* dst = s->dst + base_dst;

			TD_ASSERT( dst->code == s->dst[ zopfli_dst_index ].code );
			TD_ASSERT( dst->len == s->dst[ zopfli_dst_index ].len );

			TD_ASSERT( entry->dst - g_tdDistBase[ base_dst ] == ll_base_dst );
			TD_ASSERT( g_tdDistExtraBits[ base_dst ] == ll_extra_dst );

			TD_ASSERT( dst->len > 0 );
			tdWriteBitsRev( s, dst->code, dst->len );
			tdWriteBits( s, entry->dst - g_tdDistBase[ base_dst ], g_tdDistExtraBits[ base_dst ] );

			length += entry->len;
		}

		else
		{
			tdLeaf* symbol = s->len + entry->symbol_index;
			int code = symbol->code;
			int len = symbol->len;
			TD_ASSERT( entry->symbol_index <= 256 && entry->symbol_index >= 0 );
			TD_ASSERT( len > 0 );
			TD_ASSERT( !(code & ~((1 << len) - 1)) );
			tdWriteBitsRev( s, code, len );
			++length;
		}
	}

	s->entry_count = 0;
	memset( s->len, 0, sizeof( s->len ) );
	memset( s->dst, 0, sizeof( s->dst ) );
	//memset( s->entries, 0, sizeof( s->entries ) );
	TD_ASSERT( length - 1 == s->processed_length ); // -1 for 256 end of block literal
	s->processed_length = 0;

	// ODDLY SUSPICIOUS
//	memset( s->hashes, 0, sizeof( s->hashes ) );
//	memset( s->buckets, 0, sizeof( s->buckets ) );
//	for ( int i = 0; i < TD_HASH_COUNT; ++i )
//	{
//		tdHash* h = s->hashes + i;
//		h->next = h;
//		h->prev = h;
//	}
}

// RFC-1951 - section 3.2.4
void tdEncodeStored( tdDState* s )
{
	tdWriteBits( s, 1, 1 ); // final
	tdWriteBits( s, 0, 2 ); // stored (uncompressed)
	int ignored = (32 - s->count) & 7;
	tdWriteBits( s, 0, ignored );
	uint16_t LEN = (uint16_t)(s->in_end - s->window);
	uint16_t NLEN = ~LEN;
	tdWriteBits( s, LEN, 16 );
	tdWriteBits( s, NLEN, 16 );

	while ( s->window < s->in_end )
	{
		tdWriteBits( s, *s->window++, 8 );
		s->processed_length += 1;
	}
}

// TODO (randy)
// gracefully handle case of file getting larger
// find any other possible errors, gracefully handle and report them
#define TD_EXTRA_DBG_MEMORY (1024 * 1024)
void* tdDeflateMem( const void* in, int bytes, int* out_bytes, tdDeflateOptions* options )
{
	TD_ASSERT( !((int)in & 3) );
	char* in_buffer = (char*)in;
	tdDState* s = (tdDState*)calloc( 1, sizeof( tdDState ) );
	s->in = (unsigned char*)in;
	s->in_end = s->in + bytes;
	s->out = (unsigned char*)malloc( bytes + TD_EXTRA_DBG_MEMORY );
	s->out_end = s->out + bytes + TD_EXTRA_DBG_MEMORY;
	s->window = s->in;

	s->words = (uint32_t*)s->out;
	s->word_count = (bytes + TD_EXTRA_DBG_MEMORY + 3) / sizeof( uint32_t );
	s->bits_left = bytes * 8 + TD_EXTRA_DBG_MEMORY * 8;
	int64_t bits_left = s->bits_left;

	if ( options )
	{
		s->max_chain_len = options->max_chain_len;
		s->do_lazy_search = options->do_lazy_search;
	}

	else
	{
		s->max_chain_len = 0;
		s->do_lazy_search = 1;
	}

	int max_chain_len = s->max_chain_len;
	int do_lazy_search = s->do_lazy_search;

	for ( int i = 0; i < TD_HASH_COUNT; ++i )
	{
		tdHash* h = s->hashes + i;
		h->next = h;
		h->prev = h;
	}

	int count = 0;
	while ( s->window < s->in_end - 3 )
	{
		//TD_CHECK_MEM( );
		if ( s->entry_count + 1 == TD_ENTRY_BUFFER_SIZE )
		{
			tdFlushEntries( s, 0 );
			++count;
			if ( count == 10 )
			{
				int x;
				x = 10;
			}
		}

		// move sliding window over one byte
		// make 3 byte hash
		char* start = s->window;
		uint32_t h = djb2( start, s->window + 3 ) % TD_HASH_COUNT;
		tdHash* chain = s->buckets[ h ];

		// patchup dangling chains
		//for ( int i = 0; i < TD_HASH_COUNT; ++i )
		//{
		//	if ( i == h ) continue;
		//	tdHash* old_chain = s->buckets[ i ];
		//	if ( !old_chain ) continue;
		//	if ( old_chain == chain )
		//		s->buckets[ i ] = 0;
		//}

#define TD_DBG_BRK( str ) if ( strncmp( start, str, strlen( str ) ) == 0 ) __debugbreak( )
		//TD_DBG_BRK( "ADVENTURES OF" );

		uint32_t hash_index = s->hash_rolling++ % TD_HASH_COUNT;
		tdHash* hash = s->hashes + hash_index;
		//hash = (tdHash*)malloc( sizeof( tdHash ) );

		// Unlink the oldest node from its chain (if not in a chain the node
		// only points to itself, so no harm done).
		hash->prev->next = hash->next;
		hash->next->prev = hash->prev;

		//tdPatchHash( s, hash );

		hash->h = h;
		hash->start = start;
		s->buckets[ h ] = hash;

		// Insert new node *before* the chain. This keeps the chain a circular
		// doubly linked list, and the inserted node can be used as the sentinel
		if ( chain )
		{
			hash->next = chain;
			hash->prev = chain->prev;
			chain->prev = hash;
			hash->prev->next = hash;
		}

		// When no chains exist for a given bucket, setup the circular linked list
		// by pointing node to itself (it becomes its own sentinel).
		else
		{
			hash->next = hash;
			hash->prev = hash;
		}

		if ( chain )
		{
			// loop over chain
			// keep track of best bit reduction
			tdHash* list = chain;
			tdHash* best_match = 0;
			int best_base_len = ~0;
			int best_base_dst = ~0;
			int best_len_bits = ~0;
			int best_dst_bits = ~0;
			int best_dst = ~0;
			int best_len = ~0;
			int lowest_cost = INT_MAX;
			int chain_count = 0;
			while ( list != hash )
			{
				char* a = hash->start;
				char* b = list->start;
				int dst = (int)(a - b);
				if ( dst > 32768 ) break;
				//{
				//	tdHash* next = list->next;
				//	list->next->prev = list->prev;
				//	list->prev->next = list->next;
				//	tdPatchHash( s, list );
				//	memset( list, 0, sizeof( list ) );
				//	list->next = list;
				//	list->prev = list;
				//	list->bucket = ~0;
				//	list = next;
				//	continue;
				//}
				int len = 0;
				while ( len < 258 && *a == *b && a < s->in_end - 3 )
				{
					++a; ++b;
					++len;
				}
				if ( len >= 3 )
				{
					TD_ASSERT( len <= 258 );
					TD_ASSERT( dst == (int)(s->window - list->start) );
					int base_len, base_dst;
					tdMatchIndices( len, dst, &base_len, &base_dst );
					TD_ASSERT( base_len < 29 );
					TD_ASSERT( base_dst < 30 );
					int len_bits = g_tdLenExtraBits[ base_len ];
					int dst_bits = g_tdDistExtraBits[ base_dst ];
					int cost = tdMatchCost( len, len_bits, dst_bits );
					if ( cost < lowest_cost )
					{
						lowest_cost = cost;
						best_match = list;
						best_base_len = base_len;
						best_base_dst = base_dst;
						best_len_bits = len_bits;
						best_dst_bits = dst_bits;
						best_dst = dst;
						best_len = len;
					}
				}

				list = list->next;
				chain_count++;
				if ( max_chain_len && chain_count == max_chain_len )
					break;
			}

			if ( !best_match )
				goto literal;

			TD_ASSERT( best_base_len != ~0 );
			TD_ASSERT( best_base_dst != ~0 );
			TD_ASSERT( best_len_bits != ~0 );
			TD_ASSERT( best_dst_bits != ~0 );
			TD_ASSERT( best_dst != ~0 );
			TD_ASSERT( best_len != ~0 );

			s->len[ 257 + best_base_len ].freq++;
			s->dst[ best_base_dst ].freq++;
			TD_ASSERT( !strncmp( s->window, s->window - best_dst, best_len ) );
			TD_LOG( "%.*s", best_len, s->window );
			s->window += best_len;
			s->processed_length += best_len;

			tdEntry entry;
			TD_ASSERT( best_len >= 3 );
			entry.len = best_len;
			entry.dst = best_dst;
			entry.base_len = best_base_len;
			entry.base_dst = best_base_dst;
			entry.symbol_index = 257 + best_base_len;
			TD_ASSERT( entry.symbol_index > 256 < 288 );
			TD_ASSERT( s->entry_count < TD_ENTRY_BUFFER_SIZE );
			s->entries[ s->entry_count++ ] = entry;

			continue;
		}

	literal:
		TD_LOG( "%c", *s->window );
		tdLiteral( s, *s->window );
	}

	while ( s->window < s->in_end )
		if ( !tdLiteral( s, *s->window ) ) break;

	if ( s->window == s->in_end ) tdFlushEntries( s, 1 );
	else
	{
		tdFlushEntries( s, 0 );
		tdEncodeStored( s );
	}

	tdFlush( s );
	TD_CHECK_MEM( );

	int64_t bits_written = bits_left - s->bits_left;
	int64_t bytes_written = (bits_written + 7) / 8;
	int64_t out_size = bytes_written;
	if ( out_bytes ) *out_bytes = (int)out_size;
	void* out = s->out;
	free( s );
	TD_LOG( "---\n" );
	return out;
}

void* tdDeflate( const char* in_path, const char* out_path, tdDeflateOptions* options )
{
	int size;
	char* in = tdReadFileToMemory( in_path, &size );
	char* out = 0;

	TD_CHECK( in, "Unable to open in_path, or not enough memory to allocate file size." );
	int out_bytes;
	out = (char*)tdDeflateMem( in, size, &out_bytes, options );
	if ( !out ) TD_FAIL( );

	FILE* fp = fopen( out_path, "wb" );
	fwrite( out, out_bytes, 1, fp );
	fclose( fp );

	free( in );
	return out;

td_err:
	free( in );
	return 0;
}

#if TD_PNG

static uint8_t tdPaeth( uint8_t a, uint8_t b, uint8_t c )
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
} tdSavePngData;

uint32_t tdCRC_TABLE[] = {
	0, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
	0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

static void tdPut8( tdSavePngData* s, uint32_t a )
{
	fputc( a, s->fp );
	s->crc = (s->crc >> 4) ^ tdCRC_TABLE[ (s->crc & 15) ^ (a & 15) ];
	s->crc = (s->crc >> 4) ^ tdCRC_TABLE[ (s->crc & 15) ^ (a >> 4) ];
}

static void tdUpdateAdler( tdSavePngData* s, uint32_t v )
{
	uint32_t s1 = s->adler & 0xFFFF;
	uint32_t s2 = (s->adler >> 16) & 0xFFFF;
	s1 = (s1 + v) % 65521;
	s2 = (s2 + s1) % 65521;
	s->adler = (s2 << 16) + s1;
}

static void tdPut32( tdSavePngData* s, uint32_t v )
{
	tdPut8( s, (v >> 24) & 0xFF );
	tdPut8( s, (v >> 16) & 0xFF );
	tdPut8( s, (v >> 8) & 0xFF );
	tdPut8( s, v & 0xFF );
}

static void tdPutBits( tdSavePngData* s, uint32_t data, uint32_t bitcount )
{
	while ( bitcount-- )
	{
		uint32_t prev = s->bits;
		s->bits = (s->bits >> 1) | ((data & 1) << 7);
		data >>= 1;

		if ( prev & 1 )
		{
			tdPut8( s, s->bits );
			s->bits = 0x80;
		}
	}
}

static void tdPutBitsr( tdSavePngData* s, uint32_t data, uint32_t bitcount )
{
	while ( bitcount-- )
		tdPutBits( s, data >> bitcount, 1 );
}

static void tdBeginChunk( tdSavePngData* s, const char* id, uint32_t len )
{
	tdPut32( s, len );
	s->crc = 0xFFFFFFFF;
	tdPut8( s, id[ 0 ] );
	tdPut8( s, id[ 1 ] );
	tdPut8( s, id[ 2 ] );
	tdPut8( s, id[ 3 ] );
}

static void tdEncodeLiteral( tdSavePngData* s, uint32_t v )
{
	// Encode a literal/length using the built-in tables.
	// Could do better with a custom table but whatever.
	     if (v < 144) tdPutBitsr( s, 0x030 + v -   0, 8 );
	else if (v < 256) tdPutBitsr( s, 0x190 + v - 144, 9 );
	else if (v < 280) tdPutBitsr( s, 0x000 + v - 256, 7 );
	else              tdPutBitsr( s, 0x0c0 + v - 280, 8 );
}

static void tdEncodeLen( tdSavePngData* s, uint32_t code, uint32_t bits, uint32_t len )
{
	tdEncodeLiteral( s, code + (len >> bits) );
	tdPutBits( s, len, bits );
	tdPutBits( s, 0, 5 );
}

static void tdEndRun( tdSavePngData* s )
{
	s->runlen--;
	tdEncodeLiteral( s, s->prev );

	if ( s->runlen >= 67 ) tdEncodeLen( s, 277, 4, s->runlen - 67 );
	else if ( s->runlen >= 35 ) tdEncodeLen( s, 273, 3, s->runlen - 35 );
	else if ( s->runlen >= 19 ) tdEncodeLen( s, 269, 2, s->runlen - 19 );
	else if ( s->runlen >= 11 ) tdEncodeLen( s, 265, 1, s->runlen - 11 );
	else if ( s->runlen >= 3 ) tdEncodeLen( s, 257, 0, s->runlen - 3 );
	else while ( s->runlen-- ) tdEncodeLiteral( s, s->prev );
}

static void tdEncodeByte( tdSavePngData *s, uint8_t v )
{
	tdUpdateAdler( s, v );

	// Simple RLE compression. We could do better by doing a search
	// to find matches, but this works pretty well TBH.
	if ( s->prev == v && s->runlen < 115 ) s->runlen++;

	else
	{
		if ( s->runlen ) tdEndRun( s );

		s->prev = v;
		s->runlen = 1;
	}
}

static void tdSaveHeader( tdSavePngData* s, tdImage* img )
{
	fwrite( "\211PNG\r\n\032\n", 8, 1, s->fp );
	tdBeginChunk( s, "IHDR", 13 );
	tdPut32( s, img->w );
	tdPut32( s, img->h );
	tdPut8( s, 8 ); // bit depth
	tdPut8( s, 6 ); // RGBA
	tdPut8( s, 0 ); // compression (deflate)
	tdPut8( s, 0 ); // filter (standard)
	tdPut8( s, 0 ); // interlace off
	tdPut32( s, ~s->crc );
}

static long tdSaveData( tdSavePngData* s, tdImage* img, long dataPos )
{
	tdBeginChunk( s, "IDAT", 0 );
	tdPut8( s, 0x08 ); // zlib compression method
	tdPut8( s, 0x1D ); // zlib compression flags
	tdPutBits( s, 3, 3 ); // zlib last block + fixed dictionary

	for ( int y = 0; y < img->h; ++y )
	{
		tdPixel *row = &img->pix[ y * img->w ];
		tdPixel prev = tdMakePixelA( 0, 0, 0, 0 );

		tdEncodeByte( s, 1 ); // sub filter
		for ( int x = 0; x < img->w; ++x )
		{
			tdEncodeByte( s, row[ x ].r - prev.r );
			tdEncodeByte( s, row[ x ].g - prev.g );
			tdEncodeByte( s, row[ x ].b - prev.b );
			tdEncodeByte( s, row[ x ].a - prev.a );
			prev = row[ x ];
		}
	}

	tdEndRun( s );
	tdEncodeLiteral( s, 256 ); // terminator
	while ( s->bits != 0x80 ) tdPutBits( s, 0, 1 );
	tdPut32( s, s->adler );
	long dataSize = (ftell( s->fp ) - dataPos) - 8;
	tdPut32( s, ~s->crc );

	return dataSize;
}

int tdSavePNG( const char* fileName, tdImage* img )
{
	tdSavePngData s;
	long dataPos, dataSize, err;

	FILE* fp = fopen( fileName, "wb" );
	if ( !fp ) return 1;

	s.fp = fp;
	s.adler = 1;
	s.bits = 0x80;
	s.prev = 0xFFFF;
	s.runlen = 0;

	tdSaveHeader( &s, img );
	dataPos = ftell( s.fp );
	dataSize = tdSaveData( &s, img, dataPos );

	// End chunk.
	tdBeginChunk( &s, "IEND", 0 );
	tdPut32( &s, ~s.crc );

	// Write back payload size.
	fseek( fp, dataPos, SEEK_SET );
	tdPut32( &s, dataSize );

	err = ferror( fp );
	fclose( fp );
	return !err;
}

typedef struct
{
	const uint8_t* p;
	const uint8_t* end;
} tdRawPNG;

TD_INLINE static uint32_t tdMake32( const uint8_t* s )
{
	return (s[ 0 ] << 24) | (s[ 1 ] << 16) | (s[ 2 ] << 8) | s[ 3 ];
}

static const uint8_t *tdChunk( tdRawPNG* png, const char *chunk, uint32_t minlen )
{
	uint32_t len = tdMake32( png->p );
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

static const uint8_t* tdFind( tdRawPNG* png, const char* chunk, uint32_t minlen )
{
	const uint8_t *start;
	while ( png->p < png->end )
	{
		uint32_t len = tdMake32( png->p );
		start = png->p;
		png->p += len + 12;

		if ( !memcmp( start+4, chunk, 4 ) && len >= minlen && png->p <= png->end )
			return start + 8;
	}

	return 0;
}

static int tdUnfilter( int w, int h, int bpp, uint8_t *raw )
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
		case 4: FILTER_LOOP( prev[ x ]    , tdPaeth( raw[ x - bpp], prev[ x ], prev[ x -bpp ] ) );
		default: return 0;
		}
#undef FILTER_LOOP
	}

	return 1;
}

static void tdConvert( int bpp, int w, int h, uint8_t *src, tdPixel *dest )
{
	for ( int y = 0; y < h; y++ )
	{
		// skip filter byte
		src++;

		for ( int x = 0; x < w; x++, src += bpp )
		{
			switch ( bpp )
			{
				case 1: *dest++ = tdMakePixel( src[ 0 ], src[ 0 ], src[ 0 ] ); break;
				case 2: *dest++ = tdMakePixelA( src[ 0 ], src[ 0 ], src[ 0 ], src[ 1 ] ); break;
				case 3: *dest++ = tdMakePixel( src[ 0 ], src[ 1 ], src[ 2 ] ); break;
				case 4: *dest++ = tdMakePixelA( src[ 0 ], src[ 1 ], src[ 2 ], src[ 3 ] ); break;
			}
		}
	}
}

static void tdDepalette( int w, int h, uint8_t *src, tdPixel *dest, const uint8_t *plte )
{
	for ( int y = 0; y < h; y++ )
	{
		// skip filter byte
		src++;

		for ( int x = 0; x < w; x++, src++ )
		{
			int c = src[ 0 ];
			*dest++ = tdMakePixelA(
				plte[ c * 3 ],
				plte[ c * 3 + 1 ],
				plte[ c * 3 + 2 ],
				c ? 255 : 0
				);
		}
	}
}

TD_INLINE static int tdOutSize( tdImage* img, int bpp )
{
	return (img->w + 1) * img->h * bpp;
}

tdImage tdLoadPNGMem( const void *png_data, int png_length )
{
	const char* sig = "\211PNG\r\n\032\n";
	const uint8_t* ihdr, *first, *plte;
	int bit_depth, color_type, bpp, w, h, pix_bytes;
	int compression, filter, interlace;
	int datalen, offset;
	uint8_t* out;
	tdImage img = { 0 };
	uint8_t* data = 0;
	tdRawPNG png;
	png.p = (uint8_t*)png_data;
	png.end = (uint8_t*)png_data + png_length;

	TD_CHECK( !memcmp( png.p, sig, 8 ), "incorrect file signature (is this a png file?)" );
	png.p += 8;

	ihdr = tdChunk( &png, "IHDR", 13 );
	TD_CHECK( ihdr, "unable to find IHDR chunk" );
	bit_depth = ihdr[ 8 ];
	color_type = ihdr[ 9 ];
	bpp; // bytes per pixel
	TD_CHECK( bit_depth == 8, "only bit-depth of 8 is supported" );

	switch ( color_type )
	{
		case 0: bpp = 1; break; // greyscale
		case 2: bpp = 3; break; // RGB
		case 3: bpp = 1; break; // paletted
		case 4: bpp = 2; break; // grey+alpha
		case 6: bpp = 4; break; // RGBA
		default: TD_CHECK( 0, "unknown color type" );
	}

	// +1 for filter byte (which is dumb! just stick this at file header...)
	w = tdMake32( ihdr ) + 1;
	h = tdMake32( ihdr + 4 );
	pix_bytes = w * h * sizeof( tdPixel );
	img.w = w - 1;
	img.h = h;
	img.pix = (tdPixel*)malloc( pix_bytes );
	TD_CHECK( img.pix, "unable to allocate raw image space" );

	compression = ihdr[ 10 ];
	filter = ihdr[ 11 ];
	interlace = ihdr[ 12 ];
	TD_CHECK( !compression, "only standard compression DEFLATE is supported" );
	TD_CHECK( !filter, "only standard adaptive filterngin is supported" );
	TD_CHECK( !interlace, "interlacing is not supported" );

	// PLTE must come before any IDAT chunk
	first = png.p;
	plte = tdFind( &png, "PLTE", 0 );
	if ( !plte ) png.p = first;
	else first = png.p;

	// Compute length of the DEFLATE stream through IDAT chunk data sizes
	datalen = 0;
	for ( const uint8_t* idat = tdFind( &png, "IDAT", 0 ); idat; idat = tdChunk( &png, "IDAT", 0 ) )
	{
		uint32_t len = tdMake32( idat - 8 );
		datalen += len;
	}

	// Copy in IDAT chunk data sections to form the compressed DEFLATE stream
	png.p = first;
	data = (uint8_t*)malloc( datalen );
	offset = 0;
	for ( const uint8_t* idat = tdFind( &png, "IDAT", 0 ); idat; idat = tdChunk( &png, "IDAT", 0 ) )
	{
		uint32_t len = tdMake32( idat - 8 );
		memcpy( data + offset, idat, len );
		offset += len;
	}

	// check for proper zlib structure in DEFLATE stream
	TD_CHECK( data && datalen >= 6, "corrupt zlib structure in DEFLATE stream" );
	TD_CHECK( (data[ 0 ] & 0x0f) == 0x08, "only zlib compression method (RFC 1950) is supported" );
	TD_CHECK( (data[ 0 ] & 0xf0) <= 0x70, "innapropriate window size detected" );
	TD_CHECK( !(data[ 1 ] & 0x20), "preset dictionary is present and not supported" );

	out = (uint8_t*)img.pix + tdOutSize( &img, 4 ) - tdOutSize( &img, bpp );
	TD_CHECK( tdInflate( data + 2, datalen - 6, out, pix_bytes ), "DEFLATE algorithm failed" );
	TD_CHECK( tdUnfilter( img.w, img.h, bpp, out ), "invalid filter byte found" );

	if ( color_type == 3 )
	{
		TD_CHECK( plte, "color type of indexed requires a PLTE chunk" );
		tdDepalette( img.w, img.h, out, img.pix, plte );
	}
	else tdConvert( bpp, img.w, img.h, out, img.pix );

	free( data );
	return img;

td_err:
	free( data );
	free( img.pix );
	img.pix = 0;

	return img;
}

tdImage tdLoadPNG( const char *fileName )
{
	tdImage img = { 0 };
	int len;
	void* data = tdReadFileToMemory( fileName, &len );
	if ( !data ) return img;
	img = tdLoadPNGMem( data, len );
	free( data );
	return img;
}

#if TD_ATLAS

typedef struct
{
	int x;
	int y;
} tdv2i;

TD_INLINE static tdv2i tdV2I( int x, int y )
{
	tdv2i v;
	v.x = x;
	v.y = y;
	return v;
}

TD_INLINE static tdv2i tdSub( tdv2i a, tdv2i b )
{
	tdv2i v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	return v;
}

TD_INLINE static tdv2i tdAdd( tdv2i a, tdv2i b )
{
	tdv2i v;
	v.x = a.x + b.x;
	v.y = a.y + b.y;
	return v;
}

typedef struct
{
	tdv2i size;
	tdv2i min;
	tdv2i max;
} tdAtlasNode;

typedef struct
{
	const char* name;
	tdImage* png;
	tdv2i size;
	tdv2i min;
	tdv2i max;
	int fit;
} tdRawImage;

static tdAtlasNode* tdBestFit( int sp, tdImage* png, tdAtlasNode* nodes )
{
	int bestVolume = INT_MAX;
	tdAtlasNode *bestNode = 0;
	int width = png->w;
	int height = png->h;
	int pngVolume = width * height;

	for ( int i = 0; i < sp; ++i )
	{
		tdAtlasNode *node = nodes + i;
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

TD_INLINE static int tdPerimeterPred( tdRawImage* a, tdRawImage* b )
{
	int perimeterA = 2 * (a->size.x + a->size.y);
	int perimeterB = 2 * (b->size.x + b->size.y);
	return perimeterB < perimeterA;
}

// Pre-process the pixels to transform the image data to a premultiplied alpha format.
// Resource: http://www.essentialmath.com/GDC2015/VanVerth_Jim_DoingMathwRGB.pdf
static void tdPremultiply( tdImage* img )
{
	int w = img->w;
	int h = img->h;
	int stride = w * TD_BPP;
	uint8_t* data = (uint8_t*)img->pix;

	for( int i = 0; i < (int)stride * h; i += TD_BPP )
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

static void tdQSort( tdRawImage* items, int count )
{
	if ( count <= 1 ) return;

	tdRawImage pivot = items[ count - 1 ];
	int low = 0;
	for ( int i = 0; i < count - 1; ++i )
	{
		if ( tdPerimeterPred( items + i, &pivot ) )
		{
			tdRawImage tmp = items[ i ];
			items[ i ] = items[ low ];
			items[ low ] = tmp;
			low++;
		}
	}

	items[ count - 1 ] = items[ low ];
	items[ low ] = pivot;
	tdQSort( items, low );
	tdQSort( items + low + 1, count - 1 - low );
}

#define TD_MAX_ATLAS_NODES 1024

int tdMakeAtlas( const char* out_path_image, const char* out_path_atlas_txt, int atlasWidth, int atlasHeight, tdImage* pngs, int png_count, const char** names )
{
	float w0, h0, div, wTol, hTol;
	int atlasImageSize, atlasStride, sp;
	FILE* fp = fopen( out_path_atlas_txt, "wt" );
	char* atlasPixels = 0;
	tdRawImage* images = (tdRawImage*)alloca( sizeof( tdRawImage ) * png_count );
	tdAtlasNode* nodes = (tdAtlasNode*)malloc( sizeof( tdAtlasNode ) * TD_MAX_ATLAS_NODES );

	for ( int i = 0; i < png_count; ++i )
	{
		tdImage* png = pngs + i;
		tdRawImage* image = images + i;
		image->png = png;
		image->fit = 0;
		image->size = tdV2I( png->w, png->h );
		image->name = out_path_image;
	}

	// Sort PNGs from largest to smallest
	tdQSort( images, png_count );

	// stack pointer, the stack is the nodes array which we will
	// allocate nodes from as necessary.
	sp = 1;

	nodes[ 0 ].min = tdV2I( 0, 0 );
	nodes[ 0 ].max = tdV2I( atlasWidth, atlasHeight );
	nodes[ 0 ].size = tdV2I( atlasWidth, atlasHeight );

	// Nodes represent empty space in the atlas. Placing a texture into the
	// atlas involves splitting a node into two smaller pieces (or, if a
	// perfect fit is found, deleting the node).
	for ( int i = 0; i < png_count; ++i )
	{
		tdRawImage* image = images + i;
		tdImage* png = image->png;
		int width = png->w;
		int height = png->h;
		tdAtlasNode *bestFit = tdBestFit( sp, png, nodes );

		if ( TD_ATLAS_MUST_FIT )
			TD_CHECK( bestFit, "Not enough room to place image in atlas." );

		image->min = bestFit->min;
		image->max = tdAdd( image->min, image->size );

		if ( bestFit->size.x == width && bestFit->size.y == height )
		{
			tdAtlasNode* lastNode = nodes + --sp;
			*bestFit = *lastNode;
			image->fit = 1;

			continue;
		}

		image->fit = 1;

		TD_ASSERT( sp < TD_MAX_ATLAS_NODES ); // make TD_MAX_ATLAS_NODES bigger
		tdAtlasNode* newNode = nodes + sp++;
		newNode->min = bestFit->min;

		// Split bestFit along x or y, whichever minimizes
		// fragmentation of empty space
		tdv2i d = tdSub( bestFit->size, tdV2I( width, height ) );
		if ( d.x < d.y )
		{
			newNode->size.x = d.x;
			newNode->size.y = height;
			newNode->min.x += width;

			bestFit->size.y = d.y;
			bestFit->min.y += height;
		}

		else
		{
			newNode->size.x = width;
			newNode->size.y = d.y;
			newNode->min.y += height;

			bestFit->size.x = d.x;
			bestFit->min.x += width;
		}

		newNode->max = tdAdd( newNode->min, newNode->size );
	}

	// Write the final atlas image, empty space as white
	atlasStride = atlasWidth * TD_BPP;
	atlasImageSize = atlasWidth * atlasHeight * TD_BPP;
	atlasPixels = (char*)malloc( atlasImageSize );
	memset( atlasPixels, 0xFFFFFFFF, atlasImageSize );

	for ( int i = 0; i < png_count; ++i )
	{
		tdRawImage* image = images + i;

		if ( image->fit )
		{
			tdImage* png = image->png;
			char* pixels = (char*)png->pix;
			tdv2i min = image->min;
			tdv2i max = image->max;
			int atlasOffset = min.x * TD_BPP;
			int texStride = png->w * TD_BPP;

			for ( int row = min.y, y = 0; row < max.y; ++row, ++y )
			{
				char* rowPtr = atlasPixels + (row * atlasStride + atlasOffset);
				memcpy( rowPtr, pixels + y * texStride, texStride );
			}
		}
	}

	// Save atlas image PNG to disk
	tdImage atlasImage;
	atlasImage.w = atlasWidth;
	atlasImage.h = atlasHeight;
	atlasImage.pix = (tdPixel*)atlasPixels;
	tdPremultiply( &atlasImage );
	tdSavePNG( out_path_image, &atlasImage );

	// squeeze UVs inward by 128th of a pixel
	// this prevents atlas bleeding. tune as necessary for good results.
	w0 = 1.0f / (float)(atlasWidth);
	h0 = 1.0f / (float)(atlasHeight);
	div = 1.0f / 128.0f;
	wTol = w0 * div;
	hTol = h0 * div;

	fprintf( fp, "%s\n%d\n\n", out_path_image, png_count );

	for ( int i = 0; i < png_count; ++i )
	{
		tdRawImage* image = images + i;
		tdImage* png = image->png;

		if ( image->fit )
		{
			tdv2i min = image->min;
			tdv2i max = image->max;

			int width = png->w;
			int height = png->h;
			float min_x = (float)min.x * w0;
			float min_y = (float)min.y * h0;
			float max_x = (float)max.x * w0 - wTol;
			float max_y = (float)max.y * h0 - hTol;

			// flip image on y axis
			if ( TD_ATLAS_FLIP_Y_AXIS_FOR_UV )
			{
				float tmp = min_y;
				min_y = max_y;
				max_y = tmp;
			}

			if ( names ) fprintf( fp, "{ \"%s\", w = %d, h = %d, u = { %.10f, %.10f }, v = { %.10f, %.10f } }\n", names[ i ], width, height, min_x, min_y, max_x, max_y );
			else fprintf( fp, "{ w = %d, h = %d, u = { %.10f, %.10f }, v = { %.10f, %.10f } }\n", width, height, min_x, min_y, max_x, max_y );
		}
	}

	fclose( fp );
	free( atlasPixels );
	free( nodes );
	return 1;

td_err:
	fclose( fp );
	free( atlasPixels );
	free( nodes );
	return 0;
}

#endif // TD_ATLAS

#endif // TD_PNG

#endif // TINYDEFLATE_IMPL

/*
	This is free and unencumbered software released into the public domain.

	Our intent is that anyone is free to copy and use this software,
	for any purpose, in any form, and by any means.

	The authors dedicate any and all copyright interest in the software
	to the public domain, at their own expense for the betterment of mankind.

	The software is provided "as is", without any kind of warranty, including
	any implied warranty. If it breaks, you get to keep both pieces.
*/
