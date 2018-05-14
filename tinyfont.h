/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	tinyfont.h - v1.00

	SUMMARY

		Loads up hand-crafted fonts with either ASCII-128, codepage 1252, or BMFont
		file formats. There are also some functions for getting text width/height
		in pixels, as well as a vertex buffer filling function. Kerning is supported
		via the BMFont format. All functions dealing with text accept utf8 string
		format.

	ASCII-128 - tinyfont_load_ascii

		The font image associated with the ASCII-128 format must have all 96 ascii
		glyphs defined. Each glyph in the image must be bordered by a "border color".
		The border color is defined as the first pixel in the image. The border should
		be an AABB that defines the glyph's quad.

	Codepage 1252 - tinyfont_load_1252

		The same as ASCII-128, except is supports 256-32 different glyphs.

	BMFont - tinyfont_load_bmfont

		Simply pass in a BMFont .fnt file to `tinyfont_load_bmfont`.

	Revision history:
		1.0  (05/13/2018) initial release
*/

#if !defined(TINYFONT_H)

#define TINYFONT_U64 unsigned long long

extern const char* tinyfont_error_reason;

typedef struct tinyfont_glyph_t
{
	float minx, miny;
	float maxx, maxy;
	float w, h;
	int xoffset, yoffset;
	int xadvance;
} tinyfont_glyph_t;

typedef struct tinyfont_t
{
	int font_height;
	int glyph_count;
	tinyfont_glyph_t* glyphs;
	int* codes;
	int atlas_w;
	int atlas_h;
	TINYFONT_U64 atlas_id;
	struct tinyfont_kern_t* kern;
	void* mem_ctx;
} tinyfont_t;

tinyfont_t* tinyfont_load_ascii(TINYFONT_U64 atlas_id, const void* pixels, int w, int h, int stride, void* mem_ctx);
tinyfont_t* tinyfont_load_1252(TINYFONT_U64 atlas_id, const void* pixels, int w, int h, int stride, void* mem_ctx);
tinyfont_t* tinyfont_load_bmfont(TINYFONT_U64 atlas_id, const void* fnt, int size, void* mem_ctx);
void tinyfont_free(tinyfont_t* font);

int tinyfont_text_width(tinyfont_t* font, const char* text);
int tinyfont_text_height(tinyfont_t* font, const char* text);

int tinyfont_get_glyph_index(tinyfont_t* font, int code); // returns run-time glyph index associated with a utf32 codepoint (unicode)
tinyfont_glyph_t* tinyfont_get_glyph(tinyfont_t* font, int index); // returns a glyph, given run-time glyph index
int tinyfont_kerning(tinyfont_t* font, int code0, int code1);

// Here just in case someone wants to load up a custom file format.
tinyfont_t* tinyfont_create_blank(int font_height, int glyph_count);
void tinyfont_add_kerning_pair(tinyfont_t* font, int code0, int code1, int kerning);

typedef struct tinyfont_vert_t
{
	float x, y;
	float u, v;
} tinyfont_vert_t;

// Fills in an array of triangles, two triangles for each quad, one quad for each text glyph.
// Will return 0 if the function tries to overrun the vertex buffer. Quads are setup in 2D where
// the y axis points up, x axis points left. The top left of the first glyph is placed at the
// coordinate {`x`, `y`}. Newlines move quads downward by the text height added with `line_height`.
// `count_written` contains the number of outputted vertices.
int tinyfont_fill_vertex_buffer(tinyfont_t* font, const char* text, float x, float y, float line_height, tinyfont_vert_t* buffer, int buffer_max, int* count_written);

#define TINYFONT_H
#endif

#if defined(TINYFONT_IMPLEMENTATION)
#if !defined(TINYFONT_IMPLEMENTATION_ONCE)
#define TINYFONT_IMPLEMENTATION_ONCE

#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
	#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#if !defined(TINYFONT_ALLOC)
	#include <stdlib.h>
	#define TINYFONT_ALLOC(size, ctx) malloc(size)
	#define TINYFONT_FREE(mem, ctx) free(mem)
#endif

#if !defined(TINYFONT_MEMSET)
	#include <string.h>
	#define TINYFONT_MEMSET memset
#endif

#if !defined(TINYFONT_MEMCPY)
	#include <string.h>
	#define TINYFONT_MEMCPY memcpy
#endif

#if !defined(TINYFONT_STRNCMP)
	#include <string.h>
	#define TINYFONT_STRNCMP strncmp
#endif

#if !defined(TINYFONT_STRTOLL)
	#include <stdlib.h>
	#define TINYFONT_STRTOLL strtoll
#endif

#if !defined(TINYFONT_STRTOD)
	#include <stdlib.h>
	#define TINYFONT_STRTOD strtod
#endif


#define HASHTABLE_MEMSET(ptr, val, n) TINYFONT_MEMSET(ptr, val, n)
#define HASHTABLE_MEMCPY(dst, src, n) TINYFONT_MEMCPY(dst, src, n)
#define HASHTABLE_MALLOC(ctx, size) TINYFONT_ALLOC(size, ctx)
#define HASHTABLE_FREE(ctx, ptr) TINYFONT_FREE(ptr, ctx)
#define HASHTABLE_U64 TINYFONT_U64


// hashtable.h implementation by Mattias Gustavsson
// See: http://www.mattiasgustavsson.com/ and https://github.com/mattiasgustavsson/libs/blob/master/hashtable.h
// begin hashtable.h

/*
------------------------------------------------------------------------------
          Licensing information can be found at the end of the file.
------------------------------------------------------------------------------
hashtable.h - v1.1 - Cache efficient hash table implementation for C/C++.
Do this:
    #define HASHTABLE_IMPLEMENTATION
before you include this file in *one* C/C++ file to create the implementation.
*/

#ifndef hashtable_h
#define hashtable_h

