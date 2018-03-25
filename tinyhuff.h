/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	tinyhuff.h - v1.00

	To create implementation (the function definitions)
		#define TINYHUFF_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY:

		This header contains functions to compress and decompress data streams via
		Huffman encoding. Huffman encoding is generally good for smallish pieces of
		data (like 1KB), or at least data with very high repitition.

		It works by looking at input data as bytes. It counts which byte-values are
		the most common, and which are the most infrequent. Instead of using 8 bits
		per byte, frequent byte-values are remapped to short bit sequences (like 1,
		2 or 3 bits), while infrequent byte-values are given longer bit sequences.
		In this way Huffman encoding will usually have a good *average compression
		ratio*, but in rare cases can increase the size of input.

		Great use cases are:
		* Network packets
		* Messages, text or otherwise
		* More elaborate compression schemes
		* Serialized data

		Usage steps:

			1. Compute a shared key set
			2. Compress with compression_key
			3. Decompress with decompression_key

			1. thBuildKeys
			2. thCompress
			3. thDecompress

		More info:

			Generally the keyset is computed offline one time and serialized for later use.
			For example in a multiplayer game one client can use a compression_key on out-
			going packets, will the recipient can use the associated decompression_key to
			decompress incoming packets. The keys themselves are pre-computed, and never
			sent across the network.

			The keysets themselves can actually be stored in a fairly efficient manner by
			transmitting the code-lengths of the Huffman codes just before the encoded
			input stream. However, serializing Huffman trees in this way is: A) non-trivial
			to implement, and B) not that useful for practical purposes, since the tree
			itself takes up some space, eating away at compression gains. More sophisticated
			compression schemes can be used (like DEFLATE or even something fancier) if keys
			need to be serialized along with the data. Huffman encoding is very simple and
			quite efficient, making it a more practical choice for smaller but frequent
			pieces of redundant data.

	Revision history:
		1.0  (05/22/2017) initial release
*/

/*
	EXAMPLE USAGE:

	This string

		"Lorem ipsum dolor sit amet, consectetur adipiscing elit. Cras nec faucibus leo. Praesent
		risus tellus, dictum ut ipsum vitae, fringilla elementum justo. Sed placerat, mauris ac
		elementum rhoncus, dui ipsum tincidunt dolor, eu vehicula ipsum arcu vitae turpis. Vivamus
		pulvinar odio non orci sodales, at dictum ex faucibus. Donec ornare a dolor vel malesuada.
		Donec dapibus, mauris malesuada imperdiet hendrerit, nisl dui rhoncus nisi, ac gravida quam
		nulla at tellus. Praesent auctor odio vel maximus tempus. Sed luctus cursus varius. Morbi
		placerat ipsum quis velit gravida rhoncus. Nunc malesuada urna nisl, nec facilisis diam
		tincidunt at. Aliquam condimentum nulla ac urna feugiat tincidunt. Nullam semper ullamcorper
		scelerisque. Nunc condimentum consectetur magna, sed aliquam risus tempus vitae. Praesent
		ornare id massa a facilisis. Quisque mollis tristique dolor. Morbi ut velit quis augue
		placerat sollicitudin a eu massa."

	Gets compressed from 933 bytes, down to 3979 bits (or 497 bytes). Code to generate keys and do
	compression is below (a complete C program). Please note that generally the compression keys
	are computed *offline* and loaded upon startup, but the example shows how to generate them.
	There are no specific functions in this header to serialize compression keys; store them however
	you see fit. Compression keys can be used to store the compression keys themselves! This would
	be effective since the keys usually contain many zero bytes and some repitition.

		#define TINYHUFF_IMPLEMENTATION
		#include "../tinyhuff.h"

		#include <stdio.h>
		#include <stdlib.h>
		#include <string.h>

		int main( )
		{
			// allocate scratch memory for building shared keysets
			int scratch_bytes = TH_SCRATCH_MEMORY_BYTES;
			void* scratch_memory = malloc( scratch_bytes );
			thKey compress;
			thKey decompress;

			// the data to compress
			const char* string = INPUT_STRING;
			int bytes = strlen( string ) + 1;

			// construct compression and decompression shared keyset
			int ret = thBuildKeys( &compress, &decompress, string, bytes, scratch_memory );
			if ( !ret )
			{
				printf( "thBuildKeys failed: %s", th_error_reason );
				return -1;
			}

			// compute size of data after it would be compressed
			int compressed_bits = thCompressedSize( &compress, string, bytes );
			int compressed_bytes = (compressed_bits + 7) / 8;
			void* compressed_buffer = malloc( compressed_bytes );

			// do compression
			ret = thCompress( &compress, string, bytes, compressed_buffer, compressed_bytes );
			if ( !ret )
			{
				printf( "thCompress failed: %s", th_error_reason );
				return -1;
			}


			// do decompression
			char* buf = (char*)malloc( bytes );
			thDecompress( &decompress, compressed_buffer, compressed_bits, buf, bytes );

			if ( strcmp( buf, string ) )
			{
				printf( "String mismatch. Something bad happened" );
				return -1;
			}

			return 0;
		}

*/

