/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_huff.h - v1.00

	To create implementation (the function definitions)
		#define CUTE_HUFF_IMPLEMENTATION
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

			1. huff_build_keys
			2. huff_compress
			3. huff_decompress

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

		#define CUTE_HUFF_IMPLEMENTATION
		#include "../cute_huff.h"

		#include <stdio.h>
		#include <stdlib.h>
		#include <string.h>

		int main()
		{
			// allocate scratch memory for building shared keysets
			int scratch_bytes = CUTE_HUFF_SCRATCH_MEMORY_BYTES;
			void* scratch_memory = malloc(scratch_bytes);
			huff_key_t compress;
			huff_key_t decompress;

			// the data to compress
			const char* string = INPUT_STRING;
			int bytes = strlen(string) + 1;

			// construct compression and decompression shared keyset
			int ret = huff_build_keys(&compress, &decompress, string, bytes, scratch_memory);
			if (!ret)
			{
				printf("huff_build_keys failed: %s", huff_error_reason);
				return -1;
			}

			// compute size of data after it would be compressed
			int compressed_bits = huff_compressed_size(&compress, string, bytes);
			int compressed_bytes = (compressed_bits + 7) / 8;
			void* compressed_buffer = malloc(compressed_bytes);

			// do compression
			ret = huff_compress(&compress, string, bytes, compressed_buffer, compressed_bytes);
			if (!ret)
			{
				printf("huff_compress failed: %s", huff_error_reason);
				return -1;
			}


			// do decompression
			char* buf = (char*)malloc(bytes);
			huff_decompress(&decompress, compressed_buffer, compressed_bits, buf, bytes);

			if (strcmp(buf, string))
			{
				printf("String mismatch. Something bad happened");
				return -1;
			}

			return 0;
		}

*/

#if !defined(CUTE_HUFF_H)

// Read in the event of any errors
extern const char* huff_error_reason;

typedef struct huff_key_t
{
	unsigned count;
	unsigned char values[255];
	unsigned char lengths[255];
	unsigned codes[255];
} huff_key_t;

// pre-computes a shared key set given arbitrary input data
// scratch_memory must be at least CUTE_HUFF_SCRATCH_MEMORY_BYTES, sized for worst-case memory usage scenario
// returns 1 on success, 0 on failure
int huff_build_keys(huff_key_t* compression_key, huff_key_t* decompression_key, const void* in, int in_bytes, void* scratch_memory);
#define CUTE_HUFF_SCRATCH_MEMORY_BYTES ((255 * 2 - 1) * (sizeof(int) * 4 + sizeof(void*) * 3))

// returns size in *bits*, NOT bytes
int huff_compressed_size(const huff_key_t* compression_key, const void* in, int in_bytes);
int huff_compress(const huff_key_t* compression_key, const void* in, int in_bytes, void* out, int out_bytes);

// please note in_bits is *bits*, NOT bytes
void huff_decompress(const huff_key_t* decompression_key, const void* in, int in_bits, void* out, int out_bytes);

// feel free to turn this on and try out the tests
#define CUTE_HUFF_INTERNAL_TESTS 0

#if CUTE_HUFF_INTERNAL_TESTS
	// runs internal tests for validate implementation correctness
	int huff_test_buffer();
#endif

#define CUTE_HUFF_H
#endif

#ifdef CUTE_HUFF_IMPLEMENTATION
#ifndef CUTE_HUFF_IMPLEMENTATION_ONCE
#define CUTE_HUFF_IMPLEMENTATION_ONCE

const char* huff_error_reason;

// Feel free to turn this on to try it out
#define CUTE_HUFF_DEBUG_OUT 0

#define CUTE_HUFF_CHECK(X, Y) do { if (!(X)) { huff_error_reason = Y; return 0; } } while (0)
#define CUTE_HUFF_BITS_IN_INT (sizeof(int) * 8)

#ifdef _MSC_VER
	#define CUTE_HUFF_INLINE __forceinline
#else
	#define CUTE_HUFF_INLINE inline __attribute__((always_inline))
#endif

typedef struct huff_symbol_t
{
	unsigned code;
	unsigned length;
	unsigned freq;
	unsigned value;
} huff_symbol_t;

typedef struct huff_node_t
{
	huff_symbol_t sym;
	struct huff_node_t* a;
	struct huff_node_t* b;
} huff_node_t;

static CUTE_HUFF_INLINE int huff_free_pred_internal(huff_symbol_t* a, huff_symbol_t* b)
{
	return a->freq <= b->freq;
}