#ifndef HASHTABLE_U64
    #define HASHTABLE_U64 unsigned long long
#endif

typedef struct hashtable_t hashtable_t;

void hashtable_init( hashtable_t* table, int item_size, int initial_capacity, void* memctx );
void hashtable_term( hashtable_t* table );

void* hashtable_insert( hashtable_t* table, HASHTABLE_U64 key, void const* item );
void hashtable_remove( hashtable_t* table, HASHTABLE_U64 key );
void hashtable_clear( hashtable_t* table );

void* hashtable_find( hashtable_t const* table, HASHTABLE_U64 key );

int hashtable_count( hashtable_t const* table );
void* hashtable_items( hashtable_t const* table );
HASHTABLE_U64 const* hashtable_keys( hashtable_t const* table );

void hashtable_swap( hashtable_t* table, int index_a, int index_b );


#endif /* hashtable_h */

/*
----------------------
    IMPLEMENTATION
----------------------
*/

#ifndef hashtable_t_h
#define hashtable_t_h

#ifndef HASHTABLE_U32
    #define HASHTABLE_U32 unsigned int
#endif

struct hashtable_internal_slot_t
    {
    HASHTABLE_U32 key_hash;
    int item_index;
    int base_count;
    };

struct hashtable_t
    {
    void* memctx;
    int count;
    int item_size;

    struct hashtable_internal_slot_t* slots;
    int slot_capacity;

    HASHTABLE_U64* items_key;
    int* items_slot;
    void* items_data;
    int item_capacity;

    void* swap_temp;
    };

#endif /* hashtable_t_h */

#define HASHTABLE_IMPLEMENTATION

#ifdef HASHTABLE_IMPLEMENTATION
#ifndef HASHTABLE_IMPLEMENTATION_ONCE
#define HASHTABLE_IMPLEMENTATION_ONCE

#ifndef HASHTABLE_SIZE_T
    #include <stddef.h>
    #define HASHTABLE_SIZE_T size_t
#endif

#ifndef HASHTABLE_ASSERT
    #include <assert.h>
    #define HASHTABLE_ASSERT( x ) assert( x )
#endif

#ifndef HASHTABLE_MEMSET
    #include <string.h>
    #define HASHTABLE_MEMSET( ptr, val, cnt ) ( memset( ptr, val, cnt ) )
#endif

#ifndef HASHTABLE_MEMCPY
    #include <string.h>
    #define HASHTABLE_MEMCPY( dst, src, cnt ) ( memcpy( dst, src, cnt ) )
#endif

#ifndef HASHTABLE_MALLOC
    #include <stdlib.h>
    #define HASHTABLE_MALLOC( ctx, size ) ( malloc( size ) )
    #define HASHTABLE_FREE( ctx, ptr ) ( free( ptr ) )
#endif


static HASHTABLE_U32 hashtable_internal_pow2ceil( HASHTABLE_U32 v )
    {
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    ++v;
    v += ( v == 0 );
    return v;
    }


void hashtable_init( hashtable_t* table, int item_size, int initial_capacity, void* memctx )
    {
    initial_capacity = (int)hashtable_internal_pow2ceil( initial_capacity >=0 ? (HASHTABLE_U32) initial_capacity : 32U );
    table->memctx = memctx;
    table->count = 0;
    table->item_size = item_size;
    table->slot_capacity = (int) hashtable_internal_pow2ceil( (HASHTABLE_U32) ( initial_capacity + initial_capacity / 2 ) );
    int slots_size = (int)( table->slot_capacity * sizeof( *table->slots ) );
    table->slots = (struct hashtable_internal_slot_t*) HASHTABLE_MALLOC( table->memctx, (HASHTABLE_SIZE_T) slots_size );
    HASHTABLE_ASSERT( table->slots );
    HASHTABLE_MEMSET( table->slots, 0, (HASHTABLE_SIZE_T) slots_size );
    table->item_capacity = (int) hashtable_internal_pow2ceil( (HASHTABLE_U32) initial_capacity );
    table->items_key = (HASHTABLE_U64*) HASHTABLE_MALLOC( table->memctx,
        table->item_capacity * ( sizeof( *table->items_key ) + sizeof( *table->items_slot ) + table->item_size ) + table->item_size );
    HASHTABLE_ASSERT( table->items_key );
    table->items_slot = (int*)( table->items_key + table->item_capacity );
    table->items_data = (void*)( table->items_slot + table->item_capacity );
    table->swap_temp = (void*)( ( (uintptr_t) table->items_data ) + table->item_size * table->item_capacity ); 
    }


void hashtable_term( hashtable_t* table )
    {
    HASHTABLE_FREE( table->memctx, table->items_key );
    HASHTABLE_FREE( table->memctx, table->slots );
    }


// from https://gist.github.com/badboy/6267743
static HASHTABLE_U32 hashtable_internal_calculate_hash( HASHTABLE_U64 key )
    {
    key = ( ~key ) + ( key << 18 );
    key = key ^ ( key >> 31 );
    key = key * 21;
    key = key ^ ( key >> 11 );
    key = key + ( key << 6 );
    key = key ^ ( key >> 22 );  
    HASHTABLE_ASSERT( key );
    return (HASHTABLE_U32) key;
    }


static int hashtable_internal_find_slot( hashtable_t const* table, HASHTABLE_U64 key )
    {
    int const slot_mask = table->slot_capacity - 1;
    HASHTABLE_U32 const hash = hashtable_internal_calculate_hash( key );

    int const base_slot = (int)( hash & (HASHTABLE_U32)slot_mask );
    int base_count = table->slots[ base_slot ].base_count;
    int slot = base_slot;

    while( base_count > 0 )
        {
        HASHTABLE_U32 slot_hash = table->slots[ slot ].key_hash;
        if( slot_hash )
            {
            int slot_base = (int)( slot_hash & (HASHTABLE_U32)slot_mask );
            if( slot_base == base_slot ) 
                {
                HASHTABLE_ASSERT( base_count > 0 );
                --base_count;
                if( slot_hash == hash && table->items_key[ table->slots[ slot ].item_index ] == key )
                    return slot;
                }
            }
        slot = ( slot + 1 ) & slot_mask;
        }   

    return -1;
    }