#if !defined( TINYHUFF_H )

// Read in the event of any errors
extern const char* th_error_reason;

typedef struct
{
	unsigned count;
	unsigned char values[ 255 ];
	unsigned char lengths[ 255 ];
	unsigned codes[ 255 ];
} thKey;

// pre-computes a shared key set given arbitrary input data
// scratch_memory must be at least TH_SCRATCH_MEMORY_BYTES, sized for worst-case memory usage scenario
// returns 1 on success, 0 on failure
int thBuildKeys( thKey* compression_key, thKey* decompression_key, const void* in, int in_bytes, void* scratch_memory );
#define TH_SCRATCH_MEMORY_BYTES ((255 * 2 - 1) * (sizeof( int ) * 4 + sizeof( void* ) * 3))

// returns size in *bits*, NOT bytes
int thCompressedSize( const thKey* compression_key, const void* in, int in_bytes );
int thCompress( const thKey* compression_key, const void* in, int in_bytes, void* out, int out_bytes );

// please note in_bits is *bits*, NOT bytes
void thDecompress( const thKey* decompression_key, const void* in, int in_bits, void* out, int out_bytes );

// feel free to turn this on and try out the tests
#define TH_INTERNAL_TESTS 0

#if TH_INTERNAL_TESTS
	// runs internal tests for validate implementation correctness
	int thTestBuffer( );
#endif

#define TINYHUFF_H
#endif

#if defined( TINYHUFF_IMPLEMENTATION )
#undef TINYHUFF_IMPLEMENTATION

const char* th_error_reason;

// Feel free to turn this on to try it out
#define TH_DEBUG_OUT 0

#define TH_CHECK( X, Y ) do { if ( !(X) ) { th_error_reason = Y; return 0; } } while ( 0 )
#define TH_BITS_IN_INT (sizeof( int ) * 8)

#ifdef _MSC_VER
	#define TH_INLINE __forceinline
#else
	#define TH_INLINE inline __attribute__((always_inline))
#endif

typedef struct
{
	unsigned code;
	unsigned length;
	unsigned freq;
	unsigned value;
} thSym;

typedef struct thNode
{
	thSym sym;
	struct thNode* a;
	struct thNode* b;
} thNode;

static TH_INLINE int thFreqPred( thSym* a, thSym* b )
{
	return a->freq <= b->freq;
}

static TH_INLINE int thLexicalPred( thSym* a, thSym* b )
{
	return a->value <= b->value;
}

static TH_INLINE int thCanonicalPred( thSym* a, thSym* b )
{
	// first sort by length
	if ( a->length < b->length ) return 1;
	if ( a->length > b->length ) return 0;

	// tie breaker on lexical value
	return thLexicalPred( a, b );
}

static TH_INLINE int thCodePred( thSym* a, thSym* b )
{
	return a->code < b->code;
}