static CUTE_HUFF_INLINE int huff_lexical_pred_internal(huff_symbol_t* a, huff_symbol_t* b)
{
	return a->value <= b->value;
}

static CUTE_HUFF_INLINE int huff_canonical_pred_internal(huff_symbol_t* a, huff_symbol_t* b)
{
	// first sort by length
	if (a->length < b->length) return 1;
	if (a->length > b->length) return 0;

	// tie breaker on lexical value
	return huff_lexical_pred_internal(a, b);
}

static CUTE_HUFF_INLINE int huff_code_pred_internal(huff_symbol_t* a, huff_symbol_t* b)
{
	return a->code < b->code;
}

#define CUTE_HUFF_DEFINE_INLINED_SORT(NAME, PRED) \
static void NAME(huff_symbol_t* items, int count) \
{ \
	if (count <= 1) return; \
	\
	huff_symbol_t pivot = items[count - 1]; \
	int low = 0; \
	for (int i = 0; i < count - 1; ++i) \
	{ \
		if (PRED(items + i, &pivot)) \
		{ \
			huff_symbol_t tmp = items[i]; \
			items[i] = items[low]; \
			items[low] = tmp; \
			low++; \
		} \
	} \
	\
	items[count - 1] = items[low]; \
	items[low] = pivot; \
	NAME(items, low); \
	NAME(items + low + 1, count - 1 - low); \
}

CUTE_HUFF_DEFINE_INLINED_SORT(huff_freq_sort_internal, huff_free_pred_internal);
CUTE_HUFF_DEFINE_INLINED_SORT(huff_canonical_sort_internal, huff_canonical_pred_internal);
CUTE_HUFF_DEFINE_INLINED_SORT(huff_lexical_sort_internal, huff_lexical_pred_internal);
CUTE_HUFF_DEFINE_INLINED_SORT(huff_code_sort_internal, huff_code_pred_internal);

#if CUTE_HUFF_DEBUG_OUT

	#include <stdio.h>

	void huff_print_list_internal(huff_node_t** list, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			printf("f : %d, v : %d\n", list[i]->sym.freq, list[i]->sym.value);
		}
		printf("\n");
	}

	char huff_depth_internal[32 + 1];
	int huff_di_internal;
	char huff_code_internal[32 + 1];
	int huff_code_internali;

	void thPush(char c)
	{
		huff_code_internal[huff_code_internali++] = c == '|' ? '0' : '1';
		huff_depth_internal[huff_di_internal++] = ' ';
		huff_depth_internal[huff_di_internal++] = c;
		huff_depth_internal[huff_di_internal++] = ' ';
		huff_depth_internal[huff_di_internal++] = ' ';
		huff_depth_internal[huff_di_internal] = 0;
	}

	void huff_pop_internal()
	{
		huff_depth_internal[huff_di_internal -= 4] = 0;
		huff_code_internal[--huff_code_internali] = 0;
	}

	void huff_print_tree_internal(huff_node_t* tree)
	{
		printf("(f : %d, v : %d, c : %s)\n", tree->sym.freq, tree->sym.value, *huff_code_internal ? huff_code_internal : "NULL");

		if (tree->a)
		{
			printf("%s `-L", huff_depth_internal);
			thPush('|');
			huff_print_tree_internal(tree->a);
			huff_pop_internal();
 
			printf("%s `-R", huff_depth_internal);
			thPush(' ');
			huff_print_tree_internal(tree->b);
			huff_pop_internal();
		}
	}

	void huff_print_tree(huff_node_t* tree)
	{
		huff_print_tree_internal(tree);
		printf("\n");
	}

	void huff_demp_tree_internal(huff_node_t* tree)
	{
		if (tree->a)
		{
			huff_demp_tree_internal(tree->a);
			huff_demp_tree_internal(tree->b);
		}
		else printf("f : %d, v : %d\n", tree->sym.freq, tree->sym.value);
	}

	void huff_dump_tree(huff_node_t* tree)
	{
		huff_demp_tree_internal(tree);
		printf("\n");
	}

#else

	#define huff_print_list_internal(...)
	#define huff_print_tree(...)
	#define huff_dump_tree(...)

#endif

CUTE_HUFF_INLINE static int huff_code_lengths(huff_symbol_t* symbols, int* max_index, huff_node_t* tree, int length)
{
	if (tree->a)
	{
		int a = huff_code_lengths(symbols, max_index, tree->a, length + 1);
		int b = huff_code_lengths(symbols, max_index, tree->b, length + 1);
		#define CUTE_HUFF_MAX(x, y) ((x) > (y) ? (x) : (y))
		return CUTE_HUFF_MAX(b, CUTE_HUFF_MAX(a, length));
	}

	else
	{
		int i = (*max_index)++;
		symbols[i].length = length;
		symbols[i].value = tree->sym.value;
		symbols[i].freq = tree->sym.freq;
		return length;
	}
}