static void hashtable_internal_expand_slots( hashtable_t* table )
    {
    int const old_capacity = table->slot_capacity;
    struct hashtable_internal_slot_t* old_slots = table->slots;

    table->slot_capacity *= 2;
    int const slot_mask = table->slot_capacity - 1;

    int const size = (int)( table->slot_capacity * sizeof( *table->slots ) );
    table->slots = (struct hashtable_internal_slot_t*) HASHTABLE_MALLOC( table->memctx, (HASHTABLE_SIZE_T) size );
    HASHTABLE_ASSERT( table->slots );
    HASHTABLE_MEMSET( table->slots, 0, (HASHTABLE_SIZE_T) size );

    for( int i = 0; i < old_capacity; ++i )
        {
        HASHTABLE_U32 const hash = old_slots[ i ].key_hash;
        if( hash )
            {
            int const base_slot = (int)( hash & (HASHTABLE_U32)slot_mask );
            int slot = base_slot;
            while( table->slots[ slot ].key_hash )
                slot = ( slot + 1 ) & slot_mask;
            table->slots[ slot ].key_hash = hash;
            int item_index = old_slots[ i ].item_index;
            table->slots[ slot ].item_index = item_index;
            table->items_slot[ item_index ] = slot; 
            ++table->slots[ base_slot ].base_count;
            }               
        }

    HASHTABLE_FREE( table->memctx, old_slots );
    }


static void hashtable_internal_expand_items( hashtable_t* table )
    {
    table->item_capacity *= 2;
     HASHTABLE_U64* const new_items_key = (HASHTABLE_U64*) HASHTABLE_MALLOC( table->memctx, 
         table->item_capacity * ( sizeof( *table->items_key ) + sizeof( *table->items_slot ) + table->item_size ) + table->item_size);
    HASHTABLE_ASSERT( new_items_key );

    int* const new_items_slot = (int*)( new_items_key + table->item_capacity );
    void* const new_items_data = (void*)( new_items_slot + table->item_capacity );
    void* const new_swap_temp = (void*)( ( (uintptr_t) new_items_data ) + table->item_size * table->item_capacity ); 

    HASHTABLE_MEMCPY( new_items_key, table->items_key, table->count * sizeof( *table->items_key ) );
    HASHTABLE_MEMCPY( new_items_slot, table->items_slot, table->count * sizeof( *table->items_key ) );
    HASHTABLE_MEMCPY( new_items_data, table->items_data, (HASHTABLE_SIZE_T) table->count * table->item_size );
    
    HASHTABLE_FREE( table->memctx, table->items_key );

    table->items_key = new_items_key;
    table->items_slot = new_items_slot;
    table->items_data = new_items_data;
    table->swap_temp = new_swap_temp;
    }


void* hashtable_insert( hashtable_t* table, HASHTABLE_U64 key, void const* item )
    {
    HASHTABLE_ASSERT( hashtable_internal_find_slot( table, key ) < 0 );

    if( table->count >= ( table->slot_capacity - table->slot_capacity / 3 ) )
        hashtable_internal_expand_slots( table );
        
    int const slot_mask = table->slot_capacity - 1;
    HASHTABLE_U32 const hash = hashtable_internal_calculate_hash( key );

    int const base_slot = (int)( hash & (HASHTABLE_U32)slot_mask );
    int base_count = table->slots[ base_slot ].base_count;
    int slot = base_slot;
    int first_free = slot;
    while( base_count )
        {
        HASHTABLE_U32 const slot_hash = table->slots[ slot ].key_hash;
        if( slot_hash == 0 && table->slots[ first_free ].key_hash != 0 ) first_free = slot;
        int slot_base = (int)( slot_hash & (HASHTABLE_U32)slot_mask );
        if( slot_base == base_slot ) 
            --base_count;
        slot = ( slot + 1 ) & slot_mask;
        }       

    slot = first_free;
    while( table->slots[ slot ].key_hash )
        slot = ( slot + 1 ) & slot_mask;

    if( table->count >= table->item_capacity )
        hashtable_internal_expand_items( table );

    HASHTABLE_ASSERT( !table->slots[ slot ].key_hash && ( hash & (HASHTABLE_U32) slot_mask ) == (HASHTABLE_U32) base_slot );
    HASHTABLE_ASSERT( hash );
    table->slots[ slot ].key_hash = hash;
    table->slots[ slot ].item_index = table->count;
    ++table->slots[ base_slot ].base_count;


    void* dest_item = (void*)( ( (uintptr_t) table->items_data ) + table->count * table->item_size );
    memcpy( dest_item, item, (HASHTABLE_SIZE_T) table->item_size );
    table->items_key[ table->count ] = key;
    table->items_slot[ table->count ] = slot;
    ++table->count;
    return dest_item;
    } 


void hashtable_remove( hashtable_t* table, HASHTABLE_U64 key )
    {
    int const slot = hashtable_internal_find_slot( table, key );
    HASHTABLE_ASSERT( slot >= 0 );

    int const slot_mask = table->slot_capacity - 1;
    HASHTABLE_U32 const hash = table->slots[ slot ].key_hash;
    int const base_slot = (int)( hash & (HASHTABLE_U32) slot_mask );
    HASHTABLE_ASSERT( hash );
    --table->slots[ base_slot ].base_count;
    table->slots[ slot ].key_hash = 0;

    int index = table->slots[ slot ].item_index;
    int last_index = table->count - 1;
    if( index != last_index )
        {
        table->items_key[ index ] = table->items_key[ last_index ];
        table->items_slot[ index ] = table->items_slot[ last_index ];
        void* dst_item = (void*)( ( (uintptr_t) table->items_data ) + index * table->item_size );
        void* src_item = (void*)( ( (uintptr_t) table->items_data ) + last_index * table->item_size );
        HASHTABLE_MEMCPY( dst_item, src_item, (HASHTABLE_SIZE_T) table->item_size );
        table->slots[ table->items_slot[ last_index ] ].item_index = index;
        }
    --table->count;
    } 


