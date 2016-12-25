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
			tdMakeAtlas( "atlas.png", "atlas.txt", 256, 256, imgs, imgs_count );
			// just pass an array of pointers to images, and the array count
			// outputs a png along with a txt file. The txt contains UV info in
			// a very easy to parse format.

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
int tdMakeAtlas( const char* out_path_image, const char* out_path_atlas_txt, int atlasWidth, int atlasHeight, tdImage* pngs, int png_count );

// these two functions return tdImage::pix as 0 in event of errors
tdImage tdLoadPNG( const char *fileName );
tdImage tdLoadPNGMem( const void *png_data, int png_length );

// read this whenever one of the above functions returns an error
extern const char* g_tdDeflateErrorReason;

#define TD_DEBUG_CHECKS 1
#if TD_DEBUG_CHECKS

	#include <assert.h>
	#define TD_ASSERT assert

#else

	#define TD_ASSERT( ... )

#endif

#define TINYDEFLATE_H
#endif

#ifdef TINYDEFLATE_IMPL

#include <stdio.h>  // fopen, fclose, etc.
#include <stdlib.h> // malloc, free, calloc
#include <string.h> // memcpy

const char* g_tdDeflateErrorReason;
#define TD_CHECK( X, Y ) do { if ( !(X) ) { g_tdDeflateErrorReason = Y; return 0; } } while ( 0 )
#define TD_CALL( X ) do { if ( !(X) ) goto td_error; } while ( 0 )
#define TD_LOOKUP_BITS 9
#define TD_LOOKUP_COUNT (1 << TD_LOOKUP_BITS)
#define TD_LOOKUP_MASK (TD_LOOKUP_COUNT - 1)

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
uint32_t g_tdDistBase[ 30 + 2 ] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 }; // 3.2.5

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

	uint16_t lookup[ TD_LOOKUP_COUNT ];
	uint32_t lit[ 288 ];
	uint32_t dst[ 32 ];
	uint32_t len[ 19 ];
	uint32_t nlit;
	uint32_t ndst;
	uint32_t nlen;
} tdIState;

TD_INLINE static int tdWouldOverflow( tdIState* s, int num_bits )
{
	return s->bits_left - num_bits < 0;
}

TD_INLINE static char* tdPtr( tdIState* s )
{
	TD_ASSERT( !(s->bits_left & 7) );
	return ((char*)s->words) + s->count;
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
	return bits;
}

