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

// Performs huffman compression on the input, stores compressed bits in a buffer allocated
// with TH_ALLOC. Returns NULL upon any errors. Read th_error_reason for an error string.
// Optionally stores output size in out_bytes (can be NULL). Upon errors will not write to
// out_bytes.
char* thCompressAuto( void* in, int in_bytes, int* out_bytes );

// Adjust as necessary. Only used by thCompressAuto
#define TH_ALLOC malloc

// Read in the event of any errors
extern const char* th_error_reason;

// The rest of the functions in this header are lower-level operations that do no memory
// allocation whatsoever. The typical usage looks like:

typedef struct thTree thTree;
int thBuildTree( thTree* tree, void* in, int in_bytes, void* scratch_memory );

// WORKING HERE
#define TH_SCRATCH_MEMORY_BYTES ((255 * 2 - 1) * (sizeof( int ) * 4 + sizeof( void* ) * 3))
int thCompressedSize( const thTree* tree );
int thWriteTree( thTree* tree, void* out, int out_bytes );
int thCompress( thTree* tree, void* in, int in_bytes, void* out, int out_bytes );

#define TINYHUFF_H
#endif

#if defined( TINYHUFF_IMPL )

// Feel free to turn this on to try it out
#define TH_DEBUG_OUT 1

#ifdef _MSC_VER
	#define TH_INLINE __forceinline
#else
	#define TH_INLINE inline __attribute__((always_inline))
#endif

typedef struct
{
	int freq;
	int value;

	int code;
	int length;
} thSym;

struct thTree
{
	int max_bits;
	int max_index;
	int codes[ 255 ];
	int values[ 255 ];
	int lengths[ 255 ];
	int frequencies[ 255 ];
};

typedef struct thNode
{
	thSym sym;
	struct thNode* a;
	struct thNode* b;
} thNode;

static TH_INLINE int thPred( thSym* a, thSym* b )
{
  return a->freq <= b->freq;
}

static void thQSort( thSym* items, int count )
{
	if ( count <= 1 ) return;

	thSym pivot = items[ count - 1 ];
	int low = 0;
	for ( int i = 0; i < count - 1; ++i )
	{
		if ( thPred( items + i, &pivot ) )
		{
			thSym tmp = items[ i ];
			items[ i ] = items[ low ];
			items[ low ] = tmp;
			low++;
		}
	}

	items[ count - 1 ] = items[ low ];
	items[ low ] = pivot;
	thQSort( items, low );
	thQSort( items + low + 1, count - 1 - low );
}

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

#else

	#define thPrintList( ... )
	#define thPrintTree( ... )

#endif

int thCodeLengths( thTree* out, thNode* tree, int code, int length )
{
	if ( tree->a )
	{
		int a = thCodeLengths( out, tree->a, code << 1, length + 1 );
		int b = thCodeLengths( out, tree->b, (code << 1) | 1, length + 1 );
		tree->sym.code = ~0;
		tree->sym.length = ~0;
		#define TH_MAX( x, y ) ((x) > (y) ? (x) : (y))
		return TH_MAX( b, TH_MAX( a, length ) );
	}

	else
	{
		tree->sym.code = code;
		tree->sym.length = length;
		int i = out->max_index++;
		out->codes[ i ] = code;
		out->lengths[ i ] = length;
		out->values[ i ] = tree->sym.value;
		out->frequencies[ i ] = tree->sym.freq;
		return length;
	}
}

int thBuildTree( thTree* tree, void* in_buf, int in_bytes, void* scratch_memory )
{
	char* in = (char*)in_buf;
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
	thQSort( symbols, symbol_count );

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
		int f = c->sym.freq;

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

	tree->max_index = 0;
	int depth = thCodeLengths( tree, root, 0, 0 );
	tree->max_bits = depth;

	if ( tree->max_index != symbol_count )
	{
		// todo
	}

	return 0;
}

int thCompressedSize( const thTree* tree )
{
	// loop over each code
	// multiply length + freq
	// sum that and return
}

#endif // TINYHUFF_IMPL