void hashtable_clear( hashtable_t* table )
    {
    table->count = 0;
    HASHTABLE_MEMSET( table->slots, 0, table->slot_capacity * sizeof( *table->slots ) );
    }


void* hashtable_find( hashtable_t const* table, HASHTABLE_U64 key )
    {
    int const slot = hashtable_internal_find_slot( table, key );
    if( slot < 0 ) return 0;

    int const index = table->slots[ slot ].item_index;
    void* const item = (void*)( ( (uintptr_t) table->items_data ) + index * table->item_size );
    return item;
    }


int hashtable_count( hashtable_t const* table )
    {
    return table->count;
    }


void* hashtable_items( hashtable_t const* table )
    {
    return table->items_data;
    }


HASHTABLE_U64 const* hashtable_keys( hashtable_t const* table )
    {
    return table->items_key;
    }


void hashtable_swap( hashtable_t* table, int index_a, int index_b )
    {
    if( index_a < 0 || index_a >= table->count || index_b < 0 || index_b >= table->count ) return;

    int slot_a = table->items_slot[ index_a ];
    int slot_b = table->items_slot[ index_b ];

    table->items_slot[ index_a ] = slot_b;
    table->items_slot[ index_b ] = slot_a;

    HASHTABLE_U64 temp_key = table->items_key[ index_a ];
    table->items_key[ index_a ] = table->items_key[ index_b ];
    table->items_key[ index_b ] = temp_key;

    void* item_a = (void*)( ( (uintptr_t) table->items_data ) + index_a * table->item_size );
    void* item_b = (void*)( ( (uintptr_t) table->items_data ) + index_b * table->item_size );
    HASHTABLE_MEMCPY( table->swap_temp, item_a, table->item_size );
    HASHTABLE_MEMCPY( item_a, item_b, table->item_size );
    HASHTABLE_MEMCPY( item_b, table->swap_temp, table->item_size );

    table->slots[ slot_a ].item_index = index_b;
    table->slots[ slot_b ].item_index = index_a;
    }


#endif /* HASHTABLE_IMPLEMENTATION */
#endif // HASHTABLE_IMPLEMENTATION_ONCE

/*
contributors:
    Randy Gaul (hashtable_clear, hashtable_swap )
revision history:
    1.1     added hashtable_clear, hashtable_swap
    1.0     first released version  
*/

