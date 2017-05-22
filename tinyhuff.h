/*
	tinyhuff.h - v0.00

	SUMMARY:

	Revision history:
		1.0  (05/19/2017) initial release
*/

/*
	To create implementation (the function definitions)
		#define TINYHUFF_IMPL
	in *one* C/CPP file (translation unit) that includes this file
*/

#if !defined( TINYHUFF_H )

// Read in the event of any errors
extern const char* th_error_reason;

typedef struct thTree thTree;
int thBuildTree( thTree* tree, const void* in, int in_bytes, void* scratch_memory );
#define TH_SCRATCH_MEMORY_BYTES ((255 * 2 - 1) * (sizeof( int ) * 4 + sizeof( void* ) * 3))

int thCompressedSize( const thTree* tree );
int thCompress( thTree* tree, const void* in, int in_bytes, void* out, int out_bytes );
int thDecompress( thTree* tree, const void* in, int in_bits, void* out, int out_bytes );

int thTestBuffer( );

#define TINYHUFF_H
#endif

#if defined( TINYHUFF_IMPL )

const char* th_error_reason;

// Feel free to turn this on to try it out
#define TH_DEBUG_OUT 1

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

struct thTree
{
	int max_bits;
	int symbol_count;
	thSym symbols[ 255 ];
};

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

int thCodeLengths( thSym* symbols, int* max_index, thNode* tree, int length )
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

void thLengthsToCodes( thSym* symbols, int count )
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

int thBuildTree( thTree* tree, const void* in_buf, int in_bytes, void* scratch_memory )
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

	tree->symbol_count = 0;
	int depth = thCodeLengths( symbols, &tree->symbol_count, root, 0 );
	tree->max_bits = depth;

	if ( depth >= 16 )
	{
		// todo
	}

	if ( tree->symbol_count != symbol_count )
	{
		// todo
	}

	// convert to canonical huffman tree format
	// https://en.wikipedia.org/wiki/Canonical_Huffman_code
	thCanonicalSort( symbols, symbol_count );
	thLengthsToCodes( symbols, symbol_count );

	// convert to lexical ordering to easy encoding
	thLexicalSort( symbols, symbol_count );

	for ( int i = 0; i < symbol_count; ++i )
		tree->symbols[ i ] = symbols[ i ];

	return 0;
}

int thCompressedSize( const thTree* tree )
{
	int sum = 0;
	for ( int i = 0; i < tree->symbol_count; ++i )
		sum += tree->symbols[ i ].length * tree->symbols[ i ].freq;
	return sum;
}

typedef struct
{
	unsigned char* memory;
	unsigned bits_left;
	unsigned count;
	unsigned bits;
} thBuffer;

void thSetBuffer( thBuffer* b, const unsigned char* mem, unsigned bits )
{
	b->memory = (unsigned char*)mem;
	b->bits = 0;
	b->bits_left = bits;
	b->count = 0;
}

int thPeakBits( thBuffer* b, unsigned bit_count )
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

int thGetBits( thBuffer* b, int bit_count )
{
	int bits = thPeakBits( b, bit_count );
	b->bits >>= bit_count;
	b->bits_left -= bit_count;
	b->count -= bit_count;
	return bits;
}

void thPut8( thBuffer* b )
{
	*b->memory++ = (unsigned char)(b->bits & 0xFF);
	b->bits >>= 8;
	b->bits_left -= 8;
}

int thPutBits( thBuffer* b, unsigned value, unsigned bit_count )
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
	return thPutBits( b, thRev( value, bit_count ), bit_count );
}

void thFlush( thBuffer* b )
{
	*b->memory = (unsigned char)b->bits;
}

thSym* thEncode( thSym* symbols, int symbol_count, unsigned search )
{
	unsigned lo = 0;
	unsigned hi = symbol_count;
	while ( lo < hi )
	{
		unsigned guess = (lo + hi) / 2;
		if ( search < symbols[ guess ].value ) hi = guess;
		else lo = guess + 1;
	}
	return symbols + lo - 1;
}

thSym* thDecode( thSym* symbols, int symbol_count, unsigned search )
{
	unsigned lo = 0;
	unsigned hi = symbol_count;
	while ( lo < hi )
	{
		int guess = (lo + hi) / 2;
		if ( search < symbols[ guess ].code ) hi = guess;
		else lo = guess + 1;
	}
	return symbols + lo - 1;
}

#define TH_BITS_IN_INT (sizeof( int ) * 8)

// code, length, value
int thCompress( thTree* tree, const void* in_buf, int in_bytes, void* out_buf, int out_bytes )
{
	unsigned char* in = (unsigned char*)in_buf;
	thBuffer b;
	thSetBuffer( &b, out_buf, out_bytes * 8 );
	for ( int i = 0; i < in_bytes; ++i )
	{
		unsigned val = *in++;
		thSym* symbol = thEncode( tree->symbols, tree->symbol_count, val );
		unsigned code = symbol->code;
		int length = symbol->length;
		if ( symbol->value != val ) return 0;
		if ( !thPutBits( &b, code, length ) ) return 0;
	}
	return 1;
}

// code with flipped bits, length, value
int thDecompress( thTree* tree, const void* in_buf, int in_bits, void* out_buf, int out_bytes )
{
	unsigned char* out = (unsigned char*)out_buf;
	thBuffer b;
	thSetBuffer( &b, in_buf, in_bits );

	// flip bits for easy decoding
	for ( int i = 0; i < tree->symbol_count; ++i )
	{
		thSym sym = tree->symbols[ i ];
		unsigned mask = ((1 << (TH_BITS_IN_INT - sym.length)) - 1) - 0xFF;
		unsigned rev = thRev( sym.code, sym.length );
		unsigned shifted = rev << (TH_BITS_IN_INT - sym.length);
		unsigned code_flipped = shifted | (i << 4) | sym.length;
		sym.code = code_flipped;
		tree->symbols[ i ] = sym;
	}

	thCodeSort( tree->symbols, tree->symbol_count );
	while ( in_bits )
	{
		unsigned bits = thRev16( thPeakBits( &b, 16 ) ) << 16 | 0xFFFF;
		thSym* symbol = thDecode( tree->symbols, tree->symbol_count, bits );
		int length = symbol->length;
		int code = thRev( bits >> (TH_BITS_IN_INT - length), length );
		int x;
		x = 10;
	}
	return 0;
}

#include <stdlib.h>

#define TH_CHECK( X, Y ) do { if ( !(X) ) { th_error_reason = Y; return 0; } } while ( 0 )

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

#endif // TINYHUFF_IMPL