#define TH_DEFINE_INLINED_SORT( NAME, PRED ) \
static void NAME( thSym* items, int count ) \
{ \
	if ( count <= 1 ) return; \
	\
	thSym pivot = items[ count - 1 ]; \
	int low = 0; \
	for ( int i = 0; i < count - 1; ++i ) \
	{ \
		if ( PRED( items + i, &pivot ) ) \
		{ \
			thSym tmp = items[ i ]; \
			items[ i ] = items[ low ]; \
			items[ low ] = tmp; \
			low++; \
		} \
	} \
	\
	items[ count - 1 ] = items[ low ]; \
	items[ low ] = pivot; \
	NAME( items, low ); \
	NAME( items + low + 1, count - 1 - low ); \
}

TH_DEFINE_INLINED_SORT( thFreqSort, thFreqPred );
TH_DEFINE_INLINED_SORT( thCanonicalSort, thCanonicalPred );
TH_DEFINE_INLINED_SORT( thLexicalSort, thLexicalPred );
TH_DEFINE_INLINED_SORT( thCodeSort, thCodePred );

#if TH_DEBUG_OUT

	#include <stdio.h>

	void thPrintList( thNode** list, int count )
	{
		for ( int i = 0; i < count; ++i )
		{
			printf( "f : %d, v : %d\n", list[ i ]->sym.freq, list[ i ]->sym.value );
		}
		printf( "\n" );
	}

	char th_depth[ 32 + 1 ];
	int th_di;
	char th_code[ 32 + 1 ];
	int th_codei;

	void thPush( char c )
	{
		th_code[ th_codei++ ] = c == '|' ? '0' : '1';
		th_depth[ th_di++ ] = ' ';
		th_depth[ th_di++ ] = c;
		th_depth[ th_di++ ] = ' ';
		th_depth[ th_di++ ] = ' ';
		th_depth[ th_di ] = 0;
	}

	void thPop( )
	{
		th_depth[ th_di -= 4 ] = 0;
		th_code[ --th_codei ] = 0;
	}

	void thPrintTree_internal( thNode* tree )
	{
		printf( "(f : %d, v : %d, c : %s)\n", tree->sym.freq, tree->sym.value, *th_code ? th_code : "NULL" );

		if ( tree->a )
		{
			printf( "%s `-L", th_depth );
			thPush( '|' );
			thPrintTree_internal( tree->a );
			thPop( );
 
			printf( "%s `-R", th_depth );
			thPush( ' ' );
			thPrintTree_internal( tree->b );
			thPop( );
		}
	}

	void thPrintTree( thNode* tree )
	{
		thPrintTree_internal( tree );
		printf( "\n" );
	}

	void thDumpTree_internal( thNode* tree )
	{
		if ( tree->a )
		{
			thDumpTree_internal( tree->a );
			thDumpTree_internal( tree->b );
		}
		else printf( "f : %d, v : %d\n", tree->sym.freq, tree->sym.value );
	}

	void thDumpTree( thNode* tree )
	{
		thDumpTree_internal( tree );
		printf( "\n" );
	}

#else

	#define thPrintList( ... )
	#define thPrintTree( ... )
	#define thDumpTree( ... )

#endif

TH_INLINE static int thCodeLengths( thSym* symbols, int* max_index, thNode* tree, int length )
{
	if ( tree->a )
	{
		int a = thCodeLengths( symbols, max_index, tree->a, length + 1 );
		int b = thCodeLengths( symbols, max_index, tree->b, length + 1 );
		#define TH_MAX( x, y ) ((x) > (y) ? (x) : (y))
		return TH_MAX( b, TH_MAX( a, length ) );
	}

	else
	{
		int i = (*max_index)++;
		symbols[ i ].length = length;
		symbols[ i ].value = tree->sym.value;
		symbols[ i ].freq = tree->sym.freq;
		return length;
	}
}

TH_INLINE static void thLengthsToCodes( thSym* symbols, int count )
{
	int code = 0;
	for ( int i = 0; i < count - 1; ++i )
	{
		symbols[ i ].code = code;
		code = (code + 1) << (symbols[ i + 1 ].length - symbols[ i ].length);
	}
	symbols[ count - 1 ].code = code;
}

