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

TD_INLINE static int tdWouldOverflow( int bits_left, int num_bits )
{
	return bits_left - num_bits < 0;
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
	printf( "C val, bits: %d, %d\n", bits, num_bits_to_read );
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
// Added 2.5) to stransform slots from "bl_count" into an array that
// represents first indices for each code length in the final tree
static int tdBuild( tdIState* s, uint32_t* tree, uint8_t* lens, int sym_count )
{
	int n, codes[16], first[16], counts[16]={0};

	// Frequency count.
	for (n=0;n<sym_count;n++) counts[lens[n]]++;

	// Distribute codes.
	counts[0] = codes[0] = first[0] = 0;
	for (n=1;n<=15;n++) {
		codes[n] = (codes[n-1] + counts[n-1]) << 1;
		first[n] = first[n-1] + counts[n-1];
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

	int code = tdConsumeBits( s, key & 0xF );
	(void)code;
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
	int ndst = 1 + tdReadBits( s, 5 );
	int nlen = 4 + tdReadBits( s, 4 );

	for ( int i = 0 ; i < nlen; ++i )
		lenlens[ g_tdPermutationOrder[ i ] ] = (uint8_t)tdReadBits( s, 3 );

	// Build the tree for decoding code lengths
	int lenlens2[19] = { 0 };
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
	printf( "the lz77 data\n" );
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

	int first_bytes = (int)((int)in & 3);
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

// if TD_HASH_COUNT is set to >= TD_WINDOW_SIZE we can use a rolling buffer
#define TD_WINDOW_SIZE       (1024 * 32)
#define TD_HASH_COUNT        TD_WINDOW_SIZE
#define TD_ENTRY_BUFFER_SIZE (TD_WINDOW_SIZE * 4)

#if TD_HASH_COUNT < TD_WINDOW_SIZE
#error rolling buffer implementation requires worst case size minimum for hash entry memory
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
	char* start;
	struct tdHash* next;
} tdHash;

typedef struct
{
	char base_len;
	char base_dst;
	uint16_t symbol_index;
	uint16_t len;
	int dst;
} tdEntry;

typedef struct
{
	int key;
	int val;
} tdItem;

typedef struct
{
	union
	{
		uint16_t freq;
		uint16_t code;
	};
	union
	{
		uint16_t parent;
		uint16_t len;
	};

	uint16_t depth;
	uint16_t original;

#if TD_DEBUG_CHECKS
	uint16_t left;
	uint16_t right;
#endif
} tdNode;

typedef struct
{
	uint16_t freq;
	uint16_t code;
	uint16_t len;
} tdLeaf;

typedef struct
{
	char* in;
	char* in_end;
	char* out;
	char* out_end;
	char* window;
	tdLeaf len[ 286 ];
	tdLeaf dst[ 30 ];

	uint64_t bits;
	int count;
	uint32_t* words;
	int word_count;
	int word_index;
	int bits_left;

	int max_chain_len;
	int do_lazy_search;

	int hash_rolling;
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

#undef TD_CHECK
#define TD_FAIL( ) do { goto td_error; } while ( 0 )
#define TD_CHECK( X, Y ) do { if ( !(X) ) { g_tdDeflateErrorReason = Y; TD_FAIL( ); } } while ( 0 )

static void tdWriteBits( tdDState* s, uint32_t value, uint32_t num_bits_to_write )
{
	TD_ASSERT( num_bits_to_write <= 32 );
	TD_ASSERT( s->bits_left > 0 );
	TD_ASSERT( s->count <= 32 );
	TD_ASSERT( !tdWouldOverflow( s->bits_left, num_bits_to_write ) );

	printf( "val, bits: %d, %d\n", value, num_bits_to_write );
	s->bits |= (uint64_t)(value & (((uint64_t)1 << num_bits_to_write) - 1)) << s->count;
	s->count += num_bits_to_write;
	s->bits_left -= num_bits_to_write;

	if ( s->count >= 32 )
	{
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
	for ( int i = 0; i < 31; ++i )
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
	TD_ASSERT( len_index >=0 && len_index <= 29 );
	TD_ASSERT( dst_index >=0 && dst_index <= 31 );
	*base_len = len_index;
	*base_dst = dst_index;
}

TD_INLINE static int tdMatchCost( int len, int len_bits, int dst_bits )
{
	int match_bits = len * 8;
	return (len_bits + dst_bits) - match_bits;
}

#include <math.h> // log2f

TD_INLINE static float tdEntropy( tdLeaf freq_in[ 286 ] )
{
	int sum = 0;
	for ( int i = 0; i < 286; ++i ) sum += freq_in[ i ].freq;
	float fsum = (float)sum;
	float entropy = 0;
	for ( int i = 0; i < 286; ++i )
	{
		int freq_int = freq_in[ i ].freq;
		if ( freq_int )
		{
			float freq = (float)freq_int / fsum;
			entropy -= freq * log2f( freq );
		}
	}
	return entropy;
}

TD_INLINE static void tdLiteral( tdDState* s, int symbol )
{
	tdEntry entry = { 0 };
	entry.symbol_index = symbol;
	s->window++;
	s->entries[ s->entry_count++ ] = entry;
	s->len[ symbol ].freq++;
}

TD_INLINE static int tdSmaller( tdItem a, tdItem b, tdNode* nodes )
{
	if ( a.key < b.key ) return 1;
	else if ( a.key == b.key && nodes[ a.val ].depth <= nodes[ b.val ].depth ) return 1;
	return 0;
}

static void tdCascade( tdItem* items, int N, int i, tdNode* nodes )
{
	int min = i;
	tdItem min_val = items[ i ];
	int i2 = 2 * i;

	while ( i2 < N )
	{
		int left = i2;
		int right = i2 + 1;

		if ( tdSmaller( items[ left ], min_val, nodes ) ) min = left;
		if ( right < N && tdSmaller( items[ right ], items[ min ], nodes ) ) min = right;
		if ( min == i ) break;

		items[ i ] = items[ min ];
		items[ min ] = min_val;
		i = min;
		i2 = 2 * i;
	}
}

static void tdMakeHeap( tdItem* items, int count, tdNode* nodes )
{
	for ( int i = count / 2; i > 0; --i )
		tdCascade( items, count, i, nodes );
}

static tdItem tdPopHeap( tdItem* items, int count, tdNode* nodes )
{
	tdItem root = items[ 1 ];
	items[ 1 ] = items[ count - 1 ];
	tdCascade( items, count - 1, 1, nodes );
	return root;
}

static void tdPushHeap( tdItem* items, int count, tdItem a, tdNode* nodes )
{
	int i = count;
	items[ count ] = a;

	while ( i )
	{
		int j = i / 2;
		tdItem child = items[ i ];
		tdItem parent = items[ j ];

		if ( tdSmaller( child, parent, nodes ) )
		{
			items[ i ] = parent;
			items[ j ] = child;
		}

		i = j;
	}
}

#if TD_DEBUG_CHECKS
	#define tdAssertHeap( data, N, i, nodes ) tdAssertHeap_internal( data, N , i, nodes )
#else
	#define tdAssertHeap( ... )
#endif
void tdAssertHeap_internal( tdItem* data, int N, int i, tdNode* nodes )
{
	int left = i * 2;
	int right = left + 1;

	if ( left < N )
	{
		tdItem iVal = data[ i ];
		tdItem lVal = data[ left ];
		TD_ASSERT( tdSmaller( iVal, lVal, nodes ) );

		if ( right < N )
		{
			tdItem rVal = data[ right ];
			TD_ASSERT( tdSmaller( iVal, rVal, nodes ) );
			tdAssertHeap( data, N, left, nodes );
			tdAssertHeap( data, N, right, nodes );
		}
	}
}

void tdPrintHeap( tdItem* data, int count )
{
	int i = 1;
	int j = 2;
	int N = count;
	do
	{
		if ( j > N ) j = N;
		for ( int index = 0; index < N / 2 - i; ++index ) printf( "    " );
		for ( int index = i; index < j; ++index ) printf( "(%2d %2d) ", data[ index ].key, data[ index ].val );
		printf( "\n" );

		i *= 2;
		j = i * 2;
	}
	while ( i < N );
	printf( "\n" );
}

TD_INLINE static tdMax( int a, int b )
{
	return a > b ? a : b;
}

char depth[ 2056 ];
int di;

void Push( char c )
{
	depth[ di++ ] = ' ';
	depth[ di++ ] = c;
	depth[ di++ ] = ' ';
	depth[ di++ ] = ' ';
	depth[ di ] = 0;
}
 
void tdPop( )
{
	depth[ di -= 4 ] = 0;
}

void PrintCode( int code, int len, int L )
{
	printf( "(" );
	for ( int i = 0; i < len; ++i ) printf( "%d", code & (1 << i) ? 1 : 0 );
	printf( ", %d)", len );
	if ( L ) printf( " L" );
	printf( "\n" );
}
 
void tdPrint( tdNode* tree, tdNode* nodes )
{
	if ( tree->original != (uint16_t)~0 ) PrintCode( tree->code, tree->len, 1 );
	else PrintCode( tree->freq, tree->len, 0 );
 
	if ( tree->left != (uint16_t)~0 )
	{
		if ( tree->right != (uint16_t)~0 ) printf( "%s %c%c%c", depth, 195, 196, 196 );
		else printf( "%s %c%c%c", depth, 192, 196, 196 );
		Push( 179 );
		tdPrint( nodes + tree->left, nodes );
		tdPop( );
	}

	if ( tree->right != (uint16_t)~0 )
	{
		printf( "%s %c%c%c", depth, 192, 196, 196 );
		Push( ' ' );
		tdPrint( nodes + tree->right, nodes );
		tdPop( );
	}
}

typedef struct Node Node;

/*
Nodes forming chains. Also used to represent leaves.
*/
struct Node {
  int weight;  /* Total weight (symbol count) of this chain. */
  Node* tail;  /* Previous node(s) of this chain, or 0 if none. */
  int count;  /* Leaf symbol index, or number of leaves before this chain. */
};

/*
Memory pool for nodes.
*/
typedef struct NodePool {
  Node* next;  /* Pointer to a free node in the pool. */
} NodePool;

/*
Initializes a chain node with the given values and marks it as in use.
*/
static void InitNode(int weight, int count, Node* tail, Node* node) {
  node->weight = weight;
  node->count = count;
  node->tail = tail;
}

/*
Performs a Boundary Package-Merge step. Puts a new chain in the given list. The
new chain is, depending on the weights, a leaf or a combination of two chains
from the previous list.
lists: The lists of chains.
maxbits: Number of lists.
leaves: The leaves, one per symbol.
numsymbols: Number of leaves.
pool: the node memory pool.
index: The index of the list in which a new chain or leaf is required.
*/
static void BoundaryPM(Node* (*lists)[2], Node* leaves, int numsymbols,
                       NodePool* pool, int index) {
  Node* newchain;
  Node* oldchain;
  int lastcount = lists[index][1]->count;  /* Count of last chain of list. */

  if (index == 0 && lastcount >= numsymbols) return;

  newchain = pool->next++;
  oldchain = lists[index][1];

  /* These are set up before the recursive calls below, so that there is a list
  pointing to the new node, to let the garbage collection know it's in use. */
  lists[index][0] = oldchain;
  lists[index][1] = newchain;

  if (index == 0) {
    /* New leaf node in list 0. */
    InitNode(leaves[lastcount].weight, lastcount + 1, 0, newchain);
  } else {
    int sum = lists[index - 1][0]->weight + lists[index - 1][1]->weight;
    if (lastcount < numsymbols && sum > leaves[lastcount].weight) {
      /* New leaf inserted in list, so count is incremented. */
      InitNode(leaves[lastcount].weight, lastcount + 1, oldchain->tail,
          newchain);
    } else {
      InitNode(sum, lastcount, lists[index - 1][1], newchain);
      /* Two lookahead chains of previous list used up, create new ones. */
      BoundaryPM(lists, leaves, numsymbols, pool, index - 1);
      BoundaryPM(lists, leaves, numsymbols, pool, index - 1);
    }
  }
}

static void BoundaryPMFinal(Node* (*lists)[2],
    Node* leaves, int numsymbols, NodePool* pool, int index) {
  int lastcount = lists[index][1]->count;  /* Count of last chain of list. */

  int sum = lists[index - 1][0]->weight + lists[index - 1][1]->weight;

  if (lastcount < numsymbols && sum > leaves[lastcount].weight) {
    Node* newchain = pool->next;
    Node* oldchain = lists[index][1]->tail;

    lists[index][1] = newchain;
    newchain->count = lastcount + 1;
    newchain->tail = oldchain;
  } else {
    lists[index][1]->tail = lists[index - 1][1];
  }
}

/*
Initializes each list with as lookahead chains the two leaves with lowest
weights.
*/
static void InitLists(
    NodePool* pool, const Node* leaves, int maxbits, Node* (*lists)[2]) {
  int i;
  Node* node0 = pool->next++;
  Node* node1 = pool->next++;
  InitNode(leaves[0].weight, 1, 0, node0);
  InitNode(leaves[1].weight, 2, 0, node1);
  for (i = 0; i < maxbits; i++) {
    lists[i][0] = node0;
    lists[i][1] = node1;
  }
}

/*
Converts result of boundary package-merge to the bitlengths. The result in the
last chain of the last list contains the amount of active leaves in each list.
chain: Chain to extract the bit length from (last chain from last list).
*/
static void ExtractBitLengths(Node* chain, Node* leaves, unsigned* bitlengths) {
  int counts[16] = {0};
  unsigned end = 16;
  unsigned ptr = 15;
  unsigned value = 1;
  Node* node;
  int val;

  for (node = chain; node; node = node->tail) {
    counts[--end] = node->count;
  }

  val = counts[15];
  while (ptr >= end) {
    for (; val > counts[ptr - 1]; val--) {
      bitlengths[leaves[val - 1].count] = value;
    }
    ptr--;
    value++;
  }
}

/*
Comparator for sorting the leaves. Has the function signature for qsort.
*/
static int LeafComparator(const void* a, const void* b) {
  return ((const Node*)a)->weight - ((const Node*)b)->weight;
}

int ZopfliLengthLimitedCodeLengths(
    const int* frequencies, int n, int maxbits, unsigned* bitlengths) {
  NodePool pool;
  int i;
  int numsymbols = 0;  /* Amount of symbols with frequency > 0. */
  int numBoundaryPMRuns;
  Node* nodes;

  /* Array of lists of chains. Each list requires only two lookahead chains at
  a time, so each list is a array of two Node*'s. */
  Node* (*lists)[2];

  /* One leaf per symbol. Only numsymbols leaves will be used. */
  Node* leaves = (Node*)malloc(n * sizeof(*leaves));

  /* Initialize all bitlengths at 0. */
  for (i = 0; i < n; i++) {
    bitlengths[i] = 0;
  }

  /* Count used symbols and place them in the leaves. */
  for (i = 0; i < n; i++) {
    if (frequencies[i]) {
      leaves[numsymbols].weight = frequencies[i];
      leaves[numsymbols].count = i;  /* Index of symbol this leaf represents. */
      numsymbols++;
    }
  }

  /* Check special cases and error conditions. */
  if ((1 << maxbits) < numsymbols) {
    free(leaves);
    return 1;  /* Error, too few maxbits to represent symbols. */
  }
  if (numsymbols == 0) {
    free(leaves);
    return 0;  /* No symbols at all. OK. */
  }
  if (numsymbols == 1) {
    bitlengths[leaves[0].count] = 1;
    free(leaves);
    return 0;  /* Only one symbol, give it bitlength 1, not 0. OK. */
  }
  if (numsymbols == 2) {
    bitlengths[leaves[0].count]++;
    bitlengths[leaves[1].count]++;
    free(leaves);
    return 0;
  }

  /* Sort the leaves from lightest to heaviest. Add count into the same
  variable for stable sorting. */
  for (i = 0; i < numsymbols; i++) {
    if (leaves[i].weight >=
        ((int)1 << (sizeof(leaves[0].weight) * CHAR_BIT - 9))) {
      free(leaves);
      return 1;  /* Error, we need 9 bits for the count. */
    }
    leaves[i].weight = (leaves[i].weight << 9) | leaves[i].count;
  }
  qsort(leaves, numsymbols, sizeof(Node), LeafComparator);
  for (i = 0; i < numsymbols; i++) {
    leaves[i].weight >>= 9;
  }

  if (numsymbols - 1 < maxbits) {
    maxbits = numsymbols - 1;
  }

  /* Initialize node memory pool. */
  nodes = (Node*)malloc(maxbits * 2 * numsymbols * sizeof(Node));
  pool.next = nodes;

  lists = (Node* (*)[2])malloc(maxbits * sizeof(*lists));
  InitLists(&pool, leaves, maxbits, lists);

  /* In the last list, 2 * numsymbols - 2 active chains need to be created. Two
  are already created in the initialization. Each BoundaryPM run creates one. */
  numBoundaryPMRuns = 2 * numsymbols - 4;
  for (i = 0; i < numBoundaryPMRuns - 1; i++) {
    BoundaryPM(lists, leaves, numsymbols, &pool, maxbits - 1);
  }
  BoundaryPMFinal(lists, leaves, numsymbols, &pool, maxbits - 1);

  ExtractBitLengths(lists[maxbits - 1][1], leaves, bitlengths);

  free(lists);
  free(leaves);
  free(nodes);
  return 0;  /* OK. */
}

void ZopfliLengthsToSymbols(const unsigned* lengths, int n, unsigned maxbits,
                            unsigned* symbols) {
  int* bl_count = (int*)malloc(sizeof(int) * (maxbits + 1));
  int* next_code = (int*)malloc(sizeof(int) * (maxbits + 1));
  unsigned bits, i;
  unsigned code;

  for (i = 0; i < (unsigned)n; i++) {
    symbols[i] = 0;
  }

  /* 1) Count the number of codes for each code length. Let bl_count[N] be the
  number of codes of length N, N >= 1. */
  for (bits = 0; bits <= maxbits; bits++) {
    bl_count[bits] = 0;
  }
  for (i = 0; i < (unsigned)n; i++) {
    assert(lengths[i] <= maxbits);
    bl_count[lengths[i]]++;
  }
  /* 2) Find the numerical value of the smallest code for each code length. */
  code = 0;
  bl_count[0] = 0;
  for (bits = 1; bits <= maxbits; bits++) {
    code = (code + bl_count[bits-1]) << 1;
    next_code[bits] = code;
  }
  /* 3) Assign numerical values to all codes, using consecutive values for all
  codes of the same length with the base values determined at step 2. */
  for (i = 0;  i < (unsigned)n; i++) {
    unsigned len = lengths[i];
    if (len != 0) {
      symbols[i] = next_code[len];
      next_code[len]++;
    }
  }

  free(bl_count);
  free(next_code);
}

// WORKING HERE
// this func isn't working correctly
// maybe copy zopfli
void tdMakeTree( tdLeaf* tree, int count )
{
#if 1
	int freq[ 288 ] = { 0 };
	int lens[ 288 ] = { 0 };
	int code[ 288 ] = { 0 };
	for ( int i = 0; i < count; ++i ) freq[ i ] = tree[ i ].freq;
	ZopfliLengthLimitedCodeLengths( freq, count, 15, lens );
	ZopfliLengthsToSymbols(lens, count, 15, code);
	for ( int i = 0; i < count; ++i )
	{
		tree[ i ].code = code[ i ];
		tree[ i ].freq = freq[ i ];
		tree[ i ].len = lens[ i ];
	}
	return;
#endif

	int bits_saved = 0; // TODO: calc bits saved?
	// setup leaf nodes with frequencies set
	int node_count = 0;
	int item_count = 1;
	// TODO: figure out correct upper bounds for all arrays in this function
	// for now the upper bounds are placed really high as guesses that are 100% safe.
	// surely they can be reduced to smaller sizes.
	tdNode nodes[ 286 * 2 + 1 ];
	tdItem items[ 286 ];

	for ( int i = 0; i < count; ++i )
	{
		int freq = tree[ i ].freq;
		if ( freq )
		{
			//sum += freq;
			TD_ASSERT( item_count < 286 );
			tdItem* item = items + item_count++;
			item->key = freq;
			item->val = node_count++;
			tdNode* node = nodes + item->val;
			node->freq = freq;
			node->parent = ~0;
			node->depth = 0;
			node->original = i;
			if ( TD_DEBUG_CHECKS )
			{
				node->left = ~0;
				node->right = ~0;
			}
		}
	}

	// construct heap of leaves
	tdMakeHeap( items, item_count, nodes );
	tdAssertHeap( items, item_count, 1, nodes );
	uint16_t sorted[ 286 * 2 + 1 ];
	int sorted_count = 0;
	int leaf_index = item_count - 1;

	// Huffman algorithm
	// tdItem::val is index into node array, key as frequency of node
	// Also use the pop function to sort the nodes from lowest to highest frequency
	do
	{
		tdItem a = tdPopHeap( items, item_count--, nodes );
		tdItem b = tdPopHeap( items, item_count--, nodes );
		tdItem c;
		tdNode node_a = nodes[ a.val ];
		tdNode node_b = nodes[ b.val ];
		tdNode node_c;
		sorted[ sorted_count++ ] = a.val;
		sorted[ sorted_count++ ] = b.val;
		int c_index = node_count++;
		nodes[ a.val ].parent = c_index;
		nodes[ b.val ].parent = c_index;
		node_c.depth = tdMax( node_a.depth, node_b.depth ) + 1;
		node_c.parent = ~0;
		node_c.freq = node_a.freq + node_b.freq;
		node_c.original = ~0;
		if ( TD_DEBUG_CHECKS )
		{
			node_c.left = a.val;
			node_c.right = b.val;
		}
		nodes[ c_index ] = node_c;
		c.key = node_c.freq;
		c.val = c_index;
		tdPushHeap( items, item_count++, c, nodes );
	}
	while ( item_count > 2 );

	//printf( "\n(node.freq, node.depth)\n" );
	//for ( int i = 0; i < sorted_count; ++i )
	//{
	//	tdNode node = nodes[ sorted[ i ] ];
	//	printf( "(%d, %d)\n", node.freq, node.depth );
	//}

	// crawling over the sorted list produces a level-order traversal
	// setting code lengths becomes trivial
	nodes[ sorted_count ].len = 0;
	int len_counts[ 16 ] = { 0 };
	int leaves[ 286 ];
	int leaf_count = 0;
	int overflow = 0;

	for ( int i = sorted_count - 1; i >= 0; --i )
	{
		int index = sorted[ i ];
		tdNode* child = nodes + index;
		int parent = child->parent;
		int len = nodes[ parent ].len + 1;
		child->len = len;

		if ( len > TD_DEFLATE_MAX_BITLEN )
		{
			len = TD_DEFLATE_MAX_BITLEN;
			++overflow;
		}

		if ( index >= leaf_index ) continue;
		TD_ASSERT( child->len < 16  && child->len >= 0 );
		len_counts[ child->len ]++;
		TD_ASSERT( leaf_count < 286 );
		bits_saved += nodes[ index ].freq;
		leaves[ leaf_count++ ] = index;
	}

	// overflow protection, untested
	if ( overflow )
	{
		do
		{
			int len = TD_DEFLATE_MAX_BITLEN - 1;
			while ( !len_counts[ len ] ) --len;
			len_counts[ len ]--; // move one leaf down the tree
			len_counts[ len + 1 ] += 2; // move one overflow item as its brother
			len_counts[ TD_DEFLATE_MAX_BITLEN ]--;
			overflow -= 2;
		}
		while ( overflow > 0 );

		int h = sorted_count;
		for ( int i = 0; i < TD_DEFLATE_MAX_BITLEN; ++i )
		{
			int j = len_counts[ i ];
			while ( j )
			{
				int k = sorted[ --h ];
				if ( k >= leaf_index ) continue;
				if ( nodes[ k ].len != i )
				{
					nodes[ k ].len = i;
				}
				--j;
			}
		}
	}

	//printf( "\n(node.freq, node.len)\n" );
	//for ( int i = 0; i < sorted_count; ++i )
	//{
	//	tdNode node = nodes[ sorted[ i ] ];
	//	printf( "(%d, %d)\n", node.freq, node.len );
	//}

	// freq and len are set
	tdItem root = items[ 1 ];
	//printf( "freq, len\n" );
	//tdPrint( nodes + root.val, nodes );
	
	// now we overwrite freq with code
	int codes[ 16 ];
#define USE_ZOPFLI 0
	ZopfliLengthLimitedCodeLengths( len_counts, 16, 15, codes );
	codes[ 0 ] = 0;
	
#if !USE_ZOPFLI
	// Distribute codes.
	for (int n=1;n<=15;n++) {
		codes[n] = (codes[n-1] + len_counts[n-1]) << 1;
	}
	for ( int i = 1; i < 16; ++i ) codes[ i ] = (codes[ i - 1 ] + len_counts[ i - 1 ]) << 1;
#endif
	for ( int i = 0; i < leaf_count; ++i )
	{
		int leaf = leaves[ i ];
		int len = nodes[ leaf ].len;
		TD_ASSERT( len > 0 && len < 16 );
		int original = nodes[ leaf ].original;
		TD_ASSERT( original != (uint16_t)~0 );
		TD_ASSERT( original >= 0 && original < count );
#if !USE_ZOPFLI
		if ( TD_DEBUG_CHECKS ) nodes[ leaf ].code = codes[ len ];
		tree[ original ].code = tdRev( codes[ len ]++, len );
		tree[ original ].len = len;
		TD_ASSERT( !(tree[ original ].code & ~((1 << tree[ original ].len ) - 1)) );
#else
		//tree[ original ].code = tdRev( codes[ len ]++, len );
		tree[ original ].code = codes[ len ]++;
		tree[ original ].len = len;
#endif
	}

	// now len and code are set
	printf( "code, len\n" );
	tdPrint( nodes + root.val, nodes );
}

TD_INLINE static int tdRun( tdLeaf* tree, int i, int N, int match )
{
	int start = i - 1;
	while ( i < N )
	{
		tdLeaf* symbol = tree + i;
		if ( symbol->len != match ) return i - start;
		++i;
	}
	return i - start;
}

static int tdRLE( tdLeaf* tree, int size, int* rle, int* rle_bits )
{
	int rle_count = 0;
	for ( int i = 0; i < size; )
	{
		int symbol = tree[ i ].len;
		TD_ASSERT( !(symbol & ~0xF) );
		int run = tdRun( tree, i + 1, size, symbol );

		if ( run >= 3 )
		{
			if ( symbol )
			{
				if ( run > 6 ) run = 6;
				rle_bits[ rle_count ] = run - 3;
				rle[ rle_count++ ] = 16;
			}

			else
			{
				if ( run > 10 )
				{
					if ( run > 138 ) run = 138;
					rle_bits[ rle_count ] = run - 11;
					rle[ rle_count++ ] = 18;
				}

				else
				{
					rle_bits[ rle_count ] = run - 3;
					rle[ rle_count++ ] = 17;
				}
			}
		}

		else
		{
			rle[ rle_count++ ] = symbol;
			run = 1;
		}

		i += run;
	}
	return rle_count;
}

#define ZOPFLI_APPEND_DATA(/* T */ value, /* T** */ data, /* int* */ size) {\
  if (!((*size) & ((*size) - 1))) {\
    /*double alloc size if it's a power of two*/\
    (*data) = (*size) == 0 ? malloc(sizeof(**data))\
                           : realloc((*data), (*size) * 2 * sizeof(**data));\
  }\
  (*data)[(*size)] = (value);\
  (*size)++;\
}

static int EncodeTree(tdDState* s, const unsigned* ll_lengths,
                         const unsigned* d_lengths,
                         int use_16, int use_17, int use_18) {
  unsigned lld_total;  /* Total amount of literal, length, distance codes. */
  /* Runlength encoded version of lengths of litlen and dist trees. */
  unsigned* rle = 0;
  unsigned* rle_bits = 0;  /* Extra bits for rle values 16, 17 and 18. */
  unsigned rle_size = 0;  /* Size of rle array. */
  int rle_bits_size = 0;  /* Should have same value as rle_size. */
  unsigned hlit = 29;  /* 286 - 257 */
  unsigned hdist = 29;  /* 32 - 1, but gzip does not like hdist > 29.*/
  unsigned hclen;
  unsigned hlit2;
  unsigned i, j;
  int clcounts[19];
  unsigned clcl[19];  /* Code length code lengths. */
  unsigned clsymbols[19];
  /* The order in which code length code lengths are encoded as per deflate. */
  static const unsigned order[19] = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
  };
  int size_only = 0;
  int result_size = 0;

  for(i = 0; i < 19; i++) clcounts[i] = 0;

  /* Trim zeros. */
  while (hlit > 0 && ll_lengths[257 + hlit - 1] == 0) hlit--;
  while (hdist > 0 && d_lengths[1 + hdist - 1] == 0) hdist--;
  hlit2 = hlit + 257;

  lld_total = hlit2 + hdist + 1;

  for (i = 0; i < lld_total; i++) {
    /* This is an encoding of a huffman tree, so now the length is a symbol */
    unsigned char symbol = i < hlit2 ? ll_lengths[i] : d_lengths[i - hlit2];
    unsigned count = 1;
    if(use_16 || (symbol == 0 && (use_17 || use_18))) {
      for (j = i + 1; j < lld_total && symbol ==
          (j < hlit2 ? ll_lengths[j] : d_lengths[j - hlit2]); j++) {
        count++;
      }
    }
    i += count - 1;

    /* Repetitions of zeroes */
    if (symbol == 0 && count >= 3) {
      if (use_18) {
        while (count >= 11) {
          unsigned count2 = count > 138 ? 138 : count;
          if (!size_only) {
            ZOPFLI_APPEND_DATA(18, &rle, &rle_size);
            ZOPFLI_APPEND_DATA(count2 - 11, &rle_bits, &rle_bits_size);
          }
          clcounts[18]++;
          count -= count2;
        }
      }
      if (use_17) {
        while (count >= 3) {
          unsigned count2 = count > 10 ? 10 : count;
          if (!size_only) {
            ZOPFLI_APPEND_DATA(17, &rle, &rle_size);
            ZOPFLI_APPEND_DATA(count2 - 3, &rle_bits, &rle_bits_size);
          }
          clcounts[17]++;
          count -= count2;
        }
      }
    }

    /* Repetitions of any symbol */
    if (use_16 && count >= 4) {
      count--;  /* Since the first one is hardcoded. */
      clcounts[symbol]++;
      if (!size_only) {
        ZOPFLI_APPEND_DATA(symbol, &rle, &rle_size);
        ZOPFLI_APPEND_DATA(0, &rle_bits, &rle_bits_size);
      }
      while (count >= 3) {
        unsigned count2 = count > 6 ? 6 : count;
        if (!size_only) {
          ZOPFLI_APPEND_DATA(16, &rle, &rle_size);
          ZOPFLI_APPEND_DATA(count2 - 3, &rle_bits, &rle_bits_size);
        }
        clcounts[16]++;
        count -= count2;
      }
    }

    /* No or insufficient repetition */
    clcounts[symbol] += count;
    while (count > 0) {
      if (!size_only) {
        ZOPFLI_APPEND_DATA(symbol, &rle, &rle_size);
        ZOPFLI_APPEND_DATA(0, &rle_bits, &rle_bits_size);
      }
      count--;
    }
  }

  ZopfliLengthLimitedCodeLengths(clcounts, 19, 7, clcl);
  if (!size_only) ZopfliLengthsToSymbols(clcl, 19, 7, clsymbols);

  hclen = 15;
  /* Trim zeros. */
  while (hclen > 0 && clcounts[order[hclen + 4 - 1]] == 0) hclen--;

  if (!size_only) {
    tdWriteBits(s, hlit, 5);
    tdWriteBits(s, hdist, 5);
    tdWriteBits(s, hclen, 4);

    for (i = 0; i < hclen + 4; i++) {
      tdWriteBits(s, clcl[order[i]], 3);
    }

    for (i = 0; i < rle_size; i++) {
      unsigned symbol = clsymbols[rle[i]];
      tdWriteBitsRev(s, symbol, clcl[rle[i]]);
      /* Extra bits. */
      if (rle[i] == 16) tdWriteBits(s, rle_bits[i], 2);
      else if (rle[i] == 17) tdWriteBits(s, rle_bits[i], 3);
      else if (rle[i] == 18) tdWriteBits(s, rle_bits[i], 7);
    }
  }

  result_size += 14;  /* hlit, hdist, hclen bits */
  result_size += (hclen + 4) * 3;  /* clcl bits */
  for(i = 0; i < 19; i++) {
    result_size += clcl[i] * clcounts[i];
  }
  /* Extra bits. */
  result_size += clcounts[16] * 2;
  result_size += clcounts[17] * 3;
  result_size += clcounts[18] * 7;

  /* Note: in case of "size_only" these are null pointers so no effect. */
  free(rle);
  free(rle_bits);

  return result_size;
}

static void AddDynamicTree(tdDState* s, const unsigned* ll_lengths, const unsigned* d_lengths) {
  EncodeTree(s, ll_lengths, d_lengths,
             1, 1, 1);
}

static int tdWriteTree( tdDState* s, tdLeaf* len, tdLeaf* dst, int size_only )
{
	int nlit = 286 - 257;
	int ndst = 32 - 1;
	int nlen = 15;

	int rle[ 286 + 32 ];
	int rle_bits[ 286 + 32 ];
	int rle_count = tdRLE( len, 286, rle, rle_bits );
	rle_count += tdRLE( dst, 32, rle + rle_count, rle_bits + rle_count );

	tdLeaf lenlen_tree[ 19 ] = { 0 };
	int lenlens[ 19 ] = { 0 };
	for ( int i = 0; i < rle_count; ++i ) lenlen_tree[ rle[ i ] ].freq++;
	tdMakeTree( lenlen_tree, 19 );
	for ( int i = 0; i < 19; ++i ) lenlens[ i ] = lenlen_tree[ i ].len;
	for ( int i = 0; i < 19; ++i ) TD_ASSERT( lenlens[ i ] >= 0 && lenlens[ i ] <= 7 );

	while ( nlit > 0 && !len[ 257 + nlit - 1 ].len ) nlit--;
	while ( ndst > 0 && !dst[ 1 + ndst - 1 ].len ) ndst--;
	while ( nlen > 0 && !lenlens[ g_tdPermutationOrder[ nlen + 4 - 1 ] ] ) nlen--;
	TD_ASSERT( nlit >= 0 && nlit < (286 - 257) );
	TD_ASSERT( ndst >= 0 && ndst < (32 - 1) );
	TD_ASSERT( nlen >= 0 && nlen < (19 - 4) );

	int bits_used = s->bits_left;
	if ( !size_only )
	{
		tdWriteBits( s, nlit, 5 );
		tdWriteBits( s, ndst, 5 );
		tdWriteBits( s, nlen, 4 );
		int done = nlen + 4;
		for ( int i = 0; i < done; ++i )
			tdWriteBits( s, lenlens[ g_tdPermutationOrder[ i ] ], 3 );
		for ( int i = 0; i < rle_count; ++i )
		{
			int symbol = rle[ i ];
			int code = lenlen_tree[ symbol ].code;
			int len = lenlen_tree[ symbol ].len;
			TD_ASSERT( !(code & ~((1 << len) - 1)) );
			tdWriteBits( s, code, len );
			switch ( symbol )
			{
			case 16: tdWriteBits( s, rle_bits[ i ], 2 ); break;
			case 17: tdWriteBits( s, rle_bits[ i ], 3 ); break;
			case 18: tdWriteBits( s, rle_bits[ i ], 7 ); break;
			}
		}
	}
	bits_used = bits_used - s->bits_left;

	int tree_size = 14;
	tree_size += (nlen + 4) * 3;
	for ( int i = 0; i < 19; i++ ) tree_size += lenlen_tree[ i ].len * lenlen_tree[ i ].freq;
	tree_size += lenlen_tree[ 16 ].freq * 2;
	tree_size += lenlen_tree[ 17 ].freq * 3;
	tree_size += lenlen_tree[ 18 ].freq * 7;
	TD_ASSERT( bits_used == tree_size );
	return tree_size;
}

// TODO: remove = 0, we already calloc
void* tdDeflateMem( const void* in, int bytes, int* out_bytes, tdDeflateOptions* options )
{
	TD_ASSERT( !((int)in & 3) );
	TD_ASSERT( !((int)bytes & 3) ); // TODO: get rid of this
	char* in_buffer = (char*)in;
	tdDState* s = (tdDState*)calloc( 1, sizeof( tdDState ) );
	s->in = (char*)in;
	s->in_end = s->in + bytes;
	s->out = (char*)malloc( bytes );
	s->out_end = s->out + bytes;
	s->window = s->in;

	s->bits = 0;
	s->count = 0;
	s->words = (uint32_t*)s->out;
	s->word_count = bytes / sizeof( uint32_t );
	s->word_index = 0;
	s->bits_left = s->word_count * sizeof( uint32_t ) * 8;
	int bits_left = s->bits_left;

	if ( options )
	{
		s->max_chain_len = options->max_chain_len;
		s->do_lazy_search = options->do_lazy_search;
	}

	else
	{
		// TODO think about these defaults
		// and actually use them
		s->max_chain_len = 1024;
		s->do_lazy_search = 1;
	}

	s->hash_rolling = 0;
	memset( s->buckets, 0, sizeof( s->buckets ) );
	s->entry_count = 0;

	int pair_count = 0;
	int run_count = 0;

	while ( s->window != s->in_end - 3 )
	{
		// move sliding window over one byte
		// make 3 byte hash
		char* start = s->window;
		char* end = s->window + 3;
		uint32_t h = djb2( start, end ) % TD_HASH_COUNT;
		tdHash* chain = s->buckets[ h ];

		int hash_index = s->hash_rolling++ % TD_HASH_COUNT;
		tdHash* hash = s->hashes + hash_index;
		hash->h = h;
		hash->start = start;
		hash->next = chain;
		s->buckets[ h ] = hash;

		if ( chain )
		{
			// loop over chain
			// keep track of best bit reduction
			tdHash* list = chain;
			tdHash* best_match = 0;
			int len = 0;
			int dst;
			int base_len;
			int base_dst;
			int len_bits;
			int dst_bits;
			int lowest_cost = INT_MAX;
			while ( list )
			{
				char* a = hash->start;
				char* b = list->start;
				while ( len <= 258 && *a == *b && a < s->in_end )
				{
					*a++ = *b++;
					++len;
				}
				if ( len )
				{
					TD_ASSERT( len <= 258 );
					dst = (int)(int)(s->window - list->start);
					tdMatchIndices( len, dst, &base_len, &base_dst );
					len_bits = g_tdLenExtraBits[ base_len ];
					dst_bits = g_tdDistExtraBits[ base_dst ];
					int cost = tdMatchCost( len, len_bits, dst_bits );
					if ( cost < lowest_cost )
					{
						lowest_cost = cost;
						best_match = list;
					}
				}
				list = list->next;
			}

			if ( !best_match )
				goto literal;

			if ( s->entry_count + 1 == TD_ENTRY_BUFFER_SIZE )
			{
				// if pair does not fit in block_buffer
				// dump tree
				__debugbreak( );
			}

			++pair_count;
			++run_count;
			s->len[ 257 + base_len ].freq++;
			s->dst[ base_dst ].freq++;
			s->window += len;

			tdEntry entry;
			entry.len = len;
			entry.dst = dst;
			entry.base_len = base_len;
			entry.base_dst = base_dst;
			entry.symbol_index = 257 + base_len;
			s->entries[ s->entry_count++ ] = entry;

			// num children in full binary tree, since we ideally pack in tons of
			// leaves with bit length less than 9, for fast decoder lookups.
			int two_to_ninth = 512;
			if ( run_count > two_to_ninth )
			{
				float entropy = tdEntropy( s->len );
				float golden = 1.61803398875f;
				float factor = golden * golden;
				if ( (int)(entropy * factor + 0.5f) > 15 )
				{
					tdMakeTree( s->len, 286 );
					tdMakeTree( s->dst, 30 );
					tdWriteTree( s, s->len, s->dst, 0 );
				}
			}

			continue;
		}

	literal:
		tdLiteral( s, *s->window );
	}

	if ( s->window != s->in_end )
	{
		TD_ASSERT( s->window + 3 == s->in_end );
		tdLiteral( s, *s->window );
		tdLiteral( s, *s->window );
		tdLiteral( s, *s->window );
	}
	tdLiteral( s, 256 );

	tdWriteBits( s, 1, 1 ); // final
	tdWriteBits( s, 2, 2 ); // dynamic

	tdMakeTree( s->len, 286 );
	tdMakeTree( s->dst, 30 );
	uint32_t llens[ 288 ];
	uint32_t dlens[ 32 ];
	for ( int i = 0; i < 288; ++i ) llens[ i ] = s->len[ i ].len;
	for ( int i = 0; i < 32; ++i ) dlens[ i ] = s->dst[ i ].len;
	AddDynamicTree( s, llens, dlens );

	printf( "the lz77 data\n" );
	for ( int i = 0; i < s->entry_count; ++i )
	{
		tdEntry* entry = s->entries + i;
		if ( entry->dst )
		{
			int base_len = entry->base_len;
			tdLeaf* len = s->len + entry->symbol_index;
			tdWriteBitsRev( s, len->code, len->len );
			tdWriteBits( s, entry->len - g_tdLenBase[ base_len ], g_tdLenExtraBits[ base_len ] );
			int base_dst = entry->base_dst;
			tdLeaf* dst = s->dst + base_dst;
			tdWriteBitsRev( s, dst->code, dst->len );
			tdWriteBits( s, entry->dst - g_tdDistBase[ base_dst ], g_tdDistExtraBits[ base_dst ] );
		}

		else
		{
			tdLeaf* symbol = s->len + entry->symbol_index;
			int code = symbol->code;
			int len = symbol->len;
			TD_ASSERT( !(code & ~((1 << len) - 1)) );
			tdWriteBitsRev( s, code, len );
		}
	}
	tdFlush( s );

	int bits_written = bits_left - s->bits_left;
	int bytes_written = (bits_written + 7) / 8;
	int out_size = bytes_written;
	if ( out_bytes ) *out_bytes = out_size;
	void* out = s->out;
	free( s );
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