CUTE_HUFF_INLINE static void huff_lengths_to_codes(huff_symbol_t* symbols, int count)
{
	int code = 0;
	for (int i = 0; i < count - 1; ++i)
	{
		symbols[i].code = code;
		code = (code + 1) << (symbols[i + 1].length - symbols[i].length);
	}
	symbols[count - 1].code = code;
}

CUTE_HUFF_INLINE static unsigned huff_rev16(unsigned a)
{
	a = ((a & 0xAAAA) >>  1) | ((a & 0x5555) << 1);
	a = ((a & 0xCCCC) >>  2) | ((a & 0x3333) << 2);
	a = ((a & 0xF0F0) >>  4) | ((a & 0x0F0F) << 4);
	a = ((a & 0xFF00) >>  8) | ((a & 0x00FF) << 8);
	return a;
}

CUTE_HUFF_INLINE static unsigned huff_rev(unsigned a, unsigned len)
{
	return huff_rev16(a) >> (16 - len);
}

int huff_compressed_size(const huff_key_t* compression_key, const void* in_buf, int in_bytes)
{
	unsigned char* in = (unsigned char*)in_buf;
	int counts[255] = { 0 };
	int sum = 0;
	for (int i = 0; i < in_bytes; ++i) counts[in[i]]++;
	for (int i = 0, j = 0; i < 255; ++i)
		if (counts[i])
			sum += compression_key->lengths[j++] * counts[i];
	return sum;
}

CUTE_HUFF_INLINE static void huff_make_key(huff_key_t* decompression_key, huff_symbol_t* symbols, int symbol_count)
{
	for (int i = 0; i < symbol_count; ++i)
	{
		decompression_key->codes[i] = symbols[i].code;
		decompression_key->lengths[i] = symbols[i].length;
		decompression_key->values[i] = symbols[i].value;
	}
	decompression_key->count = symbol_count;
}

CUTE_HUFF_INLINE static void huff_make_compression_key(huff_key_t* compression_key, huff_symbol_t* symbols, int symbol_count)
{
	huff_lexical_sort_internal(symbols, symbol_count);
	huff_make_key(compression_key, symbols, symbol_count);
}

CUTE_HUFF_INLINE static void huff_make_decompression_key(huff_key_t* decompression_key, huff_symbol_t* symbols, int symbol_count)
{
	// flip bits for easy decoding
	for (int i = 0; i < symbol_count; ++i)
	{
		huff_symbol_t sym = symbols[i];
		sym.code = (sym.code << (CUTE_HUFF_BITS_IN_INT - sym.length)) | i;
		symbols[i] = sym;
	}

	// sort first by code, then by index
	huff_code_sort_internal(symbols, symbol_count);
	huff_make_key(decompression_key, symbols, symbol_count);
}