TH_INLINE static unsigned thRev16( unsigned a )
{
	a = ((a & 0xAAAA) >>  1) | ((a & 0x5555) << 1);
	a = ((a & 0xCCCC) >>  2) | ((a & 0x3333) << 2);
	a = ((a & 0xF0F0) >>  4) | ((a & 0x0F0F) << 4);
	a = ((a & 0xFF00) >>  8) | ((a & 0x00FF) << 8);
	return a;
}

TH_INLINE static unsigned thRev( unsigned a, unsigned len )
{
	return thRev16( a ) >> (16 - len);
}

int thCompressedSize( const thKey* compression_key, const void* in_buf, int in_bytes )
{
	unsigned char* in = (unsigned char*)in_buf;
	int counts[ 255 ] = { 0 };
	int sum = 0;
	for ( int i = 0; i < in_bytes; ++i ) counts[ in[ i ] ]++;
	for ( int i = 0, j = 0; i < 255; ++i )
		if ( counts[ i ] )
			sum += compression_key->lengths[ j++ ] * counts[ i ];
	return sum;
}

TH_INLINE static void thMakeKey( thKey* decompression_key, thSym* symbols, int symbol_count )
{
	for ( int i = 0; i < symbol_count; ++i )
	{
		decompression_key->codes[ i ] = symbols[ i ].code;
		decompression_key->lengths[ i ] = symbols[ i ].length;
		decompression_key->values[ i ] = symbols[ i ].value;
	}
	decompression_key->count = symbol_count;
}

TH_INLINE static void thMakeCompressionKey( thKey* compression_key, thSym* symbols, int symbol_count )
{
	thLexicalSort( symbols, symbol_count );
	thMakeKey( compression_key, symbols, symbol_count );
}

TH_INLINE static void thMakeDecompressionKey( thKey* decompression_key, thSym* symbols, int symbol_count )
{
	// flip bits for easy decoding
	for ( int i = 0; i < symbol_count; ++i )
	{
		thSym sym = symbols[ i ];
		sym.code = (sym.code << (TH_BITS_IN_INT - sym.length)) | i;
		symbols[ i ] = sym;
	}

	// sort first by code, then by index
	thCodeSort( symbols, symbol_count );
	thMakeKey( decompression_key, symbols, symbol_count );
}

int thBuildKeys( thKey* compression_key, thKey* decompression_key, const void* in_buf, int in_bytes, void* scratch_memory )
{
	unsigned char* in = (unsigned char*)in_buf;
	int counts[ 255 ] = { 0 };
	for ( int i = 0; i < in_bytes; ++i ) counts[ in[ i ] ]++;

	// count frequencies and form symbols
	thSym symbols[ 255 ];
	int symbol_count = 0;
	for ( int i = 0; i < 255; ++i ) 
	{
		if ( counts[ i ] ) 
		{
			thSym* sym = symbols + symbol_count++;
			sym->freq = counts[ i ];
			sym->value = i;
		}
	}
	thFreqSort( symbols, symbol_count );

	// create initial list of nodes sorted by frequency
	int node_count = 2 * symbol_count - 1;
	thNode* nodes = (thNode*)scratch_memory;
	thNode** list = (thNode**)(nodes + node_count);
	int list_count = symbol_count;

	for ( int i = 0; i < symbol_count; ++i )
	{
		thNode* node = nodes + i;
		node->sym = symbols[ i ];
		node->a = 0;
		node->b = 0;
		list[ i ] = node;
	}
	nodes += symbol_count;

	// construct huffman tree via:
	// https://www.siggraph.org/education/materials/HyperGraph/video/mpeg/mpegfaq/huffman_tutorial.html
	while ( list_count > 1 )
	{
		thPrintList( list, list_count );

		// pop two top nodes a and b, construct internal node c
		thNode* a = list[ 0 ];
		thNode* b = list[ 1 ];
		thNode* c = nodes++;
		c->a = a;
		c->b = b;
		c->sym.freq = a->sym.freq + b->sym.freq;
		c->sym.value = ~0;
		unsigned f = c->sym.freq;

		// place internal node back into list
		list += 2;
		int spot = list_count - 2 - 1;
		
		// preserve the sort
		if ( spot >= 0 )
		{
			for ( int i = 0; i < list_count - 2; ++i )
			{
				if ( f <= list[ i ]->sym.freq )
				{
					spot = i - 1;
					break;
				}
			}

			for ( int i = 0; i <= spot; ++i )
				list[ i - 1 ] = list[ i ];
		}

		// place c into list
		list[ spot ] = c;
		--list;
		--list_count;
	}

	thPrintList( list, list_count );
	thNode* root = *list;
	thPrintTree( root );
	thDumpTree( root );

	int tree_symbol_count = 0;
	int depth = thCodeLengths( symbols, &tree_symbol_count, root, 0 );

	// handle special case
	if ( symbol_count == 1 )
	{
		symbols->value = root->sym.value;
		symbols->freq = root->sym.freq;
		symbols->length = 1;
	}

	if ( depth >= 16 )
	{
		th_error_reason = "Bit-depth too large; input is too large to compress.";
		return 0;
	}

	if ( tree_symbol_count != symbol_count )
	{
		th_error_reason = "Symbol count mismatch; internal implementation error.";
		return 0;
	}

	// convert to canonical huffman tree format
	// https://en.wikipedia.org/wiki/Canonical_Huffman_code
	thCanonicalSort( symbols, symbol_count );
	thLengthsToCodes( symbols, symbol_count );

	// finally out the keys
	thMakeCompressionKey( compression_key, symbols, symbol_count );
	thMakeDecompressionKey( decompression_key, symbols, symbol_count );
	return 1;
}