/*
------------------------------------------------------------------------------
This software is available under 2 licenses - you may choose the one you like.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2015 Mattias Gustavsson
Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.
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

// end of hashtable.h


const char* tinyfont_error_reason;

// cp1252 table and decode utf8 functions by Mitton a la TIGR
// https://bitbucket.org/rmitton/tigr/src/default/

// Converts 8-bit codepage entries into unicode code points for indices of 128-256
static int tinyfont_cp1252[] = {
	0x20ac,0xfffd,0x201a,0x0192,0x201e,0x2026,0x2020,0x2021,0x02c6,0x2030,0x0160,0x2039,0x0152,0xfffd,0x017d,0xfffd,
	0xfffd,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x02dc,0x2122,0x0161,0x203a,0x0153,0xfffd,0x017e,0x0178,
	0x00a0,0x00a1,0x00a2,0x00a3,0x00a4,0x00a5,0x00a6,0x00a7,0x00a8,0x00a9,0x00aa,0x00ab,0x00ac,0x00ad,0x00ae,0x00af,
	0x00b0,0x00b1,0x00b2,0x00b3,0x00b4,0x00b5,0x00b6,0x00b7,0x00b8,0x00b9,0x00ba,0x00bb,0x00bc,0x00bd,0x00be,0x00bf,
	0x00c0,0x00c1,0x00c2,0x00c3,0x00c4,0x00c5,0x00c6,0x00c7,0x00c8,0x00c9,0x00ca,0x00cb,0x00cc,0x00cd,0x00ce,0x00cf,
	0x00d0,0x00d1,0x00d2,0x00d3,0x00d4,0x00d5,0x00d6,0x00d7,0x00d8,0x00d9,0x00da,0x00db,0x00dc,0x00dd,0x00de,0x00df,
	0x00e0,0x00e1,0x00e2,0x00e3,0x00e4,0x00e5,0x00e6,0x00e7,0x00e8,0x00e9,0x00ea,0x00eb,0x00ec,0x00ed,0x00ee,0x00ef,
	0x00f0,0x00f1,0x00f2,0x00f3,0x00f4,0x00f5,0x00f6,0x00f7,0x00f8,0x00f9,0x00fa,0x00fb,0x00fc,0x00fd,0x00fe,0x00ff,
};

const char* tinyfont_decode_utf8(const char* text, int* cp)
{
	unsigned char c = *text++;
	int extra = 0, min = 0;
	*cp = 0;
	     if (c >= 0xF0) { *cp = c & 0x07; extra = 3; min = 0x10000; }
	else if (c >= 0xE0) { *cp = c & 0x0F; extra = 2; min = 0x800; }
	else if (c >= 0xC0) { *cp = c & 0x1F; extra = 1; min = 0x80; }
	else if (c >= 0x80) { *cp = 0xFFFD; }
	else *cp = c;
	while (extra--)
	{
		c = *text++;
		if ((c & 0xC0) != 0x80) { *cp = 0xFFFD; break; }
		(*cp) = ((*cp) << 6) | (c & 0x3F);
	}
	if (*cp < min) *cp = 0xFFFD;
	return text;
}

typedef struct tinyfont_img_t
{
	void* pix;
	int w, h;
	int stride;
} tinyfont_img_t;

static const char* tinyfont_get_pixel(tinyfont_img_t* img, int x, int y)
{
	return ((const char*)img->pix) + y * img->w * img->stride + x * img->stride;
}

static int tinyfont_is_border(tinyfont_img_t* img, int x, int y)
{
	const char* border_color = (const char*)img->pix;
	const char* pixel = tinyfont_get_pixel(img, x, y);
	for (int i = 0; i < img->stride; ++i) if (pixel[i] != border_color[i]) return 0;
	return 1;
}

static void tinyfont_scan(tinyfont_img_t* img, int *x, int *y, int *row_height)
{
	while (*y < img->h)
	{
		if (*x >= img->w)
		{
			*x = 0;
			(*y) += *row_height;
			*row_height = 1;
		}
		if (!tinyfont_is_border(img, *x, *y)) return;
		(*x)++;
	}
}

#define TINYFONT_CHECK(X, Y) do { if (!(X)) { tinyfont_error_reason = Y; goto tinyfont_err; } } while (0)
#define TINYFONT_FAIL_IF(X) do { if (X) { goto tinyfont_err; } } while (0)

tinyfont_t* tinyfont_load(TINYFONT_U64 atlas_id, const void* pixels, int w, int h, int stride, void* mem_ctx, int codepage)
{
	// algorithm by Mitton a la TIGR
	// https://bitbucket.org/rmitton/tigr/src/default/
	tinyfont_t* font = (tinyfont_t*)TINYFONT_ALLOC(sizeof(tinyfont_t), mem_ctx);
	font->codes = 0; font->glyphs = 0;
	font->mem_ctx = mem_ctx;
	font->atlas_w = w;
	font->atlas_h = h;
	tinyfont_img_t img;
	img.pix = (void*)pixels;
	img.w = w;
	img.h = h;
	img.stride = stride;

	switch (codepage)
	{
	case 0:    font->glyph_count = 128 - 32; break;
	case 1252: font->glyph_count = 256 - 32; break;
	default: TINYFONT_CHECK(0, "Unknown codepage encountered.");
	}
	font->codes = (int*)TINYFONT_ALLOC(sizeof(int) * font->glyph_count, mem_ctx);
	font->glyphs = (tinyfont_glyph_t*)TINYFONT_ALLOC(sizeof(tinyfont_glyph_t) * font->glyph_count, mem_ctx);
	font->atlas_id = atlas_id;
	font->kern = 0;

	// Used to squeeze UVs inward by 128th of a pixel.
	float w0 = 1.0f / (float)(font->atlas_w);
	float h0 = 1.0f / (float)(font->atlas_h);
	float div = 1.0f / 128.0f;
	float wTol = w0 * div;
	float hTol = h0 * div;

	int font_height = 1;
	int x = 0, y = 0;
	for (int i = 32; i < font->glyph_count + 32; ++i)
	{
		tinyfont_scan(&img, &x, &y, &font_height);
		TINYFONT_CHECK(y < img.w, "Unable to properly scan glyph width. Are the text borders drawn properly?");

		int w = 0, h = 0;
		while (!tinyfont_is_border(&img, x + w, y)) ++w;
		while (!tinyfont_is_border(&img, x, y + h)) ++h;

		tinyfont_glyph_t* glyph = font->glyphs + i - 32;
		if (i < 128) font->codes[i - 32] = i;
		else if (codepage == 1252) font->codes[i - 32] = tinyfont_cp1252[i - 128];
		else TINYFONT_CHECK(0, "Unknown glyph index found.");

		glyph->xadvance = w + 1;
		glyph->w = (float)w;
		glyph->h = (float)h;
		glyph->minx = x * w0 + wTol;
		glyph->maxx = (x + w) * w0 - wTol;
		glyph->miny = y * h0 + wTol;
		glyph->maxy = (y + h) * h0 - wTol;
		glyph->xoffset = 0;
		glyph->yoffset = 0;

		if (h > font_height) font_height = h;
		x += w;
	}

	font->font_height = font_height;

	// sort by codepoint for non-ascii code pages
	if (codepage)
	{
		for (int i = 1; i < font->glyph_count; ++i)
		{
			tinyfont_glyph_t glyph = font->glyphs[i];
			int code = font->codes[i];
			int j = i;
			while (j > 0 && font->codes[j - 1] > code)
			{
				font->glyphs[j] = font->glyphs[j - 1];
				font->codes[j] = font->codes[j - 1];
				--j;
			}
			font->glyphs[j] = glyph;
			font->codes[j] = code;
		}
	}

	return font;

tinyfont_err:
	TINYFONT_FREE(font->glyphs, mem_ctx);
	TINYFONT_FREE(font->codes, mem_ctx);
	TINYFONT_FREE(font, mem_ctx);
	return 0;
}


tinyfont_t* tinyfont_load_ascii(TINYFONT_U64 atlas_id, const void* pixels, int w, int h, int stride, void* mem_ctx)
{
	return tinyfont_load(atlas_id, pixels, w, h, stride, mem_ctx, 0);
}

tinyfont_t* tinyfont_load_1252(TINYFONT_U64 atlas_id, const void* pixels, int w, int h, int stride, void* mem_ctx)
{
	return tinyfont_load(atlas_id, pixels, w, h, stride, mem_ctx, 1252);
}

#define TINYFONT_INTERNAL_BUFFER_MAX 1024

typedef struct tinyfont_parse_t
{
	const char* in;
	const char* end;
	int scratch_len;
	char scratch[TINYFONT_INTERNAL_BUFFER_MAX];
} tinyfont_parse_t;

static int tinyfont_isspace(char c)
{
	return (c == ' ') |
		(c == '\t') |
		(c == '\n') |
		(c == '\v') |
		(c == '\f') |
		(c == '\r');
}

static int tinyfont_next_internal(tinyfont_parse_t* p, char* c)
{
	TINYFONT_CHECK(p->in < p->end, "Attempted to read past input buffer.");
	while (tinyfont_isspace(*c = *p->in++)) TINYFONT_CHECK(p->in < p->end, "Attempted to read past input buffer.");
	return 1;

tinyfont_err:
	return 0;
}

#define tinyfont_next(p, c) \
	do { \
		TINYFONT_FAIL_IF(!tinyfont_next_internal(p, c)); \
	} while (0)

static char tinyfont_parse_char(char c)
{
	switch (c)
	{
		case '\\': return '\\';
		case '\'': return '\'';
		case '"': return '"';
		case 't': return '\t';
		case 'f': return '\f';
		case 'n': return '\n';
		case 'r': return '\r';
		case '0': return '\0';
		default: return c;
	}
}

#define tinyfont_expect(p, expect) \
	do { \
		char tinyfont_char; \
		tinyfont_next(p, &tinyfont_char); \
		TINYFONT_CHECK(tinyfont_char == expect, "Found unexpected token."); \
	} while (0)

static int tinyfont_read_string_internal(tinyfont_parse_t* p)
{
	int count = 0;
	int done = 0;
	tinyfont_expect(p, '"');

	while (!done)
	{
		TINYFONT_CHECK(count < TINYFONT_INTERNAL_BUFFER_MAX, "String too large to parse.");
		char c;
		tinyfont_next(p, &c);

		switch (c)
		{
		case '"':
			p->scratch[count] = 0;
			done = 1;
			break;

		case '\\':
		{
			char the_char;
			tinyfont_next(p, &the_char);
			the_char = tinyfont_parse_char(the_char);
			p->scratch[count++] = the_char;
		}	break;

		default:
			p->scratch[count++] = c;
			break;
		}
	}

	p->scratch_len = count;
	return 1;

tinyfont_err:
	return 0;
}

#define tinyfont_read_string(p) \
	do { \
		TINYFONT_FAIL_IF(!tinyfont_read_string_internal(p)); \
	} while (0)

static int tinyfont_read_identifier_internal(tinyfont_parse_t* p)
{
	int count = 0;
	int done = 0;

	while (1)
	{
		TINYFONT_CHECK(p->in < p->end, "Attempted to read past input buffer.");
		TINYFONT_CHECK(count < TINYFONT_INTERNAL_BUFFER_MAX, "String too large to parse.");
		char c;
		c = *p->in;
		if (!tinyfont_isspace(c)) break;
		p->in++;
	}

	while (!done)
	{
		TINYFONT_CHECK(p->in < p->end, "Attempted to read past input buffer.");
		TINYFONT_CHECK(count < TINYFONT_INTERNAL_BUFFER_MAX, "String too large to parse.");
		char c;
		c = *p->in++;

		if (tinyfont_isspace(c))
		{
			p->scratch[count] = 0;
			break;
		}

		switch (c)
		{
		case '=':
			p->scratch[count] = 0;
			done = 1;
			break;

		case '\\':
		{
			char the_char;
			tinyfont_next(p, &the_char);
			the_char = tinyfont_parse_char(the_char);
			p->scratch[count++] = the_char;
		}	break;

		default:
			p->scratch[count++] = c;
			break;
		}
	}

	p->scratch_len = count;
	return 1;

tinyfont_err:
	return 0;
}

#define tinyfont_read_identifier(p) \
	do { \
		TINYFONT_FAIL_IF(!tinyfont_read_identifier_internal(p)); \
	} while (0)

static int tinyfont_read_int_internal(tinyfont_parse_t* p, int* out)
{
	char* end;
	int val = (int)TINYFONT_STRTOLL(p->in, &end, 10);
	TINYFONT_CHECK(p->in != end, "Invalid integer found during parse.");
	p->in = end;
	*out = val;
	return 1;

tinyfont_err:
	return 0;
}

#define tinyfont_read_int(p, num) \
	do { \
		TINYFONT_FAIL_IF(!tinyfont_read_int_internal(p, num)); \
	} while (0)

static int tinyfont_read_float_internal(tinyfont_parse_t* p, float* out)
{
	char* end;
	float val = (float)TINYFONT_STRTOD(p->in, &end);
	TINYFONT_CHECK(p->in != end, "Error reading float.");
	p->in = end;
	*out = val;
	return 1;

tinyfont_err:
	return 0;
}

#define tinyfont_read_float(p, num) \
	do { \
		TINYFONT_FAIL_IF(tinyfont_read_float_internal(p, num) != TINYFONT_SUCCESS); \
	} while (0)

int tinyfont_expect_string_internal(tinyfont_parse_t* p, const char* str)
{
	tinyfont_read_string(p);
	if (TINYFONT_STRNCMP(p->scratch, str, p->scratch_len)) return 0;
	else return 1;
tinyfont_err:
	return 0;
}

#define tinyfont_expect_string(p, str) \
	do { \
		TINYFONT_FAIL_IF(!tinyfont_expect_string_internal(p, str)); \
	} while (0)

int tinyfont_expect_identifier_internal(tinyfont_parse_t* p, const char* str)
{
	tinyfont_read_identifier(p);
	if (TINYFONT_STRNCMP(p->scratch, str, p->scratch_len)) return 0;
	else return 1;
tinyfont_err:
	return 0;
}

#define tinyfont_expect_identifier(p, str) \
	do { \
		TINYFONT_FAIL_IF(!tinyfont_expect_identifier_internal(p, str)); \
	} while (0)

typedef struct tinyfont_kern_t
{
	hashtable_t table;
} tinyfont_kern_t;

tinyfont_t* tinyfont_load_bmfont(TINYFONT_U64 atlas_id, const void* fnt, int size, void* mem_ctx)
{
	tinyfont_parse_t parse;
	tinyfont_parse_t* p = &parse;
	p->in = (const char*)fnt;
	p->end = p->in + size;

	tinyfont_t* font = (tinyfont_t*)TINYFONT_ALLOC(sizeof(tinyfont_t), mem_ctx);
	font->atlas_id = atlas_id;
	font->kern = 0;
	font->mem_ctx = mem_ctx;

	// Read in font information.
	tinyfont_expect_identifier(p, "info");
	tinyfont_expect_identifier(p, "face");
	tinyfont_read_string(p);
	tinyfont_expect_identifier(p, "size");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "bold");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "italic");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "charset");
	tinyfont_read_string(p);
	tinyfont_expect_identifier(p, "unicode");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "stretchH");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "smooth");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "aa");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "padding");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "spacing");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "outline");
	tinyfont_read_identifier(p);

	tinyfont_expect_identifier(p, "common");
	tinyfont_expect_identifier(p, "lineHeight");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "base");
	tinyfont_read_int(p, &font->font_height);
	tinyfont_expect_identifier(p, "scaleW");
	tinyfont_read_int(p, &font->atlas_w);
	tinyfont_expect_identifier(p, "scaleH");
	tinyfont_read_int(p, &font->atlas_h);
	tinyfont_expect_identifier(p, "pages");
	tinyfont_expect_identifier(p, "1");
	tinyfont_expect_identifier(p, "packed");
	tinyfont_expect_identifier(p, "0");
	tinyfont_expect_identifier(p, "alphaChnl");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "redChnl");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "greenChnl");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "blueChnl");
	tinyfont_read_identifier(p);

	tinyfont_expect_identifier(p, "page");
	tinyfont_expect_identifier(p, "id");
	tinyfont_read_identifier(p);
	tinyfont_expect_identifier(p, "file");
	tinyfont_read_string(p);

	// Start parsing individual glyphs.
	tinyfont_expect_identifier(p, "chars");
	tinyfont_expect_identifier(p, "count");
	tinyfont_read_int(p, &font->glyph_count);
	font->glyphs = (tinyfont_glyph_t*)TINYFONT_ALLOC(sizeof(tinyfont_glyph_t) * font->glyph_count, mem_ctx);
	font->codes = (int*)TINYFONT_ALLOC(sizeof(int) * font->glyph_count, mem_ctx);

	// Used to squeeze UVs inward by 128th of a pixel.
	float w0 = 1.0f / (float)(font->atlas_w);
	float h0 = 1.0f / (float)(font->atlas_h);
	float div = 1.0f / 128.0f;
	float wTol = w0 * div;
	float hTol = h0 * div;

	// Read in each glyph.
	for (int i = 0; i < font->glyph_count; ++i)
	{
		tinyfont_glyph_t* glyph = font->glyphs + i;
		tinyfont_expect_identifier(p, "char");
		tinyfont_expect_identifier(p, "id");
		tinyfont_read_int(p, font->codes + i);

		int x, y;
		int width, height;
		tinyfont_expect_identifier(p, "x");
		tinyfont_read_int(p, &x);
		tinyfont_expect_identifier(p, "y");
		tinyfont_read_int(p, &y);
		tinyfont_expect_identifier(p, "width");
		tinyfont_read_int(p, &width);
		tinyfont_expect_identifier(p, "height");
		tinyfont_read_int(p, &height);

		glyph->w = (float)width;
		glyph->h = (float)height;
		glyph->minx = (float)x * w0 + wTol;
		glyph->miny = (float)y * h0 + hTol;
		glyph->maxx = (float)(x + width)  * w0 - wTol;
		glyph->maxy = (float)(y + height) * h0 - hTol;

		tinyfont_expect_identifier(p, "xoffset");
		tinyfont_read_int(p, &glyph->xoffset);
		tinyfont_expect_identifier(p, "yoffset");
		tinyfont_read_int(p, &glyph->yoffset);
		tinyfont_expect_identifier(p, "xadvance");
		tinyfont_read_int(p, &glyph->xadvance);
		tinyfont_expect_identifier(p, "page");
		tinyfont_read_identifier(p);
		tinyfont_expect_identifier(p, "chnl");
		tinyfont_read_identifier(p);
	}

	if (p->end - p->in > 8)
	{
		int kern_count;
		tinyfont_expect_identifier(p, "kernings");
		tinyfont_expect_identifier(p, "count");
		tinyfont_read_int(p, &kern_count);
		tinyfont_kern_t* kern = (tinyfont_kern_t*)TINYFONT_ALLOC(sizeof(tinyfont_kern_t), mem_ctx);
		font->kern = kern;
		hashtable_init(&kern->table, sizeof(int), kern_count, mem_ctx);

		for (int i = 0; i < kern_count; ++i)
		{
			int first, second, amount;
			tinyfont_expect_identifier(p, "kerning");
			tinyfont_expect_identifier(p, "first");
			tinyfont_read_int(p, &first);
			tinyfont_expect_identifier(p, "second");
			tinyfont_read_int(p, &second);
			tinyfont_expect_identifier(p, "amount");
			tinyfont_read_int(p, &amount);

			TINYFONT_U64 key = (TINYFONT_U64)first << 32 | (TINYFONT_U64)second;
			hashtable_insert(&kern->table, key, (void*)(size_t)amount);
		}
	}

	return font;

	tinyfont_err:
	tinyfont_free(font);
	return 0;
}

void tinyfont_free(tinyfont_t* font)
{
	void* mem_ctx = font->mem_ctx;
	TINYFONT_FREE(font->glyphs, mem_ctx);
	TINYFONT_FREE(font->codes, mem_ctx);
	if (font->kern) hashtable_term(&font->kern->table);
	TINYFONT_FREE(font->kern, mem_ctx);
	TINYFONT_FREE(font, mem_ctx);
}

int tinyfont_text_width(tinyfont_t* font, const char* text)
{
	int x = 0;
	int w = 0;

	while (*text)
	{
		int c;
		text = tinyfont_decode_utf8(text, &c);
		if (c == '\n' || c == '\r') x = 0;
		else
		{
			x += tinyfont_get_glyph(font, tinyfont_get_glyph_index(font, c))->xadvance;
			w = (x > w) ? x : w;
		}
	}

	return w;
}

int tinyfont_text_height(tinyfont_t* font, const char* text)
{
	int font_height, h;
	h = font_height = font->font_height;

	while (*text)
	{
		int c;
		text = tinyfont_decode_utf8(text, &c);
		if (c == '\n' && *text) h += font_height; 
	}

	return h;
}

int tinyfont_get_glyph_index(tinyfont_t* font, int code)
{
	int lo = 0;
	int hi = font->glyph_count;

	while (lo < hi)
	{
		int guess = (lo + hi) / 2;
		if (code < font->codes[guess]) hi = guess;
		else lo = guess + 1;
	}

	if (lo == 0 || font->codes[lo - 1] != code) return '?' - 32;
	else return lo - 1;
}

tinyfont_glyph_t* tinyfont_get_glyph(tinyfont_t* font, int index)
{
	return font->glyphs + index;
}

int tinyfont_kerning(tinyfont_t* font, int code0, int code1)
{
	return (int)(size_t)hashtable_find(&font->kern->table, (TINYFONT_U64)code0 << 32 | (TINYFONT_U64)code1);
}

tinyfont_t* tinyfont_create_blank(int font_height, int glyph_count)
{
	tinyfont_t* font = (tinyfont_t*)TINYFONT_ALLOC(sizeof(tinyfont_t), font->mem_ctx);
	font->glyph_count = glyph_count;
	font->font_height = font_height;
	font->kern = 0;
	font->glyphs = (tinyfont_glyph_t*)TINYFONT_ALLOC(sizeof(tinyfont_glyph_t) * font->glyph_count, font->mem_ctx);
	font->codes = (int*)TINYFONT_ALLOC(sizeof(int) * font->glyph_count, font->mem_ctx);
	return font;
}

void tinyfont_add_kerning_pair(tinyfont_t* font, int code0, int code1, int kerning)
{
	if (!font->kern)
	{
		tinyfont_kern_t* kern = (tinyfont_kern_t*)TINYFONT_ALLOC(sizeof(tinyfont_kern_t), font->mem_ctx);
		font->kern = kern;
		hashtable_init(&kern->table, sizeof(int), 256, font->mem_ctx);
	}

	TINYFONT_U64 key = (TINYFONT_U64)code0 << 32 | (TINYFONT_U64)code1;
	hashtable_insert(&font->kern->table, key, (void*)(size_t)kerning);
}

int tinyfont_fill_vertex_buffer(tinyfont_t* font, const char* text, float x0, float y0, float line_height, tinyfont_vert_t* buffer, int buffer_max, int* count_written)
{
	float x = x0;
	float y = y0;
	float font_height = (float)font->font_height;
	int i = 0;

	while (*text)
	{
		tinyfont_vert_t v;
		int c;
		text = tinyfont_decode_utf8(text, &c);

		if (c == '\n')
		{
			x = x0;
			y -= font_height + line_height;
			continue;
		}
		else if (c == '\r') continue;

		tinyfont_glyph_t* glyph = tinyfont_get_glyph(font, tinyfont_get_glyph_index(font, c));
		float x0 = (float)glyph->xoffset;
		float y0 = -(float)glyph->yoffset;

		// top left
		v.x = x + x0;
		v.y = y + y0;
		v.u = glyph->minx;
		v.v = glyph->miny;
		TINYFONT_CHECK(i < buffer_max, "`buffer_max` is too small.");
		buffer[i++] = v;

		// bottom left
		v.x = x + x0;
		v.y = y - glyph->h + y0;
		v.u = glyph->minx;
		v.v = glyph->maxy;
		TINYFONT_CHECK(i < buffer_max, "`buffer_max` is too small.");
		buffer[i++] = v;

		// top right
		v.x = x + glyph->w + x0;
		v.y = y + y0;
		v.u = glyph->maxx;
		v.v = glyph->miny;
		TINYFONT_CHECK(i < buffer_max, "`buffer_max` is too small.");
		buffer[i++] = v;

		// bottom right
		v.x = x + glyph->w + x0;
		v.y = y - glyph->h + y0;
		v.u = glyph->maxx;
		v.v = glyph->maxy;
		TINYFONT_CHECK(i < buffer_max, "`buffer_max` is too small.");
		buffer[i++] = v;

		// top right
		v.x = x + glyph->w + x0;
		v.y = y + y0;
		v.u = glyph->maxx;
		v.v = glyph->miny;
		TINYFONT_CHECK(i < buffer_max, "`buffer_max` is too small.");
		buffer[i++] = v;

		// bottom left
		v.x = x + x0;
		v.y = y - glyph->h + y0;
		v.u = glyph->minx;
		v.v = glyph->maxy;
		TINYFONT_CHECK(i < buffer_max, "`buffer_max` is too small.");
		buffer[i++] = v;

		x += glyph->xadvance;
	}

	*count_written = i;
	return 1;

tinyfont_err:
	*count_written = i;
	return 0;
}

#endif // TINYFONT_IMPLEMENTATION_ONCE
#endif // TINYFONT_IMPLEMENTATION