int huff_build_keys(huff_key_t* compression_key, huff_key_t* decompression_key, const void* in_buf, int in_bytes, void* scratch_memory)
{
	unsigned char* in = (unsigned char*)in_buf;
	int counts[255] = { 0 };
	for (int i = 0; i < in_bytes; ++i) counts[in[i]]++;

	// count frequencies and form symbols
	huff_symbol_t symbols[255];
	int symbol_count = 0;
	for (int i = 0; i < 255; ++i) 
	{
		if (counts[i]) 
		{
			huff_symbol_t* sym = symbols + symbol_count++;
			sym->freq = counts[i];
			sym->value = i;
		}
	}
	huff_freq_sort_internal(symbols, symbol_count);

	// create initial list of nodes sorted by frequency
	int node_count = 2 * symbol_count - 1;
	huff_node_t* nodes = (huff_node_t*)scratch_memory;
	huff_node_t** list = (huff_node_t**)(nodes + node_count);
	int list_count = symbol_count;

	for (int i = 0; i < symbol_count; ++i)
	{
		huff_node_t* node = nodes + i;
		node->sym = symbols[i];
		node->a = 0;
		node->b = 0;
		list[i] = node;
	}
	nodes += symbol_count;

	// construct huffman tree via:
	// https://www.siggraph.org/education/materials/HyperGraph/video/mpeg/mpegfaq/huffman_tutorial.html
	while (list_count > 1)
	{
		huff_print_list_internal(list, list_count);

		// pop two top nodes a and b, construct internal node c
		huff_node_t* a = list[0];
		huff_node_t* b = list[1];
		huff_node_t* c = nodes++;
		c->a = a;
		c->b = b;
		c->sym.freq = a->sym.freq + b->sym.freq;
		c->sym.value = ~0;
		unsigned f = c->sym.freq;

		// place internal node back into list
		list += 2;
		int spot = list_count - 2 - 1;
		
		// preserve the sort
		if (spot >= 0)
		{
			for (int i = 0; i < list_count - 2; ++i)
			{
				if (f <= list[i]->sym.freq)
				{
					spot = i - 1;
					break;
				}
			}

			for (int i = 0; i <= spot; ++i)
				list[i - 1] = list[i];
		}

		// place c into list
		list[spot] = c;
		--list;
		--list_count;
	}

	huff_print_list_internal(list, list_count);
	huff_node_t* root = *list;
	huff_print_tree(root);
	huff_dump_tree(root);

	int tree_symbol_count = 0;
	int depth = huff_code_lengths(symbols, &tree_symbol_count, root, 0);

	// handle special case
	if (symbol_count == 1)
	{
		symbols->value = root->sym.value;
		symbols->freq = root->sym.freq;
		symbols->length = 1;
	}

	if (depth >= 16)
	{
		huff_error_reason = "Bit-depth too large; input is too large to compress.";
		return 0;
	}

	if (tree_symbol_count != symbol_count)
	{
		huff_error_reason = "Symbol count mismatch; internal implementation error.";
		return 0;
	}

	// convert to canonical huffman tree format
	// https://en.wikipedia.org/wiki/Canonical_Huffman_code
	huff_canonical_sort_internal(symbols, symbol_count);
	huff_lengths_to_codes(symbols, symbol_count);

	// finally out the keys
	huff_make_compression_key(compression_key, symbols, symbol_count);
	huff_make_decompression_key(decompression_key, symbols, symbol_count);
	return 1;
}

typedef struct huff_buffer_t
{
	unsigned char* memory;
	unsigned bits_left;
	unsigned count;
	unsigned bits;
} huff_buffer_t;

CUTE_HUFF_INLINE static void huff_set_buffer(huff_buffer_t* b, const unsigned char* mem, unsigned bits)
{
	b->memory = (unsigned char*)mem;
	b->bits = 0;
	b->bits_left = bits;
	b->count = 0;
}

static int huff_peak_bits(huff_buffer_t* b, unsigned bit_count)
{
	if (bit_count > sizeof(int) * 8) return 0;
	if (b->bits_left - bit_count < 0) return 0;

	while (b->count < bit_count)
	{
		b->bits |= (*b->memory++) << b->count;
		b->count += 8;
	}

	int bits = b->bits & ((1 << bit_count) - 1);
	return bits;
}

static int huff_get_bits(huff_buffer_t* b, int bit_count)
{
	int bits = huff_peak_bits(b, bit_count);
	b->bits >>= bit_count;
	b->bits_left -= bit_count;
	b->count -= bit_count;
	return bits;
}

static void huff_put8(huff_buffer_t* b)
{
	*b->memory++ = (unsigned char)(b->bits & 0xFF);
	b->bits >>= 8;
	b->bits_left -= 8;
}

static int huff_put_bits(huff_buffer_t* b, unsigned value, unsigned bit_count)
{
	if (bit_count > sizeof(int) * 8) return 0;
	if (b->bits_left - bit_count < 0) return 0;

	while (bit_count >= 8)
	{
		b->bits |= (value & 0xFF) << b->count;
		value >>= 8;
		bit_count -= 8;
		huff_put8(b);
	}

	b->bits |= (value & ((1 << bit_count) - 1)) << b->count;
	b->count += bit_count;
	if (b->count >= 8)
	{
		huff_put8(b);
		b->count -= 8;
	}

	return 1;
}

CUTE_HUFF_INLINE static int huff_put_bits_rev(huff_buffer_t* b, unsigned value, unsigned bit_count)
{
	unsigned bits = huff_rev(value, bit_count);
	return huff_put_bits(b, bits, bit_count);
}

CUTE_HUFF_INLINE static void huff_flush(huff_buffer_t* b)
{
	if (!b->count) return;
	*b->memory = (unsigned char)b->bits;
}

CUTE_HUFF_INLINE static int huff_encode(const unsigned char* values, int symbol_count, unsigned search)
{
	unsigned lo = 0;
	unsigned hi = symbol_count;
	while (lo < hi)
	{
		unsigned guess = (lo + hi) / 2;
		if (search < values[guess]) hi = guess;
		else lo = guess + 1;
	}
	return lo - 1;
}