typedef struct
{
	unsigned char* memory;
	unsigned bits_left;
	unsigned count;
	unsigned bits;
} thBuffer;

TH_INLINE static void thSetBuffer( thBuffer* b, const unsigned char* mem, unsigned bits )
{
	b->memory = (unsigned char*)mem;
	b->bits = 0;
	b->bits_left = bits;
	b->count = 0;
}

static int thPeakBits( thBuffer* b, unsigned bit_count )
{
	if ( bit_count > sizeof( int ) * 8 ) return 0;
	if ( b->bits_left - bit_count < 0 ) return 0;

	while ( b->count < bit_count )
	{
		b->bits |= (*b->memory++) << b->count;
		b->count += 8;
	}

	int bits = b->bits & ((1 << bit_count) - 1);
	return bits;
}

static int thGetBits( thBuffer* b, int bit_count )
{
	int bits = thPeakBits( b, bit_count );
	b->bits >>= bit_count;
	b->bits_left -= bit_count;
	b->count -= bit_count;
	return bits;
}

static void thPut8( thBuffer* b )
{
	*b->memory++ = (unsigned char)(b->bits & 0xFF);
	b->bits >>= 8;
	b->bits_left -= 8;
}

static int thPutBits( thBuffer* b, unsigned value, unsigned bit_count )
{
	if ( bit_count > sizeof( int ) * 8 ) return 0;
	if ( b->bits_left - bit_count < 0 ) return 0;

	while ( bit_count >= 8 )
	{
		b->bits |= (value & 0xFF) << b->count;
		value >>= 8;
		bit_count -= 8;
		thPut8( b );
	}

	b->bits |= (value & ((1 << bit_count) - 1)) << b->count;
	b->count += bit_count;
	if ( b->count >= 8 )
	{
		thPut8( b );
		b->count -= 8;
	}

	return 1;
}

TH_INLINE static int thPutBitsRev( thBuffer* b, unsigned value, unsigned bit_count )
{
	unsigned bits = thRev( value, bit_count );
	return thPutBits( b, bits, bit_count );
}

TH_INLINE static void thFlush( thBuffer* b )
{
	if ( !b->count ) return;
	*b->memory = (unsigned char)b->bits;
}

TH_INLINE static int thEncode( const unsigned char* values, int symbol_count, unsigned search )
{
	unsigned lo = 0;
	unsigned hi = symbol_count;
	while ( lo < hi )
	{
		unsigned guess = (lo + hi) / 2;
		if ( search < values[ guess ] ) hi = guess;
		else lo = guess + 1;
	}
	return lo - 1;
}

