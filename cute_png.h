/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_png.h - v1.04

	To create implementation (the function definitions)
		#define CUTE_PNG_IMPLEMENTATION
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
		1.04 (08/23/2018) various bug fixes for filter and word decoder
		                  added `cp_load_blank`


	EXAMPLES:

		Loading a PNG from disk, then freeing it
			cp_image_t img = cp_load_png("images/pic.png");
			...
			free(img.pix);
			CUTE_PNG_MEMSET(&img, 0, sizeof(img));

		Loading a PNG from memory, then freeing it
			cp_image_t img = cp_load_png_mem(memory, sizeof(memory));
			...
			free(img.pix);
			CUTE_PNG_MEMSET(&img, 0, sizeof(img));

		Saving a PNG to disk
			cp_save_png("images/example.png", &img);
			// img is just a raw RGBA buffer, and can come from anywhere,
			// not only from cp_load*** functions

		Creating a texture atlas in memory
			int w = 1024;
			int h = 1024;
			cp_atlas_image_t* imgs_out = (cp_atlas_image_t*)malloc(sizeof(cp_atlas_image_t) * my_png_count);
			cp_image_t atlas_img = cp_make_atlas(w, int h, my_png_array, my_png_count, imgs_out);
			// just pass an array of pointers to images along with the image count. Make sure to also
			// provide an array of `cp_atlas_image_t` for `cp_make_atlas` to output important UV info for the
			// images that fit into the atlas.

		Using the default atlas saver
			int errors = cp_default_save_atlas("atlas.png", "atlas.txt", atlas_img, atlas_imgs, img_count, names_of_all_images ? names_of_all_images : 0);
			if (errors) { ... }
			// Atlas info (like uv coordinates) are in "atlas.txt", and the image was writen to "atlas.png".
			// atlas_imgs was an array of `cp_atlas_image_t` from the `cp_make_atlas` function.

		Inflating a DEFLATE block (decompressing memory stored in DEFLATE format)
			cp_inflate(in, in_bytes, out, out_bytes);
			// this function requires knowledge of the un-compressed size
			// does *not* do any internal realloc! Will return errors if an
			// attempt to overwrite the out buffer is made
*/

/*
	Contributors:
		Zachary Carter    1.01 - bug catch for tRNS chunk in paletted images
		Dennis Korpel     1.03 - fix some pointer/memory related bugs
		Dennis Korpel     1.04 - fix for filter on first row of pixels
*/

#if !defined(CUTE_PNG_H)

#ifdef _WIN32

	#if !defined(_CRT_SECURE_NO_WARNINGS)
		#define _CRT_SECURE_NO_WARNINGS
	#endif

#endif

#define CUTE_PNG_ATLAS_MUST_FIT           1 // returns error from cp_make_atlas if *any* input image does not fit
#define CUTE_PNG_ATLAS_FLIP_Y_AXIS_FOR_UV 1 // flips output uv coordinate's y. Can be useful to "flip image on load"
#define CUTE_PNG_ATLAS_EMPTY_COLOR        0x000000FF // the fill color for empty areas in a texture atlas (RGBA)

#include <stdint.h>
#include <limits.h>

typedef struct cp_pixel_t cp_pixel_t;
typedef struct cp_image_t cp_image_t;
typedef struct cp_indexed_image_t cp_indexed_image_t;
typedef struct cp_atlas_image_t cp_atlas_image_t;

// Read this in the event of errors from any function
extern const char* cp_error_reason;

// return 1 for success, 0 for failures
int cp_inflate(void* in, int in_bytes, void* out, int out_bytes);
int cp_save_png(const char* file_name, const cp_image_t* img);

// Constructs an atlas image in-memory. The atlas pixels are stored in the returned image. free the pixels
// when done with them. The user must provide an array of cp_atlas_image_t for the `imgs` param. `imgs` holds
// information about uv coordinates for an associated image in the `pngs` array. Output image has NULL
// pixels buffer in the event of errors.
cp_image_t cp_make_atlas(int atlasWidth, int atlasHeight, const cp_image_t* pngs, int png_count, cp_atlas_image_t* imgs_out);

// A decent "default" function, ready to use out-of-the-box. Saves out an easy to parse text formatted info file
// along with an atlas image. `names` param can be optionally NULL.
int cp_default_save_atlas(const char* out_path_image, const char* out_path_atlas_txt, const cp_image_t* atlas, const cp_atlas_image_t* imgs, int img_count, const char** names);

// these two functions return cp_image_t::pix as 0 in event of errors
// call free on cp_image_t::pix when done, or call cp_free_png
cp_image_t cp_load_png(const char *file_name);
cp_image_t cp_load_png_mem(const void *png_data, int png_length);
cp_image_t cp_load_blank(int w, int h); // Alloc's pixels, but `pix` memory is uninitialized.
void cp_free_png(cp_image_t* img);
void cp_flip_image_horizontal(cp_image_t* img);

// Reads the w/h of the png without doing any other decompression or parsing.
void cp_load_png_wh(const void* png_data, int png_length, int* w, int* h);

// loads indexed (paletted) pngs, but does not depalette the image into RGBA pixels
// these two functions return cp_indexed_image_t::pix as 0 in event of errors
// call free on cp_indexed_image_t::pix when done, or call cp_free_indexed_png
cp_indexed_image_t cp_load_indexed_png(const char* file_name);
cp_indexed_image_t cp_load_indexed_png_mem(const void *png_data, int png_length);
void cp_free_indexed_png(cp_indexed_image_t* img);

// converts paletted image into a standard RGBA image
// call free on cp_image_t::pix when done
cp_image_t cp_depallete_indexed_image(cp_indexed_image_t* img);

// Pre-process the pixels to transform the image data to a premultiplied alpha format.
// Resource: http://www.essentialmath.com/GDC2015/VanVerth_Jim_DoingMathwRGB.pdf
void cp_premultiply(cp_image_t* img);

struct cp_pixel_t
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
};

struct cp_image_t
{
	int w;
	int h;
	cp_pixel_t* pix;
};

struct cp_indexed_image_t
{
	int w;
	int h;
	uint8_t* pix;
	uint8_t palette_len;
	cp_pixel_t palette[256];
};

struct cp_atlas_image_t
{
	int img_index;    // index into the `imgs` array
	int w, h;         // pixel w/h of original image
	float minx, miny; // u coordinate
	float maxx, maxy; // v coordinate
	int fit;          // non-zero if image fit and was placed into the atlas
};

#define CUTE_PNG_H
#endif

#ifdef CUTE_PNG_IMPLEMENTATION
#ifndef CUTE_PNG_IMPLEMENTATION_ONCE
#define CUTE_PNG_IMPLEMENTATION_ONCE

#if !defined(CUTE_PNG_ALLOCA)
	#define CUTE_PNG_ALLOCA alloca

	#ifdef _WIN32
		#include <malloc.h> // alloca
	#else
		#include <alloca.h> // alloca
	#endif
#endif

#if !defined(CUTE_PNG_ALLOC)
	#include <stdlib.h> // malloc, free, calloc
	#define CUTE_PNG_ALLOC malloc
	#define CUTE_PNG_FREE free
	#define CUTE_PNG_CALLOC calloc
#endif

#if !defined(CUTE_PNG_MEMCPY)
	#include <string.h> // memcpy
	#define CUTE_PNG_MEMCPY memcpy