TD_INLINE static uint32_t tdReadBits( tdIState* s, int num_bits_to_read )
{
	TD_ASSERT( num_bits_to_read <= 32 );
	TD_ASSERT( num_bits_to_read >= 0 );
	TD_ASSERT( s->bits_left > 0 );
	TD_ASSERT( s->count <= 64 );
	TD_ASSERT( !tdWouldOverflow( s, num_bits_to_read ) );
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

// RFC 1951 section 3.2.2
// Slots arrays starts as "bl_count"
// Added 2.5) to stransform slots from "bl_count" into an array that
// represents first indices for each code length in the final tree
static int tdBuild( tdIState* s, uint32_t* tree, uint8_t* lens, int sym_count )
{
	int slots[ 16 ] = { 0 };

	// 1) Count the number of codes for each code length
	for ( int n = 0; n < sym_count; n++ )
		slots[ lens[ n ] ]++;

	// 2) Find numerical value of the smallest code for each code length
	int codes[ 16 ];
	codes[ 0 ] = 0;

	for ( int i = 0; i < 15; i++ )
		codes[ i + 1 ] = (codes[ i ] + slots[ i ]) << 1;

	// 2.5)
	// For each different length record where the first index starts
	// Shift array to right one index
	for ( int i = 1, sum = 0, prev = 0; i <= 15; i++ )
	{
		sum += slots[ i ];
		slots[ i ] = prev;
		prev = sum;
	}

	// 3)
	// Assign numerical values to all codes, using consecutive values
	// for all codes of the same length with the base values determined
	// at step 2. Codes that are never used (which have a bit length of
	// zero) must not be assigned a value. Two encodings are used to
	// later retrieve lengths. One via lookup table, and a slower alt-
	// ernative using a binary search. Tables are not constructed for
	// smaller trees (distance, and first lenlen tree in dynamic case).
	if ( s ) memset( s->lookup, 0, sizeof( 512 * sizeof( uint32_t ) ) );
	for ( int i = 0; i < sym_count; ++i )
	{
		int len = lens[ i ];

		if ( len != 0 )
		{
			TD_ASSERT( len < 16 );
			uint32_t code = codes[ len ]++;
			uint32_t slot = slots[ len ]++;
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

	int max_index = slots[ 15 ];
	return max_index;
}

static int tdStored( tdIState* s )
{
	// 3.2.3
	// skip any remaining bits in current partially processed byte
	tdReadBits( s, s->count & 7 );

	// 3.2.4
	// read LEN and NLEN, should complement each other
	uint32_t LEN = tdReadBits( s, 16 );
	uint32_t NLEN = tdReadBits( s, 16 );
	TD_CHECK( LEN == ~NLEN, "Failed to find LEN and NLEN as complements within stored (uncompressed) stream." );
	TD_CHECK( s->bits_left * 8 <= (int)LEN, "Stored block extends beyond end of input stream." );
	memcpy( s->out, tdPtr( s ), LEN );
	s->out += LEN;
	return 1;
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

	tdConsumeBits( s, key & 0xF );
	return (key >> 4) & 0xFFF;
}

static int tdTryLookup( tdIState* s, uint32_t* tree, int hi )
{
	uint64_t bits = tdPeakBits( s, 16 );
	int index = bits & TD_LOOKUP_MASK;
	uint32_t code = s->lookup[ index ];

	if ( code )
	{
		tdConsumeBits( s, code >> 9 );
		return code & TD_LOOKUP_MASK;
	}

	return tdDecode( s, tree, hi );
}

// 3.2.7
static int tdDynamic( tdIState* s )
{
	uint8_t lenlens[ 19 ] = { 0 };

	int nlit = 257 + tdReadBits( s, 5 );
	int ndist = 1 + tdReadBits( s, 5 );
	int nlen = 4 + tdReadBits( s, 4 );

	for ( int i = 0 ; i < nlen; ++i )
		lenlens[ g_tdPermutationOrder[ i ] ] = (uint8_t)tdReadBits( s, 3 );

	// Build the tree for decoding code lengths
	s->nlen = tdBuild( 0, s->len, lenlens, 19 );
	uint8_t lens[ 288 + 32 ];

	for ( int n = 0; n < nlit + ndist; )
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
	s->ndst = tdBuild( 0, s->dst, lens + nlit, ndist );
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
			if ( s->out + length > s->out_end ) __debugbreak( );
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

	int first_bytes = (int)((size_t)in & 3);
	s->words = (uint32_t*)((char*)in + first_bytes);
	s->last_bits = ((in_bytes - first_bytes) & 3) * 8;
	s->final_bytes = (char*)in + in_bytes - s->last_bits;

	for ( int i = 0; i < first_bytes; ++i )
		s->bits |= (uint64_t)(((uint8_t*)in)[ i ]) << (i * 8);
	s->count = first_bytes * 8;

	s->out = (char*)out;
	s->out_end = s->out + out_bytes;

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
	}
	while ( !bfinal );

	free( s );
	return 1;

td_error:
	free( s );
	return 0;
}

#define TD_WINDOW_SIZE       (1024 * 32)
#define TD_HASH_COUNT        TD_WINDOW_SIZE
#define TD_BLOCK_BUFFER_SIZE (TD_WINDOW_SIZE * 4)

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

typedef struct tdEntry
{
	uint32_t h;
	//uint16_t len;
	char* start;
	//char* end;
	struct tdEntry* next;
} tdEntry;

typedef struct
{
	char* in;
	char* in_end;
	char* out;
	char* out_end;
	char* window;

	int max_entry_len;
	int do_lazy_search;

	tdEntry* free_list;
	tdEntry* buckets[ TD_HASH_COUNT ];
	tdEntry entries[ TD_HASH_COUNT ];
	char block_buffer[ TD_BLOCK_BUFFER_SIZE ];
} tdDState;

typedef struct
{
	int max_entry_len;
	int do_lazy_search;
} tdDeflateOptions;

#undef TD_CHECK
#define TD_FAIL( ) do { goto td_error; } while ( 0 )
#define TD_CHECK( X, Y ) do { if ( !(X) ) { g_tdDeflateErrorReason = Y; TD_FAIL( ); } } while ( 0 )

TD_INLINE static void tdInsert( tdDState* s, char* start, char* end, uint32_t h )
{
	TD_ASSERT( s->free_list );
	tdEntry* entry = s->free_list;
	s->free_list = entry->next;
	entry->h = h;
	entry->len = end - start;
	entry->start = start;
	tdEntry* chain = s->buckets + h;
	entry->next = chain;
	chain = entry;
}

void* tdDeflateMem( void* in, int bytes, int* out_bytes, tdDeflateOptions* options )
{
	tdDState* s = (tdDState*)calloc( 1, sizeof( tdDState ) );
	s->in = (char*)in;
	s->in_end = s->in + bytes;
	s->out = (char*)malloc( bytes );
	s->out_end = s->out + bytes;

	s->max_entry_len = options->max_entry_len;
	s->do_lazy_search = options->do_lazy_search;

	for ( int i = 0; i < TD_HASH_COUNT - 1; ++i )
		s->entries[ i ].next = s->entries + i + 1;
	s->entries[ TD_HASH_COUNT - 1 ].next = 0;
	s->free_list = s->entries;
	memset( s->buckets, 0, sizeof( s->buckets ) );
	s->window = s->block_buffer;

	// write out 0
	// write out 1

	// write out first three bytes
	for ( int i = 0; i < 3; ++i )
		*s->window++ = *s->in++;

	while ( s->in != s->in_end - 3 )
	{
		// move sliding window over one byte
		// make 3 byte hash
		char* start = s->in;
		char* end = start + 3;
		char byte = *s->in++;
		uint32_t h = djb2( start, end ) % TD_HASH_COUNT;
		tdEntry* chain = s->buckets + h;
		
		TD_ASSERT( s->free_list );
		tdEntry* entry = s->free_list;
		s->free_list = entry->next;
		entry->h = h;
		entry->start = start;
		entry->next = chain;
		s->buckets[ h ] = entry;

		if ( chain )
		{
			// loop over chain
				// keep track of best bit reduction
				// discard matches that are too old

			// make <len,dst> pair
			// inc pair count
			// if pair does not fit in block_buffer
				// dump block_buffer

			// else if pair count > threshold? && run count
				// calc entropy
				// if entropy * factor? > threshold?
					// make tree, check depth
					// if depth > 15
						// PANIC
					// if depth > threshold?
						// dump block_buffer
		}

		else
		{
			// write out literal
		}
	}

	// write out last 3 bytes??

	int out_size = 0;
	if ( out_bytes ) *out_bytes = out_size;
	return 0;
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
	TD_CHECK( out, "Unable to allocate out buffer." );

	free( in );
	return out;

td_error:
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

td_error:
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

int tdMakeAtlas( const char* out_path_image, const char* out_path_atlas_txt, int atlasWidth, int atlasHeight, tdImage* pngs, int png_count )
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

			fprintf( fp, "{ w = %d, h = %d, u = { %.10f, %.10f }, v = { %.10f, %.10f } }\n", width, height, min_x, min_y, max_x, max_y );
		}
	}

	fclose( fp );
	free( atlasPixels );
	free( nodes );
	return 1;

td_error:
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