CUTE_HUFF_INLINE static int huff_dencode(const unsigned* codes, int symbol_count, unsigned search)
{
	unsigned lo = 0;
	unsigned hi = symbol_count;
	while (lo < hi)
	{
		int guess = (lo + hi) / 2;
		if (search < codes[guess]) hi = guess;
		else lo = guess + 1;
	}
	return lo - 1;
}

int huff_compress(const huff_key_t* compression_key, const void* in_buf, int in_bytes, void* out_buf, int out_bytes)
{
	unsigned char* in = (unsigned char*)in_buf;
	huff_buffer_t b;
	huff_set_buffer(&b, out_buf, out_bytes * 8);
	for (int i = 0; i < in_bytes; ++i)
	{
		unsigned val = *in++;
		int index = huff_encode(compression_key->values, compression_key->count, val);
		unsigned code = compression_key->codes[index];
		unsigned length = compression_key->lengths[index];
		CUTE_HUFF_CHECK(compression_key->values[index] == val, "Value mismatch; internal implementation error.");
		int bits_rev_ret = huff_put_bits_rev(&b, code, length);
		CUTE_HUFF_CHECK(bits_rev_ret, "huff_put_bits_rev failed; internal implementation error.");
	}
	huff_flush(&b);
	return 1;
}

void huff_decompress(const huff_key_t* decompression_key, const void* in_buf, int in_bits, void* out_buf, int out_bytes)
{
	unsigned char* out = (unsigned char*)out_buf;
	huff_buffer_t b;
	huff_set_buffer(&b, in_buf, in_bits);
	int bytes_written = 0;
	while (in_bits && bytes_written < out_bytes)
	{
		unsigned bits = (huff_rev16(huff_peak_bits(&b, 16)) << 16) | 0xFFFF;
		int index = huff_dencode(decompression_key->codes, decompression_key->count, bits);
		*out++ = decompression_key->values[index];
		unsigned length = decompression_key->lengths[index];
		huff_get_bits(&b, length);
		in_bits -= length;
		++bytes_written;
	}
}

#if CUTE_HUFF_INTERNAL_TESTS

	#include <stdlib.h>

	int huff_test_buffer()
	{
		huff_buffer_t buf;
		int bits = 100 * 8;
		unsigned char* mem = (unsigned char*)malloc(100);
		huff_buffer_t* b = &buf;
		huff_set_buffer(b, mem, bits);

		for (int i = 0; i < 100; ++i)
			huff_put_bits(b, i & 1, 1);
		huff_flush(b);

		huff_set_buffer(b, mem, bits);

		for (int i = 0; i < 100; ++i)
			CUTE_HUFF_CHECK(huff_get_bits(b, 1) == (i & 1), "Failed single-bit test.");

		huff_set_buffer(b, mem, bits);

		for (int i = 0; i < 20; ++i)
			huff_put_bits(b, (i & 1) ? 0xFF : 0, 2);
		huff_flush(b);

		huff_set_buffer(b, mem, bits);

		for (int i = 0; i < 20; ++i)
		{
			int a = huff_get_bits(b, 2);
			int b = (i & 1) ? 3 : 0;
			CUTE_HUFF_CHECK(a == b, "Failed two bits test.");
		}

		huff_set_buffer(b, mem, bits);

		for (int i = 0; i < 10; ++i)
			huff_put_bits(b, 17, 5);
		huff_flush(b);

		huff_set_buffer(b, mem, bits);

		for (int i = 0; i < 10; ++i)
		{
			int a = huff_get_bits(b, 5);
			int b = 17;
			CUTE_HUFF_CHECK(a == b, "Failed five bits test.");
		}

		huff_set_buffer(b, mem, bits);

		for (int i = 0; i < 10; ++i)
			huff_put_bits(b, (i & 1) ? 117 : 83, 7);
		huff_flush(b);

		huff_set_buffer(b, mem, bits);

		for (int i = 0; i < 10; ++i)
		{
			int a = huff_get_bits(b, 7);
			int b = (i & 1) ? 117 : 83;
			CUTE_HUFF_CHECK(a == b, "Failed seven bits test.");
		}

		return 1;
	}

#endif // CUTE_HUFF_INTERNAL_TESTS

#endif // CUTE_HUFF_IMPLEMENTATION_ONCE
#endif // CUTE_HUFF_IMPLEMENTATION

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