#endif

#if !defined(CUTE_PNG_MEMSET)
	#include <string.h> // memset
	#define CUTE_PNG_MEMSET memset
#endif

#if !defined(CUTE_PNG_ASSERT)
	#include <assert.h> // assert
	#define CUTE_PNG_ASSERT assert
#endif

#include <stdio.h>  // fopen, fclose, etc.

static cp_pixel_t cp_make_pixel_a(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	cp_pixel_t p;
	p.r = r; p.g = g; p.b = b; p.a = a;
	return p;
}

static cp_pixel_t cp_make_pixel(uint8_t r, uint8_t g, uint8_t b)
{
	cp_pixel_t p;
	p.r = r; p.g = g; p.b = b; p.a = 0xFF;
	return p;
}

const char* cp_error_reason;
#define CUTE_PNG_FAIL() do { goto cp_err; } while (0)
#define CUTE_PNG_CHECK(X, Y) do { if (!(X)) { cp_error_reason = Y; CUTE_PNG_FAIL(); } } while (0)
#define CUTE_PNG_CALL(X) do { if (!(X)) goto cp_err; } while (0)
#define CUTE_PNG_LOOKUP_BITS 9
#define CUTE_PNG_LOOKUP_COUNT (1 << CUTE_PNG_LOOKUP_BITS)
#define CUTE_PNG_LOOKUP_MASK (CUTE_PNG_LOOKUP_COUNT - 1)
#define CUTE_PNG_DEFLATE_MAX_BITLEN 15

// DEFLATE tables from RFC 1951
uint8_t cp_fixed_table[288 + 32] = {
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,
	8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,8,8,8,8,8,8,8,8,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,5,
}; // 3.2.6
uint8_t cp_permutation_order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 }; // 3.2.7
uint8_t cp_len_extra_bits[29 + 2] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0,  0,0 }; // 3.2.5
uint32_t cp_len_base[29 + 2] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258,  0,0 }; // 3.2.5
uint8_t cp_dist_extra_bits[30 + 2] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13,  0,0 }; // 3.2.5
uint32_t cp_dist_base[30 + 2] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577, 0,0 }; // 3.2.5

typedef struct cp_state_t
{
	uint64_t bits;
	int count;
	uint32_t* words;
	int word_count;
	int word_index;
	int bits_left;

	int final_word_available;
	uint32_t final_word;

	char* out;
	char* out_end;
	char* begin;

	uint16_t lookup[CUTE_PNG_LOOKUP_COUNT];
	uint32_t lit[288];
	uint32_t dst[32];
	uint32_t len[19];
	uint32_t nlit;
	uint32_t ndst;
	uint32_t nlen;
} cp_state_t;

static int cp_would_overflow(cp_state_t* s, int num_bits)
{
	return (s->bits_left + s->count) - num_bits < 0;
}

static char* cp_ptr(cp_state_t* s)
{
	CUTE_PNG_ASSERT(!(s->bits_left & 7));
	return (char*)(s->words + s->word_index) - (s->count / 8);
}

static uint64_t cp_peak_bits(cp_state_t* s, int num_bits_to_read)
{
	if (s->count < num_bits_to_read)
	{
		if (s->word_index < s->word_count)
		{
			uint32_t word = s->words[s->word_index++];
			s->bits |= (uint64_t)word << s->count;
			s->count += 32;
			CUTE_PNG_ASSERT(s->word_index <= s->word_count);
		}

		else if (s->final_word_available)
		{
			uint32_t word = s->final_word;
			s->bits |= (uint64_t)word << s->count;
			s->count += s->bits_left;
			s->final_word_available = 0;
		}
	}

	return s->bits;
}

static uint32_t cp_consume_bits(cp_state_t* s, int num_bits_to_read)
{
	CUTE_PNG_ASSERT(s->count >= num_bits_to_read);
	uint32_t bits = s->bits & (((uint64_t)1 << num_bits_to_read) - 1);
	s->bits >>= num_bits_to_read;
	s->count -= num_bits_to_read;
	s->bits_left -= num_bits_to_read;
	return bits;
}

static uint32_t cp_read_bits(cp_state_t* s, int num_bits_to_read)
{
	CUTE_PNG_ASSERT(num_bits_to_read <= 32);
	CUTE_PNG_ASSERT(num_bits_to_read >= 0);
	CUTE_PNG_ASSERT(s->bits_left > 0);
	CUTE_PNG_ASSERT(s->count <= 64);
	CUTE_PNG_ASSERT(!cp_would_overflow(s, num_bits_to_read));
	cp_peak_bits(s, num_bits_to_read);
	uint32_t bits = cp_consume_bits(s, num_bits_to_read);
	return bits;
}

static char* cp_read_file_to_memory(const char* path, int* size)
{
	char* data = 0;
	FILE* fp = fopen(path, "rb");
	int sizeNum = 0;

	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		sizeNum = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		data = (char*)CUTE_PNG_ALLOC(sizeNum + 1);
		fread(data, sizeNum, 1, fp);
		data[sizeNum] = 0;
		fclose(fp);
	}

	if (size) *size = sizeNum;
	return data;
}

static uint32_t cp_rev16(uint32_t a)
{
	a = ((a & 0xAAAA) >>  1) | ((a & 0x5555) << 1);
	a = ((a & 0xCCCC) >>  2) | ((a & 0x3333) << 2);
	a = ((a & 0xF0F0) >>  4) | ((a & 0x0F0F) << 4);
	a = ((a & 0xFF00) >>  8) | ((a & 0x00FF) << 8);
	return a;
}

// RFC 1951 section 3.2.2
static int cp_build(cp_state_t* s, uint32_t* tree, uint8_t* lens, int sym_count)
{
	int n, codes[16], first[16], counts[16] = { 0 };

	// Frequency count
	for (n = 0; n < sym_count; n++) counts[lens[n]]++;

	// Distribute codes
	counts[0] = codes[0] = first[0] = 0;
	for (n = 1; n <= 15; ++n)
	{
		codes[n] = (codes[n - 1] + counts[n - 1]) << 1;
		first[n] = first[n - 1] + counts[n - 1];
	}

	if (s) CUTE_PNG_MEMSET(s->lookup, 0, sizeof(s->lookup));
	for (int i = 0; i < sym_count; ++i)
	{
		int len = lens[i];

		if (len != 0)
		{
			CUTE_PNG_ASSERT(len < 16);
			uint32_t code = codes[len]++;
			uint32_t slot = first[len]++;
			tree[slot] = (code << (32 - len)) | (i << 4) | len;

			if (s && len <= CUTE_PNG_LOOKUP_BITS)
			{
				int j = cp_rev16(code) >> (16 - len);
				while (j < (1 << CUTE_PNG_LOOKUP_BITS))
				{
					s->lookup[j] = (uint16_t)((len << CUTE_PNG_LOOKUP_BITS) | i);
					j += (1 << len);
				}
			}
		}
	}

	int max_index = first[15];
	return max_index;
}