TH_INLINE static int thDecode( const unsigned* codes, int symbol_count, unsigned search )
{
	unsigned lo = 0;
	unsigned hi = symbol_count;
	while ( lo < hi )
	{
		int guess = (lo + hi) / 2;
		if ( search < codes[ guess ] ) hi = guess;
		else lo = guess + 1;
	}
	return lo - 1;
}

int thCompress( const thKey* compression_key, const void* in_buf, int in_bytes, void* out_buf, int out_bytes )
{
	unsigned char* in = (unsigned char*)in_buf;
	thBuffer b;
	thSetBuffer( &b, out_buf, out_bytes * 8 );
	for ( int i = 0; i < in_bytes; ++i )
	{
		unsigned val = *in++;
		int index = thEncode( compression_key->values, compression_key->count, val );
		unsigned code = compression_key->codes[ index ];
		unsigned length = compression_key->lengths[ index ];
		TH_CHECK( compression_key->values[ index ] == val, "Value mismatch; internal implementation error." );
		int bits_rev_ret = thPutBitsRev( &b, code, length );
		TH_CHECK( bits_rev_ret, "thPutBitsRev failed; internal implementation error." );
	}
	thFlush( &b );
	return 1;
}

void thDecompress( const thKey* decompression_key, const void* in_buf, int in_bits, void* out_buf, int out_bytes )
{
	unsigned char* out = (unsigned char*)out_buf;
	thBuffer b;
	thSetBuffer( &b, in_buf, in_bits );
	int bytes_written = 0;
	while ( in_bits && bytes_written < out_bytes )
	{
		unsigned bits = (thRev16( thPeakBits( &b, 16 ) ) << 16) | 0xFFFF;
		int index = thDecode( decompression_key->codes, decompression_key->count, bits );
		*out++ = decompression_key->values[ index ];
		unsigned length = decompression_key->lengths[ index ];
		thGetBits( &b, length );
		in_bits -= length;
		++bytes_written;
	}
}

#if TH_INTERNAL_TESTS

	#include <stdlib.h>

	int thTestBuffer( )
	{
		thBuffer buf;
		int bits = 100 * 8;
		unsigned char* mem = (unsigned char*)malloc( 100 );
		thBuffer* b = &buf;
		thSetBuffer( b, mem, bits );

		for ( int i = 0; i < 100; ++i )
			thPutBits( b, i & 1, 1 );
		thFlush( b );

		thSetBuffer( b, mem, bits );

		for ( int i = 0; i < 100; ++i )
			TH_CHECK( thGetBits( b, 1 ) == (i & 1), "Failed single-bit test." );

		thSetBuffer( b, mem, bits );

		for ( int i = 0; i < 20; ++i )
			thPutBits( b, (i & 1) ? 0xFF : 0, 2 );
		thFlush( b );

		thSetBuffer( b, mem, bits );

		for ( int i = 0; i < 20; ++i )
		{
			int a = thGetBits( b, 2 );
			int b = (i & 1) ? 3 : 0;
			TH_CHECK( a == b, "Failed two bits test." );
		}

		thSetBuffer( b, mem, bits );

		for ( int i = 0; i < 10; ++i )
			thPutBits( b, 17, 5 );
		thFlush( b );

		thSetBuffer( b, mem, bits );

		for ( int i = 0; i < 10; ++i )
		{
			int a = thGetBits( b, 5 );
			int b = 17;
			TH_CHECK( a == b, "Failed five bits test." );
		}

		thSetBuffer( b, mem, bits );

		for ( int i = 0; i < 10; ++i )
			thPutBits( b, (i & 1) ? 117 : 83, 7 );
		thFlush( b );

		thSetBuffer( b, mem, bits );

		for ( int i = 0; i < 10; ++i )
		{
			int a = thGetBits( b, 7 );
			int b = (i & 1) ? 117 : 83;
			TH_CHECK( a == b, "Failed seven bits test." );
		}

		return 1;
	}

#endif // TH_INTERNAL_TESTS

#endif // TINYHUFF_IMPLEMENTATION

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