static int cp_stored(cp_state_t* s)
{
	char* p;

	// 3.2.3
	// skip any remaining bits in current partially processed byte
	cp_read_bits(s, s->count & 7);

	// 3.2.4
	// read LEN and NLEN, should complement each other
	uint16_t LEN = (uint16_t)cp_read_bits(s, 16);
	uint16_t NLEN = (uint16_t)cp_read_bits(s, 16);
	CUTE_PNG_CHECK(LEN == (uint16_t)(~NLEN), "Failed to find LEN and NLEN as complements within stored (uncompressed) stream.");
	CUTE_PNG_CHECK(s->bits_left / 8 <= (int)LEN, "Stored block extends beyond end of input stream.");
	p = cp_ptr(s);
	CUTE_PNG_MEMCPY(s->out, p, LEN);
	s->out += LEN;
	return 1;

cp_err:
	return 0;
}

// 3.2.6
static int cp_fixed(cp_state_t* s)
{
	s->nlit = cp_build(s, s->lit, cp_fixed_table, 288);
	s->ndst = cp_build(0, s->dst, cp_fixed_table + 288, 32);
	return 1;
}

static int cp_decode(cp_state_t* s, uint32_t* tree, int hi)
{
	uint64_t bits = cp_peak_bits(s, 16);
	uint32_t search = (cp_rev16((uint32_t)bits) << 16) | 0xFFFF;
	int lo = 0;
	while (lo < hi)
	{
		int guess = (lo + hi) >> 1;
		if (search < tree[guess]) hi = guess;
		else lo = guess + 1;
	}

	uint32_t key = tree[lo - 1];
	uint32_t len = (32 - (key & 0xF));
	CUTE_PNG_ASSERT((search >> len) == (key >> len));

	int code = cp_consume_bits(s, key & 0xF);
	(void)code;
	return (key >> 4) & 0xFFF;
}

// 3.2.7
static int cp_dynamic(cp_state_t* s)
{
	uint8_t lenlens[19] = { 0 };

	int nlit = 257 + cp_read_bits(s, 5);
	int ndst = 1 + cp_read_bits(s, 5);
	int nlen = 4 + cp_read_bits(s, 4);

	for (int i = 0 ; i < nlen; ++i)
		lenlens[cp_permutation_order[i]] = (uint8_t)cp_read_bits(s, 3);

	// Build the tree for decoding code lengths
	s->nlen = cp_build(0, s->len, lenlens, 19);
	uint8_t lens[288 + 32];

	for (int n = 0; n < nlit + ndst;)
	{
		int sym = cp_decode(s, s->len, s->nlen);
		switch (sym)
		{
		case 16: for (int i =  3 + cp_read_bits(s, 2); i; --i, ++n) lens[n] = lens[n - 1]; break;
		case 17: for (int i =  3 + cp_read_bits(s, 3); i; --i, ++n) lens[n] = 0; break;
		case 18: for (int i = 11 + cp_read_bits(s, 7); i; --i, ++n) lens[n] = 0; break;
		default: lens[n++] = (uint8_t)sym; break;
		}
	}

	s->nlit = cp_build(s, s->lit, lens, nlit);
	s->ndst = cp_build(0, s->dst, lens + nlit, ndst);
	return 1;
}

// 3.2.3
static int cp_block(cp_state_t* s)
{
	while (1)
	{
		int symbol = cp_decode(s, s->lit, s->nlit);

		if (symbol < 256)
		{
			CUTE_PNG_CHECK(s->out + 1 <= s->out_end, "Attempted to overwrite out buffer while outputting a symbol.");
			*s->out = (char)symbol;
			s->out += 1;
		}

		else if (symbol > 256)
		{
			symbol -= 257;
			int length = cp_read_bits(s, cp_len_extra_bits[symbol]) + cp_len_base[symbol];
			int distance_symbol = cp_decode(s, s->dst, s->ndst);
			int backwards_distance = cp_read_bits(s, cp_dist_extra_bits[distance_symbol]) + cp_dist_base[distance_symbol];
			CUTE_PNG_CHECK(s->out - backwards_distance >= s->begin, "Attempted to write before out buffer (invalid backwards distance).");
			CUTE_PNG_CHECK(s->out + length <= s->out_end, "Attempted to overwrite out buffer while outputting a string.");
			char* src = s->out - backwards_distance;
			char* dst = s->out;
			s->out += length;

			switch (backwards_distance)
			{
			case 1: // very common in images
				CUTE_PNG_MEMSET(dst, *src, length);
				break;
			default: while (length--) *dst++ = *src++;
			}
		}

		else break;
	}

	return 1;

cp_err:
	return 0;
}

// 3.2.3
int cp_inflate(void* in, int in_bytes, void* out, int out_bytes)
{
	cp_state_t* s = (cp_state_t*)CUTE_PNG_CALLOC(1, sizeof(cp_state_t));
	s->bits = 0;
	s->count = 0;
	s->word_index = 0;
	s->bits_left = in_bytes * 8;

	// s->words is the in-pointer rounded up to a multiple of 4
	int first_bytes = (int) ((( (size_t) in + 3) & ~3) - (size_t) in);
	s->words = (uint32_t*)((char*)in + first_bytes);
	s->word_count = (in_bytes - first_bytes) / 4;
	int last_bytes = ((in_bytes - first_bytes) & 3);

	for (int i = 0; i < first_bytes; ++i)
		s->bits |= (uint64_t)(((uint8_t*)in)[i]) << (i * 8);

	s->final_word_available = last_bytes ? 1 : 0;
	s->final_word = 0;
	for(int i = 0; i < last_bytes; i++) 
		s->final_word |= ((uint8_t*)in)[in_bytes - last_bytes+i] << (i * 8);

	s->count = first_bytes * 8;

	s->out = (char*)out;
	s->out_end = s->out + out_bytes;
	s->begin = (char*)out;

	int count = 0;
	int bfinal;
	do
	{
		bfinal = cp_read_bits(s, 1);
		int btype = cp_read_bits(s, 2);

		switch (btype)
		{
		case 0: CUTE_PNG_CALL(cp_stored(s)); break;
		case 1: cp_fixed(s); CUTE_PNG_CALL(cp_block(s)); break;
		case 2: cp_dynamic(s); CUTE_PNG_CALL(cp_block(s)); break;
		case 3: CUTE_PNG_CHECK(0, "Detected unknown block type within input stream.");
		}

		++count;
	}
	while (!bfinal);

	CUTE_PNG_FREE(s);
	return 1;

cp_err:
	CUTE_PNG_FREE(s);
	return 0;
}

static uint8_t cp_paeth(uint8_t a, uint8_t b, uint8_t c)
{
	int p = a + b - c;
	int pa = abs(p - a);
	int pb = abs(p - b);
	int pc = abs(p - c);
	return (pa <= pb && pa <= pc) ? a : (pb <= pc) ? b : c;
}

typedef struct cp_save_png_data_t
{
	uint32_t crc;
	uint32_t adler;
	uint32_t bits;
	uint32_t prev;
	uint32_t runlen;
	FILE *fp;
} cp_save_png_data_t;

uint32_t CP_CRC_TABLE[] = {
	0, 0x1db71064, 0x3b6e20c8, 0x26d930ac, 0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
	0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c, 0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

static void cp_put8(cp_save_png_data_t* s, uint32_t a)
{
	fputc(a, s->fp);
	s->crc = (s->crc >> 4) ^ CP_CRC_TABLE[(s->crc & 15) ^ (a & 15)];
	s->crc = (s->crc >> 4) ^ CP_CRC_TABLE[(s->crc & 15) ^ (a >> 4)];
}

static void cp_update_adler(cp_save_png_data_t* s, uint32_t v)
{
	uint32_t s1 = s->adler & 0xFFFF;
	uint32_t s2 = (s->adler >> 16) & 0xFFFF;
	s1 = (s1 + v) % 65521;
	s2 = (s2 + s1) % 65521;
	s->adler = (s2 << 16) + s1;
}

static void cp_put32(cp_save_png_data_t* s, uint32_t v)
{
	cp_put8(s, (v >> 24) & 0xFF);
	cp_put8(s, (v >> 16) & 0xFF);
	cp_put8(s, (v >> 8) & 0xFF);
	cp_put8(s, v & 0xFF);
}

static void cp_put_bits(cp_save_png_data_t* s, uint32_t data, uint32_t bitcount)
{
	while (bitcount--)
	{
		uint32_t prev = s->bits;
		s->bits = (s->bits >> 1) | ((data & 1) << 7);
		data >>= 1;

		if (prev & 1)
		{
			cp_put8(s, s->bits);
			s->bits = 0x80;
		}
	}
}

static void cp_put_bitsr(cp_save_png_data_t* s, uint32_t data, uint32_t bitcount)
{
	while (bitcount--)
		cp_put_bits(s, data >> bitcount, 1);
}

static void cp_begin_chunk(cp_save_png_data_t* s, const char* id, uint32_t len)
{
	cp_put32(s, len);
	s->crc = 0xFFFFFFFF;
	cp_put8(s, id[0]);
	cp_put8(s, id[1]);
	cp_put8(s, id[2]);
	cp_put8(s, id[3]);
}

static void cp_encode_literal(cp_save_png_data_t* s, uint32_t v)
{
	// Encode a literal/length using the built-in tables.
	// Could do better with a custom table but whatever.
	     if (v < 144) cp_put_bitsr(s, 0x030 + v -   0, 8);
	else if (v < 256) cp_put_bitsr(s, 0x190 + v - 144, 9);
	else if (v < 280) cp_put_bitsr(s, 0x000 + v - 256, 7);
	else              cp_put_bitsr(s, 0x0c0 + v - 280, 8);
}

static void cp_encode_len(cp_save_png_data_t* s, uint32_t code, uint32_t bits, uint32_t len)
{
	cp_encode_literal(s, code + (len >> bits));
	cp_put_bits(s, len, bits);
	cp_put_bits(s, 0, 5);
}

static void cp_end_run(cp_save_png_data_t* s)
{
	s->runlen--;
	cp_encode_literal(s, s->prev);

	if (s->runlen >= 67) cp_encode_len(s, 277, 4, s->runlen - 67);
	else if (s->runlen >= 35) cp_encode_len(s, 273, 3, s->runlen - 35);
	else if (s->runlen >= 19) cp_encode_len(s, 269, 2, s->runlen - 19);
	else if (s->runlen >= 11) cp_encode_len(s, 265, 1, s->runlen - 11);
	else if (s->runlen >= 3) cp_encode_len(s, 257, 0, s->runlen - 3);
	else while (s->runlen--) cp_encode_literal(s, s->prev);
}

static void cp_encode_byte(cp_save_png_data_t *s, uint8_t v)
{
	cp_update_adler(s, v);

	// Simple RLE compression. We could do better by doing a search
	// to find matches, but this works pretty well TBH.
	if (s->prev == v && s->runlen < 115) s->runlen++;

	else
	{
		if (s->runlen) cp_end_run(s);

		s->prev = v;
		s->runlen = 1;
	}
}

static void cp_save_header(cp_save_png_data_t* s, cp_image_t* img)
{
	fwrite("\211PNG\r\n\032\n", 8, 1, s->fp);
	cp_begin_chunk(s, "IHDR", 13);
	cp_put32(s, img->w);
	cp_put32(s, img->h);
	cp_put8(s, 8); // bit depth
	cp_put8(s, 6); // RGBA
	cp_put8(s, 0); // compression (deflate)
	cp_put8(s, 0); // filter (standard)
	cp_put8(s, 0); // interlace off
	cp_put32(s, ~s->crc);
}

static long cp_save_data(cp_save_png_data_t* s, cp_image_t* img, long dataPos)
{
	cp_begin_chunk(s, "IDAT", 0);
	cp_put8(s, 0x08); // zlib compression method
	cp_put8(s, 0x1D); // zlib compression flags
	cp_put_bits(s, 3, 3); // zlib last block + fixed dictionary

	for (int y = 0; y < img->h; ++y)
	{
		cp_pixel_t *row = &img->pix[y * img->w];
		cp_pixel_t prev = cp_make_pixel_a(0, 0, 0, 0);

		cp_encode_byte(s, 1); // sub filter
		for (int x = 0; x < img->w; ++x)
		{
			cp_encode_byte(s, row[x].r - prev.r);
			cp_encode_byte(s, row[x].g - prev.g);
			cp_encode_byte(s, row[x].b - prev.b);
			cp_encode_byte(s, row[x].a - prev.a);
			prev = row[x];
		}
	}

	cp_end_run(s);
	cp_encode_literal(s, 256); // terminator
	while (s->bits != 0x80) cp_put_bits(s, 0, 1);
	cp_put32(s, s->adler);
	long dataSize = (ftell(s->fp) - dataPos) - 8;
	cp_put32(s, ~s->crc);

	return dataSize;
}

int cp_save_png(const char* file_name, const cp_image_t* img)
{
	cp_save_png_data_t s;
	long dataPos, dataSize, err;

	FILE* fp = fopen(file_name, "wb");
	if (!fp) return 1;

	s.fp = fp;
	s.adler = 1;
	s.bits = 0x80;
	s.prev = 0xFFFF;
	s.runlen = 0;

	cp_save_header(&s, (cp_image_t*)img);
	dataPos = ftell(s.fp);
	dataSize = cp_save_data(&s, (cp_image_t*)img, dataPos);

	// End chunk.
	cp_begin_chunk(&s, "IEND", 0);
	cp_put32(&s, ~s.crc);

	// Write back payload size.
	fseek(fp, dataPos, SEEK_SET);
	cp_put32(&s, dataSize);

	err = ferror(fp);
	fclose(fp);
	return !err;
}

typedef struct cp_raw_png_t
{
	const uint8_t* p;
	const uint8_t* end;
} cp_raw_png_t;

static uint32_t cp_make32(const uint8_t* s)
{
	return (s[0] << 24) | (s[1] << 16) | (s[2] << 8) | s[3];
}

static const uint8_t* cp_chunk(cp_raw_png_t* png, const char* chunk, uint32_t minlen)
{
	uint32_t len = cp_make32(png->p);
	const uint8_t* start = png->p;

	if (!memcmp(start + 4, chunk, 4) && len >= minlen)
	{
		int offset = len + 12;

		if (png->p + offset <= png->end)
		{
			png->p += offset;
			return start + 8;
		}
	}

	return 0;
}

static const uint8_t* cp_find(cp_raw_png_t* png, const char* chunk, uint32_t minlen)
{
	const uint8_t *start;
	while (png->p < png->end)
	{
		uint32_t len = cp_make32(png->p);
		start = png->p;
		png->p += len + 12;

		if (!memcmp(start+4, chunk, 4) && len >= minlen && png->p <= png->end)
			return start + 8;
	}

	return 0;
}

static int cp_unfilter(int w, int h, int bpp, uint8_t* raw)
{
	int len = w * bpp;
	uint8_t *prev;
	int x;

	if (h > 0)
	{
#define FILTER_LOOP_FIRST(A) for (x = bpp; x < len; x++) raw[x] += A; break
		switch (*raw++)
		{
		case 0: break;
		case 1: FILTER_LOOP_FIRST(raw[x - bpp]);
		case 2: break;
		case 3: FILTER_LOOP_FIRST(raw[x - bpp] / 2);
		case 4: FILTER_LOOP_FIRST(cp_paeth(raw[x - bpp], 0, 0));
		default: return 0;
		}
#undef FILTER_LOOP_FIRST
	}

	prev = raw;
	raw += len;

	for (int y = 1; y < h; y++, prev = raw, raw += len)
	{
#define FILTER_LOOP(A, B) for (x = 0 ; x < bpp; x++) raw[x] += A; for (; x < len; x++) raw[x] += B; break
		switch (*raw++)
		{
		case 0: break;
		case 1: FILTER_LOOP(0          , raw[x - bpp] );
		case 2: FILTER_LOOP(prev[x]    , prev[x]);
		case 3: FILTER_LOOP(prev[x] / 2, (raw[x - bpp] + prev[x]) / 2);
		case 4: FILTER_LOOP(prev[x]    , cp_paeth(raw[x - bpp], prev[x], prev[x -bpp]));
		default: return 0;
		}
#undef FILTER_LOOP
	}

	return 1;
}

static void cp_convert(int bpp, int w, int h, uint8_t* src, cp_pixel_t* dst)
{
	for (int y = 0; y < h; y++)
	{
		// skip filter byte
		src++;

		for (int x = 0; x < w; x++, src += bpp)
		{
			switch (bpp)
			{
				case 1: *dst++ = cp_make_pixel(src[0], src[0], src[0]); break;
				case 2: *dst++ = cp_make_pixel_a(src[0], src[0], src[0], src[1]); break;
				case 3: *dst++ = cp_make_pixel(src[0], src[1], src[2]); break;
				case 4: *dst++ = cp_make_pixel_a(src[0], src[1], src[2], src[3]); break;
			}
		}
	}
}

// http://www.libpng.org/pub/png/spec/1.2/PNG-Chunks.html#C.tRNS
static uint8_t cp_get_alpha_for_indexed_image(int index, const uint8_t* trns, uint32_t trns_len)
{
	if (!trns) return 255;
	else if ((uint32_t)index >= trns_len) return 255;
	else return trns[index];
}

static void cp_depalette(int w, int h, uint8_t* src, cp_pixel_t* dst, const uint8_t* plte, const uint8_t* trns, uint32_t trns_len)
{
	for (int y = 0; y < h; ++y)
	{
		// skip filter byte
		++src;

		for (int x = 0; x < w; ++x, ++src)
		{
			int c = *src;
			uint8_t r = plte[c * 3];
			uint8_t g = plte[c * 3 + 1];
			uint8_t b = plte[c * 3 + 2];
			uint8_t a = cp_get_alpha_for_indexed_image(c, trns, trns_len);
			*dst++ = cp_make_pixel_a(r, g, b, a);
		}
	}
}

static uint32_t cp_get_chunk_byte_length(const uint8_t* chunk)
{
	return cp_make32(chunk - 8);
}

static int cp_out_size(cp_image_t* img, int bpp)
{
	return (img->w + 1) * img->h * bpp;
}

cp_image_t cp_load_png_mem(const void* png_data, int png_length)
{
	const char* sig = "\211PNG\r\n\032\n";
	const uint8_t* ihdr, *first, *plte, *trns;
	int bit_depth, color_type, bpp, w, h, pix_bytes;
	int compression, filter, interlace;
	int datalen, offset;
	uint8_t* out;
	cp_image_t img = { 0 };
	uint8_t* data = 0;
	cp_raw_png_t png;
	png.p = (uint8_t*)png_data;
	png.end = (uint8_t*)png_data + png_length;

	CUTE_PNG_CHECK(!memcmp(png.p, sig, 8), "incorrect file signature (is this a png file?)");
	png.p += 8;

	ihdr = cp_chunk(&png, "IHDR", 13);
	CUTE_PNG_CHECK(ihdr, "unable to find IHDR chunk");
	bit_depth = ihdr[8];
	color_type = ihdr[9];
	CUTE_PNG_CHECK(bit_depth == 8, "only bit-depth of 8 is supported");

	switch (color_type)
	{
		case 0: bpp = 1; break; // greyscale
		case 2: bpp = 3; break; // RGB
		case 3: bpp = 1; break; // paletted
		case 4: bpp = 2; break; // grey+alpha
		case 6: bpp = 4; break; // RGBA
		default: CUTE_PNG_CHECK(0, "unknown color type");
	}

	// +1 for filter byte (which is dumb! just stick this at file header...)
	w = cp_make32(ihdr) + 1;
	h = cp_make32(ihdr + 4);
	CUTE_PNG_CHECK(w >= 1, "invalid IHDR chunk found, image width was less than 1");
	CUTE_PNG_CHECK(h >= 1, "invalid IHDR chunk found, image height was less than 1");
	pix_bytes = w * h * sizeof(cp_pixel_t);
	img.w = w - 1;
	img.h = h;
	img.pix = (cp_pixel_t*)CUTE_PNG_ALLOC(pix_bytes);
	CUTE_PNG_CHECK(img.pix, "unable to allocate raw image space");

	compression = ihdr[10];
	filter = ihdr[11];
	interlace = ihdr[12];
	CUTE_PNG_CHECK(!compression, "only standard compression DEFLATE is supported");
	CUTE_PNG_CHECK(!filter, "only standard adaptive filtering is supported");
	CUTE_PNG_CHECK(!interlace, "interlacing is not supported");

	// PLTE must come before any IDAT chunk
	first = png.p;
	plte = cp_find(&png, "PLTE", 0);
	if (!plte) png.p = first;
	else first = png.p;

	// tRNS can come after PLTE
	trns = cp_find(&png, "tRNS", 0);
	if (!trns) png.p = first;
	else first = png.p;

	// Compute length of the DEFLATE stream through IDAT chunk data sizes
	datalen = 0;
	for (const uint8_t* idat = cp_find(&png, "IDAT", 0); idat; idat = cp_chunk(&png, "IDAT", 0))
	{
		uint32_t len = cp_get_chunk_byte_length(idat);
		datalen += len;
	}

	// Copy in IDAT chunk data sections to form the compressed DEFLATE stream
	png.p = first;
	data = (uint8_t*)CUTE_PNG_ALLOC(datalen);
	offset = 0;
	for (const uint8_t* idat = cp_find(&png, "IDAT", 0); idat; idat = cp_chunk(&png, "IDAT", 0))
	{
		uint32_t len = cp_get_chunk_byte_length(idat);
		CUTE_PNG_MEMCPY(data + offset, idat, len);
		offset += len;
	}

	// check for proper zlib structure in DEFLATE stream
	CUTE_PNG_CHECK(data && datalen >= 6, "corrupt zlib structure in DEFLATE stream");
	CUTE_PNG_CHECK((data[0] & 0x0f) == 0x08, "only zlib compression method (RFC 1950) is supported");
	CUTE_PNG_CHECK((data[0] & 0xf0) <= 0x70, "innapropriate window size detected");
	CUTE_PNG_CHECK(!(data[1] & 0x20), "preset dictionary is present and not supported");

	// check for integer overflow
	CUTE_PNG_CHECK(cp_out_size(&img, 4) >= 1, "invalid image size found");
	CUTE_PNG_CHECK(cp_out_size(&img, bpp) >= 1, "invalid image size found");

	out = (uint8_t*)img.pix + cp_out_size(&img, 4) - cp_out_size(&img, bpp);
	CUTE_PNG_CHECK(cp_inflate(data + 2, datalen - 6, out, pix_bytes), "DEFLATE algorithm failed");
	CUTE_PNG_CHECK(cp_unfilter(img.w, img.h, bpp, out), "invalid filter byte found");

	if (color_type == 3)
	{
		CUTE_PNG_CHECK(plte, "color type of indexed requires a PLTE chunk");
		uint32_t trns_len = trns ? cp_get_chunk_byte_length(trns) : 0;
		cp_depalette(img.w, img.h, out, img.pix, plte, trns, trns_len);
	}
	else cp_convert(bpp, img.w, img.h, out, img.pix);

	CUTE_PNG_FREE(data);
	return img;

cp_err:
	CUTE_PNG_FREE(data);
	CUTE_PNG_FREE(img.pix);
	img.pix = 0;

	return img;
}

cp_image_t cp_load_blank(int w, int h)
{
	cp_image_t img;
	img.w = w;
	img.h = h;
	img.pix = (cp_pixel_t*)CUTE_PNG_ALLOC(w * h * sizeof(cp_pixel_t));
	return img;
}

cp_image_t cp_load_png(const char *file_name)
{
	cp_image_t img = { 0 };
	int len;
	void* data = cp_read_file_to_memory(file_name, &len);
	if (!data) return img;
	img = cp_load_png_mem(data, len);
	CUTE_PNG_FREE(data);
	return img;
}

void cp_free_png(cp_image_t* img)
{
	CUTE_PNG_FREE(img->pix);
	img->pix = 0;
	img->w = img->h = 0;
}

void cp_flip_image_horizontal(cp_image_t* img)
{
	cp_pixel_t* pix = img->pix;
	int w = img->w;
	int h = img->h;
	int flips = h / 2;
	for (int i = 0; i < flips; ++i)
	{
		cp_pixel_t* a = pix + w * i;
		cp_pixel_t* b = pix + w * (h - i - 1);
		for (int j = 0; j < w; ++j)
		{
			cp_pixel_t t = *a;
			*a = *b;
			*b = t;
			++a;
			++b;
		}
	}
}

void cp_load_png_wh(const void* png_data, int png_length, int* w_out, int* h_out)
{
	const char* sig = "\211PNG\r\n\032\n";
	const uint8_t* ihdr;
	cp_raw_png_t png;
	int w, h;
	png.p = (uint8_t*)png_data;
	png.end = (uint8_t*)png_data + png_length;

	if (w_out) *w_out = 0;
	if (h_out) *h_out = 0;

	CUTE_PNG_CHECK(!memcmp(png.p, sig, 8), "incorrect file signature (is this a png file?)");
	png.p += 8;

	ihdr = cp_chunk(&png, "IHDR", 13);
	CUTE_PNG_CHECK(ihdr, "unable to find IHDR chunk");

	// +1 for filter byte (which is dumb! just stick this at file header...)
	w = cp_make32(ihdr) + 1;
	h = cp_make32(ihdr + 4);
	if (w_out) *w_out = w - 1;
	if (h_out) *h_out = h;

	cp_err:;
}

cp_indexed_image_t cp_load_indexed_png(const char* file_name)
{
	cp_indexed_image_t img = { 0 };
	int len;
	void* data = cp_read_file_to_memory(file_name, &len);
	if (!data) return img;
	img = cp_load_indexed_png_mem(data, len);
	CUTE_PNG_FREE(data);
	return img;
}

static void cp_unpack_indexed_rows(int w, int h, uint8_t* src, uint8_t* dst)
{
	for (int y = 0; y < h; ++y)
	{
		// skip filter byte
		++src;

		for (int x = 0; x < w; ++x, ++src)
		{
			*dst++ = *src;
		}
	}
}

void cp_unpack_palette(cp_pixel_t* dst, const uint8_t* plte, int plte_len, const uint8_t* trns, int trns_len)
{
	for (int i = 0; i < plte_len * 3; i += 3)
	{
		unsigned char r = plte[i];
		unsigned char g = plte[i + 1];
		unsigned char b = plte[i + 2];
		unsigned char a = cp_get_alpha_for_indexed_image(i / 3, trns, trns_len);
		cp_pixel_t p = cp_make_pixel_a(r, g, b, a);
		*dst++ = p;
	}
}

cp_indexed_image_t cp_load_indexed_png_mem(const void *png_data, int png_length)
{
	const char* sig = "\211PNG\r\n\032\n";
	const uint8_t* ihdr, *first, *plte, *trns;
	int bit_depth, color_type, bpp, w, h, pix_bytes;
	int compression, filter, interlace;
	int datalen, offset;
	int plte_len;
	uint8_t* out;
	cp_indexed_image_t img = { 0 };
	uint8_t* data = 0;
	cp_raw_png_t png;
	png.p = (uint8_t*)png_data;
	png.end = (uint8_t*)png_data + png_length;

	CUTE_PNG_CHECK(!memcmp(png.p, sig, 8), "incorrect file signature (is this a png file?)");
	png.p += 8;

	ihdr = cp_chunk(&png, "IHDR", 13);
	CUTE_PNG_CHECK(ihdr, "unable to find IHDR chunk");
	bit_depth = ihdr[8];
	color_type = ihdr[9];
	bpp = 1; // bytes per pixel
	CUTE_PNG_CHECK(bit_depth == 8, "only bit-depth of 8 is supported");
	CUTE_PNG_CHECK(color_type == 3, "only indexed png images (images with a palette) are valid for cp_load_indexed_png_mem");

	// +1 for filter byte (which is dumb! just stick this at file header...)
	w = cp_make32(ihdr) + 1;
	h = cp_make32(ihdr + 4);
	pix_bytes = w * h * sizeof(uint8_t);
	img.w = w - 1;
	img.h = h;
	img.pix = (uint8_t*)CUTE_PNG_ALLOC(pix_bytes);
	CUTE_PNG_CHECK(img.pix, "unable to allocate raw image space");

	compression = ihdr[10];
	filter = ihdr[11];
	interlace = ihdr[12];
	CUTE_PNG_CHECK(!compression, "only standard compression DEFLATE is supported");
	CUTE_PNG_CHECK(!filter, "only standard adaptive filtering is supported");
	CUTE_PNG_CHECK(!interlace, "interlacing is not supported");

	// PLTE must come before any IDAT chunk
	first = png.p;
	plte = cp_find(&png, "PLTE", 0);
	if (!plte) png.p = first;
	else first = png.p;

	// tRNS can come after PLTE
	trns = cp_find(&png, "tRNS", 0);
	if (!trns) png.p = first;
	else first = png.p;

	// Compute length of the DEFLATE stream through IDAT chunk data sizes
	datalen = 0;
	for (const uint8_t* idat = cp_find(&png, "IDAT", 0); idat; idat = cp_chunk(&png, "IDAT", 0))
	{
		uint32_t len = cp_get_chunk_byte_length(idat);
		datalen += len;
	}

	// Copy in IDAT chunk data sections to form the compressed DEFLATE stream
	png.p = first;
	data = (uint8_t*)CUTE_PNG_ALLOC(datalen);
	offset = 0;
	for (const uint8_t* idat = cp_find(&png, "IDAT", 0); idat; idat = cp_chunk(&png, "IDAT", 0))
	{
		uint32_t len = cp_get_chunk_byte_length(idat);
		CUTE_PNG_MEMCPY(data + offset, idat, len);
		offset += len;
	}

	// check for proper zlib structure in DEFLATE stream
	CUTE_PNG_CHECK(data && datalen >= 6, "corrupt zlib structure in DEFLATE stream");
	CUTE_PNG_CHECK((data[0] & 0x0f) == 0x08, "only zlib compression method (RFC 1950) is supported");
	CUTE_PNG_CHECK((data[0] & 0xf0) <= 0x70, "innapropriate window size detected");
	CUTE_PNG_CHECK(!(data[1] & 0x20), "preset dictionary is present and not supported");

	out = img.pix;
	CUTE_PNG_CHECK(cp_inflate(data + 2, datalen - 6, out, pix_bytes), "DEFLATE algorithm failed");
	CUTE_PNG_CHECK(cp_unfilter(img.w, img.h, bpp, out), "invalid filter byte found");
	cp_unpack_indexed_rows(img.w, img.h, out, img.pix);

	plte_len = cp_get_chunk_byte_length(plte) / 3;
	cp_unpack_palette(img.palette, plte, plte_len, trns, cp_get_chunk_byte_length(trns));
	img.palette_len = (uint8_t)plte_len;

	CUTE_PNG_FREE(data);
	return img;

cp_err:
	CUTE_PNG_FREE(data);
	CUTE_PNG_FREE(img.pix);
	img.pix = 0;

	return img;
}

void cp_free_indexed_png(cp_indexed_image_t* img)
{
	CUTE_PNG_FREE(img->pix);
	img->pix = 0;
	img->w = img->h = 0;
}

cp_image_t cp_depallete_indexed_image(cp_indexed_image_t* img)
{
	cp_image_t out = { 0 };
	out.w = img->w;
	out.h = img->h;
	out.pix = (cp_pixel_t*)CUTE_PNG_ALLOC(sizeof(cp_pixel_t) * out.w * out.h);

	cp_pixel_t* dst = out.pix;
	uint8_t* src = img->pix;

	for (int y = 0; y < out.h; ++y)
	{
		for (int x = 0; x < out.w; ++x)
		{
			int index = *src++;
			cp_pixel_t p = img->palette[index];
			*dst++ = p;
		}
	}

	return out;
}

typedef struct cp_v2i_t
{
	int x;
	int y;
} cp_v2i_t;

typedef struct cp_integer_image_t
{
	int img_index;
	cp_v2i_t size;
	cp_v2i_t min;
	cp_v2i_t max;
	int fit;
} cp_integer_image_t;

static cp_v2i_t cp_v2i(int x, int y)
{
	cp_v2i_t v;
	v.x = x;
	v.y = y;
	return v;
}

static cp_v2i_t cp_sub(cp_v2i_t a, cp_v2i_t b)
{
	cp_v2i_t v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	return v;
}

static cp_v2i_t cp_add(cp_v2i_t a, cp_v2i_t b)
{
	cp_v2i_t v;
	v.x = a.x + b.x;
	v.y = a.y + b.y;
	return v;
}

typedef struct cp_atlas_node_t
{
	cp_v2i_t size;
	cp_v2i_t min;
	cp_v2i_t max;
} cp_atlas_node_t;

static cp_atlas_node_t* cp_best_fit(int sp, const cp_image_t* png, cp_atlas_node_t* nodes)
{
	int bestVolume = INT_MAX;
	cp_atlas_node_t *best_node = 0;
	int width = png->w;
	int height = png->h;
	int png_volume = width * height;

	for (int i = 0; i < sp; ++i)
	{
		cp_atlas_node_t *node = nodes + i;
		int can_contain = node->size.x >= width && node->size.y >= height;
		if (can_contain)
		{
			int node_volume = node->size.x * node->size.y;
			if (node_volume == png_volume) return node;
			if (node_volume < bestVolume)
			{
				bestVolume = node_volume;
				best_node = node;
			}
		}
	}

	return best_node;
}

static int cp_perimeter_pred(cp_integer_image_t* a, cp_integer_image_t* b)
{
	int perimeterA = 2 * (a->size.x + a->size.y);
	int perimeterB = 2 * (b->size.x + b->size.y);
	return perimeterB < perimeterA;
}

void cp_premultiply(cp_image_t* img)
{
	int w = img->w;
	int h = img->h;
	int stride = w * sizeof(cp_pixel_t);
	uint8_t* data = (uint8_t*)img->pix;

	for(int i = 0; i < (int)stride * h; i += sizeof(cp_pixel_t))
	{
		float a = (float)data[i + 3] / 255.0f;
		float r = (float)data[i + 0] / 255.0f;
		float g = (float)data[i + 1] / 255.0f;
		float b = (float)data[i + 2] / 255.0f;
		r *= a;
		g *= a;
		b *= a;
		data[i + 0] = (uint8_t)(r * 255.0f);
		data[i + 1] = (uint8_t)(g * 255.0f);
		data[i + 2] = (uint8_t)(b * 255.0f);
	}
}

static void cp_qsort(cp_integer_image_t* items, int count)
{
	if (count <= 1) return;

	cp_integer_image_t pivot = items[count - 1];
	int low = 0;
	for (int i = 0; i < count - 1; ++i)
	{
		if (cp_perimeter_pred(items + i, &pivot))
		{
			cp_integer_image_t tmp = items[i];
			items[i] = items[low];
			items[low] = tmp;
			low++;
		}
	}

	items[count - 1] = items[low];
	items[low] = pivot;
	cp_qsort(items, low);
	cp_qsort(items + low + 1, count - 1 - low);
}

static void cp_write_pixel(char* mem, long color) {
	mem[0] = (color >> 24) & 0xFF;
	mem[1] = (color >> 16) & 0xFF;
	mem[2] = (color >>  8) & 0xFF;
	mem[3] = (color >>  0) & 0xFF;
}

cp_image_t cp_make_atlas(int atlas_width, int atlas_height, const cp_image_t* pngs, int png_count, cp_atlas_image_t* imgs_out)
{
	float w0, h0, div, wTol, hTol;
	int atlas_image_size, atlas_stride, sp;
	void* atlas_pixels = 0;
	int atlas_node_capacity = png_count * 2;
	cp_image_t atlas_image;
	cp_integer_image_t* images = 0;
	cp_atlas_node_t* nodes = 0;

	atlas_image.w = atlas_width;
	atlas_image.h = atlas_height;
	atlas_image.pix = 0;

	CUTE_PNG_CHECK(pngs, "pngs array was NULL");
	CUTE_PNG_CHECK(imgs_out, "imgs_out array was NULL");

	images = (cp_integer_image_t*)CUTE_PNG_ALLOCA(sizeof(cp_integer_image_t) * png_count);
	nodes = (cp_atlas_node_t*)CUTE_PNG_ALLOC(sizeof(cp_atlas_node_t) * atlas_node_capacity);
	CUTE_PNG_CHECK(images, "out of mem");
	CUTE_PNG_CHECK(nodes, "out of mem");

	for (int i = 0; i < png_count; ++i)
	{
		const cp_image_t* png = pngs + i;
		cp_integer_image_t* image = images + i;
		image->fit = 0;
		image->size = cp_v2i(png->w, png->h);
		image->img_index = i;
	}

	// Sort PNGs from largest to smallest
	cp_qsort(images, png_count);

	// stack pointer, the stack is the nodes array which we will
	// allocate nodes from as necessary.
	sp = 1;

	nodes[0].min = cp_v2i(0, 0);
	nodes[0].max = cp_v2i(atlas_width, atlas_height);
	nodes[0].size = cp_v2i(atlas_width, atlas_height);

	// Nodes represent empty space in the atlas. Placing a texture into the
	// atlas involves splitting a node into two smaller pieces (or, if a
	// perfect fit is found, deleting the node).
	for (int i = 0; i < png_count; ++i)
	{
		cp_integer_image_t* image = images + i;
		const cp_image_t* png = pngs + image->img_index;
		int width = png->w;
		int height = png->h;
		cp_atlas_node_t *best_fit = cp_best_fit(sp, png, nodes);
		if (CUTE_PNG_ATLAS_MUST_FIT) CUTE_PNG_CHECK(best_fit, "Not enough room to place image in atlas.");
		else if (!best_fit) 
		{
			image->fit = 0;
			continue;
		}

		image->min = best_fit->min;
		image->max = cp_add(image->min, image->size);

		if (best_fit->size.x == width && best_fit->size.y == height)
		{
			cp_atlas_node_t* last_node = nodes + --sp;
			*best_fit = *last_node;
			image->fit = 1;

			continue;
		}

		image->fit = 1;

		if (sp == atlas_node_capacity)
		{
			int new_capacity = atlas_node_capacity * 2;
			cp_atlas_node_t* new_nodes = (cp_atlas_node_t*)CUTE_PNG_ALLOC(sizeof(cp_atlas_node_t) * new_capacity);
			CUTE_PNG_CHECK(new_nodes, "out of mem");
			memcpy(new_nodes, nodes, sizeof(cp_atlas_node_t) * sp);
			CUTE_PNG_FREE(nodes);
			// best_fit became a dangling pointer, so relocate it
			best_fit = new_nodes + (best_fit - nodes);
			nodes = new_nodes;
			atlas_node_capacity = new_capacity;
		}

		cp_atlas_node_t* new_node = nodes + sp++;
		new_node->min = best_fit->min;

		// Split bestFit along x or y, whichever minimizes
		// fragmentation of empty space
		cp_v2i_t d = cp_sub(best_fit->size, cp_v2i(width, height));
		if (d.x < d.y)
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

		new_node->max = cp_add(new_node->min, new_node->size);
	}

	// Write the final atlas image, use CUTE_PNG_ATLAS_EMPTY_COLOR as base color
	atlas_stride = atlas_width * sizeof(cp_pixel_t);
	atlas_image_size = atlas_width * atlas_height * sizeof(cp_pixel_t);
	atlas_pixels = CUTE_PNG_ALLOC(atlas_image_size);
	CUTE_PNG_CHECK(atlas_pixels, "out of mem");
	
	for(int i = 0; i < atlas_image_size; i += sizeof(cp_pixel_t)) {
		cp_write_pixel((char*)atlas_pixels + i, CUTE_PNG_ATLAS_EMPTY_COLOR);
	}

	for (int i = 0; i < png_count; ++i)
	{
		cp_integer_image_t* image = images + i;

		if (image->fit)
		{
			const cp_image_t* png = pngs + image->img_index;
			char* pixels = (char*)png->pix;
			cp_v2i_t min = image->min;
			cp_v2i_t max = image->max;
			int atlas_offset = min.x * sizeof(cp_pixel_t);
			int tex_stride = png->w * sizeof(cp_pixel_t);

			for (int row = min.y, y = 0; row < max.y; ++row, ++y)
			{
				void* row_ptr = (char*)atlas_pixels + (row * atlas_stride + atlas_offset);
				CUTE_PNG_MEMCPY(row_ptr, pixels + y * tex_stride, tex_stride);
			}
		}
	}

	atlas_image.pix = (cp_pixel_t*)atlas_pixels;

	// squeeze UVs inward by 128th of a pixel
	// this prevents atlas bleeding. tune as necessary for good results.
	w0 = 1.0f / (float)(atlas_width);
	h0 = 1.0f / (float)(atlas_height);
	div = 1.0f / 128.0f;
	wTol = w0 * div;
	hTol = h0 * div;

	for (int i = 0; i < png_count; ++i)
	{
		cp_integer_image_t* image = images + i;
		cp_atlas_image_t* img_out = imgs_out + i;

		img_out->img_index = image->img_index;
		img_out->w = image->size.x;
		img_out->h = image->size.y;
		img_out->fit = image->fit;

		if (image->fit)
		{
			cp_v2i_t min = image->min;
			cp_v2i_t max = image->max;

			float min_x = (float)min.x * w0 + wTol;
			float min_y = (float)min.y * h0 + hTol;
			float max_x = (float)max.x * w0 - wTol;
			float max_y = (float)max.y * h0 - hTol;

			// flip image on y axis
			if (CUTE_PNG_ATLAS_FLIP_Y_AXIS_FOR_UV)
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

	CUTE_PNG_FREE(nodes);
	return atlas_image;

cp_err:
	CUTE_PNG_FREE(atlas_pixels);
	CUTE_PNG_FREE(nodes);
	atlas_image.pix = 0;
	return atlas_image;
}

int cp_default_save_atlas(const char* out_path_image, const char* out_path_atlas_txt, const cp_image_t* atlas, const cp_atlas_image_t* imgs, int img_count, const char** names)
{
	FILE* fp = fopen(out_path_atlas_txt, "wt");
	CUTE_PNG_CHECK(fp, "unable to open out_path_atlas_txt in cp_default_save_atlas");

	fprintf(fp, "%s\n%d\n\n", out_path_image, img_count);

	for (int i = 0; i < img_count; ++i)
	{
		const cp_atlas_image_t* image = imgs + i;
		const char* name = names ? names[image->img_index] : 0;

		if (image->fit)
		{
			int width = image->w;
			int height = image->h;
			float min_x = image->minx;
			float min_y = image->miny;
			float max_x = image->maxx;
			float max_y = image->maxy;

			if (name) fprintf(fp, "{ \"%s\", w = %d, h = %d, u = { %.10f, %.10f }, v = { %.10f, %.10f } }\n", name, width, height, min_x, min_y, max_x, max_y);
			else fprintf(fp, "{ w = %d, h = %d, u = { %.10f, %.10f }, v = { %.10f, %.10f } }\n", width, height, min_x, min_y, max_x, max_y);
		}
	}

	// Save atlas image PNG to disk
	CUTE_PNG_CHECK(cp_save_png(out_path_image, atlas), "failed to save atlas image to disk");

cp_err:
	fclose(fp);
	return 0;
}

#endif // CUTE_PNG_IMPLEMENTATION_ONCE
#endif // CUTE_PNG_IMPLEMENTATION

/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2019 Randy Gaul http://www.randygaul.net
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
