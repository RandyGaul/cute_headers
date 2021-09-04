/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_spritebatch.h - v1.04

	To create implementation (the function definitions)
		#define SPRITEBATCH_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY:

		This header implements a 2D sprite batcher by tracking different textures within
		a rolling atlas cache. Over time atlases are decayed and recreated when textures
		stop being used. This header is useful for batching sprites at run-time. This avoids
		the need to compile texture atlases as a pre-process step, letting the game load
		images up individually, dramatically simplifying art pipelines.

	MORE DETAILS:

		`spritebatch_push` is used to push sprite instances into a buffer. Rendering sprites
		works by calling `spritebatch_flush`. `spritebatch_flush` will use a user-supplied
		callback to report sprite batches. This callback is of type `submit_batch_fn`. The
		batches are reported as an array of `spritebatch_sprite_t` sprites, and can be
		further sorted by the user (for example to sort by depth). Sprites in a batch share
		the same texture handle (either from the same base image, or from the same internal
		atlas).

		cute_spritebatch does not know anything about how to generate texture handles, or
		destroy them. As such, the user must supply two callbacks for creating handles and
		destroying them. These can be simple wrappers around, for example, `glGenTextures`
		and `glDeleteTextures`.

		Finally, cute_spritebatch will periodically need access to pixels from images. These
		pixels are used to generate textures, or to build atlases (which in turn generate a
		texture). cute_spritebatch does not need to know much about your images, other than
		the pixel stride. The user supplies callback of type `get_pixels_fn`, which lets
		cute_spritebatch retreive the pixels associated with a particular image. The pixels
		can be stored in RAM and handed to cute_spritebatch whenever requested, or the pixels
		can be fetched directly from disk and handed to cute_spritebatch. It doesn't matter
		to cute_spritebatch. Since `get_pixels_fn` can be called from `spritebatch_flush` it
		is recommended to avoid file i/o within the `get_pixels_fn` callback, and instead try
		to already have pixels ready in RAM.

		The `spritebatch_defrag` function performs atlas creation and texture management. It
		should be called periodically. It can be called once per game tick (once per render),
		or optionally called at a different frequency (once every N game ticks).

	PROS AND CONS:

		PROS
		- Texture atlases are completely hidden behind an api. The api in this header can
		  easily be implemented with different backend sprite batchers. For example on
		  some platforms bindless textures can be utilized in order to avoid texture
		  atlases entirely! Code using this API can have the backend implementation swapped
		  without requiring any user code to change.
		- Sprites are batched in an effective manner to dramatically reduce draw call counts.
		- Supporting hotswapping or live-reloading of images can be trivialized due to
		  moving atlas creation out of the art-pipeline and into the run-time.
		- Since atlases are built at run-time and continually maintained, images are
		  guaranteed to be drawn at the same time on-screen as their atlas neighbors. This is
		  typically not the case for atlas preprocessors, as a *guess* must be made to try
		  and organize images together in atlases that need to be drawn at roughly the same
		  time.

		CONS
		- Performance hits in the `spritebatch_defrag` function, and a little as well in
		  the `spritebatch_flush` function. Extra run-time memory usage for bookkeeping,
		  which implies a RAM hit as well as more things to clog the CPU cache.
		- If each texture comes from a separate image on-disk, opening individual files on
		  disk can be very slow. For example on Windows just performing permissions and
		  related work to open a file is time-consuming. This can be mitigated by moving
		  assets into a single larger file, for example a .zip archive and read from using
		  a file io abstraction like PHYSFS.
		- For large numbers of separate images, some file abstraction is necessary to avoid
		  a large performance hit on opening/closing many individual files. This problem is
		  *not* solved by cute_spritebatch.h, and instead should be solved by some separate
		  file abstraction system. PHYSFS is a good example of a solid file io abstraciton.

	EXAMPLE USAGE:

		spritebatch_config_t config;
		spritebatch_set_default_config(&config);
		config.batch_callback = my_report_batches_function;
		config.get_pixels_callback = my_get_pixels_function;
		config.generate_texture_callback = my_make_texture_handle_function;
		config.delete_texture_callback = my_destroy_texture_handle_function;

		spritebatch_t batcher;
		spritebatch_init(&batcher, &config);

		while (game_is_running)
		{
			for (int i = 0; i < sprite_count; ++i)
				spritebatch_push(
					&batcher,
					sprites[i].image_id,
					sprites[i].image_width_in_pixels,
					sprites[i].image_height_in_pixels, 
					sprites[i].position_x,
					sprites[i].poxition_y,
					sprites[i].scale_x,
					sprites[i].scale_y,
					sprites[i].cos_rotation_angle,
					sprites[i].sin_rotation_angle
					);

			spritebatch_tick(&batcher);
			spritebatch_defrag(&batcher);
			spritebatch_flush(&batcher);
		}

	CUSTOMIZATION:

		The following macros can be defined before including this header with the
		SPRITEBATCH_IMPLEMENTATION symbol defined, in order to customize the internal
		behavior of cute_spritebatch.h. Search this header to find how each macro is
		defined and used. Note that MALLOC/FREE functions can optionally take a context
		parameter for custom allocation.

		SPRITEBATCH_MALLOC
		SPRITEBATCH_MEMCPY
		SPRITEBATCH_MEMSET
		SPRITEBATCH_MEMMOVE
		SPRITEBATCH_ASSERT
		SPRITEBATCH_ATLAS_FLIP_Y_AXIS_FOR_UV
		SPRITEBATCH_ATLAS_EMPTY_COLOR
		SPRITEBATCH_LOG

	Revision history:
		0.01 (11/20/2017) experimental release
		1.00 (04/14/2018) initial release
		1.01 (05/07/2018) modification for easier file embedding
		1.02 (02/03/2019) moved def of spritebatch_t for easier embedding,
		                  inverted get pixels callback to let users have an easier time
		                  with memory management, added support for pixel padding along
		                  the edges of all textures (useful for certain shader effects)
		1.03 (08/18/2020) refactored `spritebatch_push` so that sprites can have userdata
		1.04 (08/20/2021) qsort -> mergesort to avoid bugs, optional override
		                  sprites_sorter_callback sorting routines provided by Kariem,
		                  added new function `spritebatch_prefetch`
*/

#ifndef SPRITEBATCH_H

#ifndef SPRITEBATCH_U64
	#define SPRITEBATCH_U64 unsigned long long
#endif // SPRITEBATCH_U64

typedef struct spritebatch_t spritebatch_t;
typedef struct spritebatch_config_t spritebatch_config_t;
typedef struct spritebatch_sprite_t spritebatch_sprite_t;

// Sprites will be pushed into the spritebatch with this struct. All the fields
// should be set before calling `spritebatch_push`, though `texture_id` and
// `sort_bits` can simply be set to zero.
//
// After sprites are pushed onto the spritebatch via `spritebatch_push`, they will
// be sorted, `texture_id` is assigned to a generated atlas, and handed back to you
// via the `submit_batch_fn` callback.
struct spritebatch_sprite_t
{
	// `image_id` must be a unique identifier for the image a sprite references.
	// You must set this value!
	SPRITEBATCH_U64 image_id;

	// The `texture_id` can set to zero `spritebatch_push`. This value will be overwritten
	// with a valid texture id of a generated atlas before batches are reported back to you.
	SPRITEBATCH_U64 texture_id;

	int w, h;         // width and height of this sprite in pixels
	float x, y;       // x and y position
	float sx, sy;     // scale on x and y axis
	float c, s;       // cosine and sine (represents cos(angle) and sin(angle))
	float minx, miny; // u coordinate -- This value is for internal use only -- do not set.
	float maxx, maxy; // v coordinate -- This value is for internal use only -- do not set.

	// This field is *completely optional* -- just set it to zero if you don't wanter to bother.
	// User-defined sorting key, see: http://realtimecollisiondetection.net/blog/?p=86
	int sort_bits;

	// This is a *completely optional* field. The idea is that the `SPRITEBATCH_SPRITE_USERDATA`
	// macro can be defined to insert arbitrary data into sprites. For example, if you want to
	// allow individual sprites to have different alpha values, or a tint color, you can add
	// in some floats here within a single struct. Internally this field is *never* accessed, and
	// is simply handed back to you in each sprite via the `submit_batch_fn` callback.
#ifdef SPRITEBATCH_SPRITE_USERDATA
	SPRITEBATCH_SPRITE_USERDATA udata;
#endif
};

// Pushes a sprite onto an internal buffer. Does no other logic.
int spritebatch_push(spritebatch_t* sb, spritebatch_sprite_t sprite);

// Ensures the image associated with your unique `image_id` is loaded up into spritebatch. This
// function pretends to draw a sprite referencing `image_id` but doesn't actually do any
// drawing at all. Use this function as an optimization to pre-load images you know will be
// drawn very soon, e.g. prefetch all ten images within a single animation just as it starts
// playing.
void spritebatch_prefetch(spritebatch_t* sb, SPRITEBATCH_U64 image_id, int w, int h);

// If a match for `image_id` is found, the texture id and uv coordinates are looked up and returned
// as a sprite instance. This is sometimes useful to render sprites through an external mechanism,
// such as Dear ImGui.
spritebatch_sprite_t spritebatch_fetch(spritebatch_t* sb, SPRITEBATCH_U64 image_id, int w, int h);

// Increments internal timestamps on all textures, for use in `spritebatch_defrag`.
void spritebatch_tick(spritebatch_t* sb);

// Sorts the internal sprites and flushes the buffer built by `spritebatch_push`. Will call
// the `submit_batch_fn` function for each batch of sprites and return them as an array. Any `image_id`
// within the `spritebatch_push` buffer that do not yet have a texture handle will request pixels
// from the image via `get_pixels_fn` and request a texture handle via `generate_texture_handle_fn`.
// Returns the number of batches created and submitted.
int spritebatch_flush(spritebatch_t* sb);

// All textures created so far by `spritebatch_flush` will be considered as candidates for creating
// new internal texture atlases. Internal texture atlases compress images together inside of one
// texture to dramatically reduce draw calls. When an atlas is created, the most recently used `image_id`
// instances are prioritized, to ensure atlases are filled with images all drawn at the same time.
// As some textures cease to draw on screen, they "decay" over time. Once enough images in an atlas
// decay, the atlas is removed, and any "live" images in the atlas are used to create new atlases.
// Can be called every 1/N times `spritebatch_flush` is called.
int spritebatch_defrag(spritebatch_t* sb);

int spritebatch_init(spritebatch_t* sb, spritebatch_config_t* config, void* udata);
void spritebatch_term(spritebatch_t* sb);

// Sprite batches are submit via synchronous callback back to the user. This function is called
// from inside `spritebatch_flush`. Each time `submit_batch_fn` is called an array of sprites
// is handed to the user. The sprites are intended to be further sorted by the user as desired
// (for example, additional sorting based on depth). `w` and `h` are the width/height, respectively,
// of the texture the batch of sprites resides upon. w/h can be useful for knowing texture dim-
// ensions, which is needed to know texel size or other measurements.
typedef void (submit_batch_fn)(spritebatch_sprite_t* sprites, int count, int texture_w, int texture_h, void* udata);

// cute_spritebatch.h needs to know how to get the pixels of an image, generate textures handles (for
// example glGenTextures for OpenGL), and destroy texture handles. These functions are all called
// from within the `spritebatch_defrag` function, and sometimes from `spritebatch_flush`.

// Called when the pixels are needed from the user. `image_id` maps to a unique image, and is *not*
// related to `texture_id` at all. `buffer` must be filled in with `bytes_to_fill` number of bytes.
// The user is assumed to know the width/height of the image, and can optionally verify that
// `bytes_to_fill` matches the user's w * h * stride for this particular image.
typedef void (get_pixels_fn)(SPRITEBATCH_U64 image_id, void* buffer, int bytes_to_fill, void* udata);

// Called with a new texture handle is needed. This will happen whenever a new atlas is created,
// and whenever new `image_id`s first appear to cute_spritebatch, and have yet to find their way
// into an appropriate atlas.
typedef SPRITEBATCH_U64 (generate_texture_handle_fn)(void* pixels, int w, int h, void* udata);

// Called whenever a texture handle is ready to be free'd up. This happens whenever a particular image
// or a particular atlas has not been used for a while, and is ready to be released.
typedef void (destroy_texture_handle_fn)(SPRITEBATCH_U64 texture_id, void* udata);

// (Optional) If the user provides this callback, cute_spritebatch will call it to sort all of sprites before submit_batch 
// callback is called. The intention of sorting is to minimize the submit_batch calls. cute_spritebatch
// provides its own internal sorting function which will be used if the user does not provide this callback.
// 
// Example using std::sort (C++) - Please note the lambda needs to be a non-capturing one.
// 
//     config.sprites_sorter_callback = [](spritebatch_sprite_t* sprites, int count)
//     {
//         std::sort(sprites, sprites + count,
//         [](const spritebatch_sprite_t& a, const spritebatch_sprite_t& b) {
//             if (a.sort_bits < b.sort_bits) return true;
//             if (a.sort_bits == b.sort_bits && a.texture_id < b.texture_id) return true;
//             return false;
//         });
//     };
typedef void (sprites_sorter_fn)(spritebatch_sprite_t* sprites, int count);

// Sets all function pointers originally defined in the `config` struct when calling `spritebatch_init`.
// Useful if DLL's are reloaded, or swapped, etc.
void spritebatch_reset_function_ptrs(spritebatch_t* sb, submit_batch_fn* batch_callback, get_pixels_fn* get_pixels_callback, generate_texture_handle_fn* generate_texture_callback, destroy_texture_handle_fn* delete_texture_callback);

// Initializes a set of good default paramaters. The users must still set
// the four callbacks inside of `config`.
void spritebatch_set_default_config(spritebatch_config_t* config);

struct spritebatch_config_t
{
	int pixel_stride;
	int atlas_width_in_pixels;
	int atlas_height_in_pixels;
	int atlas_use_border_pixels;
	int ticks_to_decay_texture;         // number of ticks it takes for a texture handle to be destroyed via `destroy_texture_handle_fn`
	int lonely_buffer_count_till_flush; // Number of unique textures allowed to persist that are not a part of an atlas yet, each one allowed is another draw call.
	                                    // These are called "lonely textures", since they don't belong to any atlas yet. Set this to 0 if you want all textures to be
	                                    // immediately put into atlases. Setting a higher number, like 64, will buffer up 64 unique textures (which means up to an
	                                    // additional 64 draw calls) before flushing them into atlases. Too low of a lonely buffer count combined with a low tick
	                                    // to decay rate will cause performance problems where atlases are constantly created and immedately destroyed -- you have
	                                    // been warned! Use `SPRITEBATCH_LOG` to gain some insight on what's going on inside the spritebatch when tuning these settings.
	float ratio_to_decay_atlas;         // from 0 to 1, once ratio is less than `ratio_to_decay_atlas`, flush active textures in atlas to lonely buffer
	float ratio_to_merge_atlases;       // from 0 to 0.5, attempts to merge atlases with some ratio of empty space
	submit_batch_fn* batch_callback;
	get_pixels_fn* get_pixels_callback;
	generate_texture_handle_fn* generate_texture_callback;
	destroy_texture_handle_fn* delete_texture_callback;
	sprites_sorter_fn* sprites_sorter_callback; // (Optional)
	void* allocator_context;
};

#define SPRITEBATCH_H
#endif

#if !defined(SPRITE_BATCH_INTERNAL_H)

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

// end hashtable.h (more later)

typedef struct
{
	SPRITEBATCH_U64 image_id;
	int sort_bits;
	int w;
	int h;
	float x, y;
	float sx, sy;
	float c, s;
#ifdef SPRITEBATCH_SPRITE_USERDATA
	SPRITEBATCH_SPRITE_USERDATA udata;
#endif
} spritebatch_internal_sprite_t;

typedef struct
{
	int timestamp;
	int w, h;
	float minx, miny;
	float maxx, maxy;
	SPRITEBATCH_U64 image_id;
} spritebatch_internal_texture_t;

typedef struct spritebatch_internal_atlas_t
{
	SPRITEBATCH_U64 texture_id;
	float volume_ratio;
	hashtable_t sprites_to_textures;
	struct spritebatch_internal_atlas_t* next;
	struct spritebatch_internal_atlas_t* prev;
} spritebatch_internal_atlas_t;

typedef struct
{
	int timestamp;
	int w, h;
	SPRITEBATCH_U64 image_id;
	SPRITEBATCH_U64 texture_id;
} spritebatch_internal_lonely_texture_t;



struct spritebatch_t
{
	int input_count;
	int input_capacity;
	spritebatch_internal_sprite_t* input_buffer;

	int sprite_count;
	int sprite_capacity;
	spritebatch_sprite_t* sprites;
	spritebatch_sprite_t* sprites_scratch;

	int key_buffer_count;
	int key_buffer_capacity;
	SPRITEBATCH_U64* key_buffer;

	int pixel_buffer_size; // number of pixels
	void* pixel_buffer;

	hashtable_t sprites_to_lonely_textures;
	hashtable_t sprites_to_atlases;

	spritebatch_internal_atlas_t* atlases;

	int pixel_stride;
	int atlas_width_in_pixels;
	int atlas_height_in_pixels;
	int atlas_use_border_pixels;
	int ticks_to_decay_texture;
	int lonely_buffer_count_till_flush;
	int lonely_buffer_count_till_decay;
	float ratio_to_decay_atlas;
	float ratio_to_merge_atlases;
	submit_batch_fn* batch_callback;
	get_pixels_fn* get_pixels_callback;
	generate_texture_handle_fn* generate_texture_callback;
	destroy_texture_handle_fn* delete_texture_callback;
	sprites_sorter_fn* sprites_sorter_callback;
	void* mem_ctx;
	void* udata;
};

#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
	#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#ifndef SPRITEBATCH_MALLOC
	#include <stdlib.h>
	#define SPRITEBATCH_MALLOC(size, ctx) malloc(size)
	#define SPRITEBATCH_FREE(ptr, ctx) free(ptr)
#endif

#ifndef SPRITEBATCH_MEMCPY
	#include <string.h>
	#define SPRITEBATCH_MEMCPY(dst, src, n) memcpy(dst, src, n)
#endif

#ifndef SPRITEBATCH_MEMSET
	#include <string.h>
	#define SPRITEBATCH_MEMSET(ptr, val, n) memset(ptr, val, n)
#endif

#ifndef SPRITEBATCH_MEMMOVE
	#include <string.h>
	#define SPRITEBATCH_MEMMOVE(dst, src, n) memmove(dst, src, n)
#endif

#ifndef SPRITEBATCH_ASSERT
	#include <assert.h>
	#define SPRITEBATCH_ASSERT(condition) assert(condition)
#endif

// flips output uv coordinate's y. Can be useful to "flip image on load"
#ifndef SPRITEBATCH_ATLAS_FLIP_Y_AXIS_FOR_UV
	#define SPRITEBATCH_ATLAS_FLIP_Y_AXIS_FOR_UV 1
#endif

// flips output uv coordinate's y. Can be useful to "flip image on load"
#ifndef SPRITEBATCH_LONELY_FLIP_Y_AXIS_FOR_UV
	#define SPRITEBATCH_LONELY_FLIP_Y_AXIS_FOR_UV 1
#endif

#ifndef SPRITEBATCH_ATLAS_EMPTY_COLOR
	#define SPRITEBATCH_ATLAS_EMPTY_COLOR 0x00000000
#endif

#ifndef SPRITEBATCH_LOG
	#if 0
		#define SPRITEBATCH_LOG printf
	#else
		#define SPRITEBATCH_LOG(...)
	#endif
#endif

#ifndef HASHTABLE_MEMSET
	#define HASHTABLE_MEMSET(ptr, val, n) SPRITEBATCH_MEMSET(ptr, val, n)
#endif

#ifndef HASHTABLE_MEMCPY
	#define HASHTABLE_MEMCPY(dst, src, n) SPRITEBATCH_MEMCPY(dst, src, n)
#endif

#ifndef HASHTABLE_MALLOC
	#define HASHTABLE_MALLOC(ctx, size) SPRITEBATCH_MALLOC(size, ctx)
#endif

#ifndef HASHTABLE_FREE
	#define HASHTABLE_FREE(ctx, ptr) SPRITEBATCH_FREE(ptr, ctx)
#endif

#define SPRITE_BATCH_INTERNAL_H
#endif

#ifdef SPRITEBATCH_IMPLEMENTATION
#ifndef SPRITEBATCH_IMPLEMENTATION_ONCE
#define SPRITEBATCH_IMPLEMENTATION_ONCE

#define HASHTABLE_IMPLEMENTATION

#ifdef HASHTABLE_IMPLEMENTATION
#ifndef HASHTABLE_IMPLEMENTATION_ONCE
#define HASHTABLE_IMPLEMENTATION_ONCE

// hashtable.h implementation by Mattias Gustavsson
// See: http://www.mattiasgustavsson.com/ and https://github.com/mattiasgustavsson/libs/blob/master/hashtable.h
// begin hashtable.h (continuing from first time)

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


bool sprite_batch_internal_use_scratch_buffer(spritebatch_t* sb)
{
	return sb->sprites_sorter_callback == 0;
}

int spritebatch_init(spritebatch_t* sb, spritebatch_config_t* config, void* udata)
{
	// read config params
	if (!config | !sb) return 1;
	sb->pixel_stride = config->pixel_stride;
	sb->atlas_width_in_pixels = config->atlas_width_in_pixels;
	sb->atlas_height_in_pixels = config->atlas_height_in_pixels;
	sb->atlas_use_border_pixels = config->atlas_use_border_pixels;
	sb->ticks_to_decay_texture = config->ticks_to_decay_texture;
	sb->lonely_buffer_count_till_flush = config->lonely_buffer_count_till_flush;
	sb->lonely_buffer_count_till_decay = sb->lonely_buffer_count_till_flush / 2;
	if (sb->lonely_buffer_count_till_decay <= 0) sb->lonely_buffer_count_till_decay = 1;
	sb->ratio_to_decay_atlas = config->ratio_to_decay_atlas;
	sb->ratio_to_merge_atlases = config->ratio_to_merge_atlases;
	sb->batch_callback = config->batch_callback;
	sb->get_pixels_callback = config->get_pixels_callback;
	sb->generate_texture_callback = config->generate_texture_callback;
	sb->delete_texture_callback = config->delete_texture_callback;
	sb->sprites_sorter_callback = config->sprites_sorter_callback;
	sb->mem_ctx = config->allocator_context;
	sb->udata = udata;

	if (sb->atlas_width_in_pixels < 1 || sb->atlas_height_in_pixels < 1) return 1;
	if (sb->ticks_to_decay_texture < 1) return 1;
	if (sb->ratio_to_decay_atlas < 0 || sb->ratio_to_decay_atlas > 1.0f) return 1;
	if (sb->ratio_to_merge_atlases < 0 || sb->ratio_to_merge_atlases > 0.5f) return 1;
	if (!sb->batch_callback) return 1;
	if (!sb->get_pixels_callback) return 1;
	if (!sb->generate_texture_callback) return 1;
	if (!sb->delete_texture_callback) return 1;

	// initialize input buffer
	sb->input_count = 0;
	sb->input_capacity = 1024;
	sb->input_buffer = (spritebatch_internal_sprite_t*)SPRITEBATCH_MALLOC(sizeof(spritebatch_internal_sprite_t) * sb->input_capacity, sb->mem_ctx);
	if (!sb->input_buffer) return 1;

	// initialize sprite buffer
	sb->sprite_count = 0;
	sb->sprite_capacity = 1024;
	sb->sprites = (spritebatch_sprite_t*)SPRITEBATCH_MALLOC(sizeof(spritebatch_sprite_t) * sb->sprite_capacity, sb->mem_ctx);

	sb->sprites_scratch = 0;
	if (sprite_batch_internal_use_scratch_buffer(sb))
	{
		sb->sprites_scratch = (spritebatch_sprite_t*)SPRITEBATCH_MALLOC(sizeof(spritebatch_sprite_t) * sb->sprite_capacity, sb->mem_ctx);
	}
	if (!sb->sprites) return 1;

	if (sprite_batch_internal_use_scratch_buffer(sb))
	{
		if (!sb->sprites_scratch) return 1;
	}

	// initialize key buffer (for marking hash table entries for deletion)
	sb->key_buffer_count = 0;
	sb->key_buffer_capacity = 1024;
	sb->key_buffer = (SPRITEBATCH_U64*)SPRITEBATCH_MALLOC(sizeof(SPRITEBATCH_U64) * sb->key_buffer_capacity, sb->mem_ctx);

	// initialize pixel buffer for grabbing pixel data from the user as needed
	sb->pixel_buffer_size = 1024;
	sb->pixel_buffer = SPRITEBATCH_MALLOC(sb->pixel_buffer_size * sb->pixel_stride, sb->mem_ctx);

	// setup tables
	hashtable_init(&sb->sprites_to_lonely_textures, sizeof(spritebatch_internal_lonely_texture_t), 1024, sb->mem_ctx);
	hashtable_init(&sb->sprites_to_atlases, sizeof(spritebatch_internal_atlas_t*), 16, sb->mem_ctx);

	sb->atlases = 0;

	return 0;
}

void spritebatch_term(spritebatch_t* sb)
{
	SPRITEBATCH_FREE(sb->input_buffer, sb->mem_ctx);
	SPRITEBATCH_FREE(sb->sprites, sb->mem_ctx);
	if (sb->sprites_scratch)
	{
		SPRITEBATCH_FREE(sb->sprites_scratch, sb->mem_ctx);
	}
	SPRITEBATCH_FREE(sb->key_buffer, sb->mem_ctx);
	SPRITEBATCH_FREE(sb->pixel_buffer, ctx->mem_ctx);
	hashtable_term(&sb->sprites_to_lonely_textures);
	hashtable_term(&sb->sprites_to_atlases);

	if (sb->atlases)
	{
		spritebatch_internal_atlas_t* atlas = sb->atlases;
		spritebatch_internal_atlas_t* sentinel = sb->atlases;
		do
		{
			hashtable_term(&atlas->sprites_to_textures);
			spritebatch_internal_atlas_t* next = atlas->next;
			SPRITEBATCH_FREE(atlas, sb->mem_ctx);
			atlas = next;
		}
		while (atlas != sentinel);
	}

	SPRITEBATCH_MEMSET(sb, 0, sizeof(spritebatch_t));
}

void spritebatch_reset_function_ptrs(spritebatch_t* sb, submit_batch_fn* batch_callback, get_pixels_fn* get_pixels_callback, generate_texture_handle_fn* generate_texture_callback, destroy_texture_handle_fn* delete_texture_callback, sprites_sorter_fn* sprites_sorter_callback)
{
	sb->batch_callback = batch_callback;
	sb->get_pixels_callback = get_pixels_callback;
	sb->generate_texture_callback = generate_texture_callback;
	sb->delete_texture_callback = delete_texture_callback;
	sb->sprites_sorter_callback = sprites_sorter_callback;
}

void spritebatch_set_default_config(spritebatch_config_t* config)
{
	config->pixel_stride = sizeof(char) * 4;
	config->atlas_width_in_pixels = 1024;
	config->atlas_height_in_pixels = 1024;
	config->atlas_use_border_pixels = 0;
	config->ticks_to_decay_texture = 60 * 30;
	config->lonely_buffer_count_till_flush = 64;
	config->ratio_to_decay_atlas = 0.5f;
	config->ratio_to_merge_atlases = 0.25f;
	config->batch_callback = 0;
	config->generate_texture_callback = 0;
	config->delete_texture_callback = 0;
	config->sprites_sorter_callback = 0;
	config->allocator_context = 0;
}

#define SPRITEBATCH_CHECK_BUFFER_GROW(ctx, count, capacity, data, type) \
	do { \
		if (ctx->count == ctx->capacity) \
		{ \
			int new_capacity = ctx->capacity * 2; \
			void* new_data = SPRITEBATCH_MALLOC(sizeof(type) * new_capacity, ctx->mem_ctx); \
			if (!new_data) return 0; \
			SPRITEBATCH_MEMCPY(new_data, ctx->data, sizeof(type) * ctx->count); \
			SPRITEBATCH_FREE(ctx->data, ctx->mem_ctx); \
			ctx->data = (type*)new_data; \
			ctx->capacity = new_capacity; \
		} \
	} while (0)

int spritebatch_push(spritebatch_t* sb, spritebatch_sprite_t sprite)
{
	SPRITEBATCH_ASSERT(sprite.w <= sb->atlas_width_in_pixels);
	SPRITEBATCH_ASSERT(sprite.h <= sb->atlas_height_in_pixels);
	SPRITEBATCH_CHECK_BUFFER_GROW(sb, input_count, input_capacity, input_buffer, spritebatch_internal_sprite_t);
	spritebatch_internal_sprite_t sprite_out;
	sprite_out.image_id = sprite.image_id;
	sprite_out.sort_bits = sprite.sort_bits;
	sprite_out.w = sprite.w;
	sprite_out.h = sprite.h;
	sprite_out.x = sprite.x;
	sprite_out.y = sprite.y;
	sprite_out.sx = sprite.sx + (sb->atlas_use_border_pixels ? (sprite.sx / (float)sprite.w) * 2.0f : 0);
	sprite_out.sy = sprite.sy + (sb->atlas_use_border_pixels ? (sprite.sy / (float)sprite.h) * 2.0f : 0);
	sprite_out.c = sprite.c;
	sprite_out.s = sprite.s;
#ifdef SPRITEBATCH_SPRITE_USERDATA
	sprite_out.udata = sprite.udata;
#endif
	sb->input_buffer[sb->input_count++] = sprite_out;
	return 1;
}

int spritebatch_internal_lonely_sprite(spritebatch_t* sb, SPRITEBATCH_U64 image_id, int w, int h, spritebatch_sprite_t* sprite_out, int skip_missing_textures);

void spritebatch_prefetch(spritebatch_t* sb, SPRITEBATCH_U64 image_id, int w, int h)
{
	void* atlas_ptr = hashtable_find(&sb->sprites_to_atlases, image_id);
	if (!atlas_ptr) spritebatch_internal_lonely_sprite(sb, image_id, w, h, NULL, 0);
}

spritebatch_sprite_t spritebatch_fetch(spritebatch_t* sb, SPRITEBATCH_U64 image_id, int w, int h)
{
	spritebatch_sprite_t s;
	SPRITEBATCH_MEMSET(&s, 0, sizeof(s));

	s.w = w;
	s.h = h;
	s.c = 1;
	s.s = 0;

	void* atlas_ptr = hashtable_find(&sb->sprites_to_atlases, image_id);
	if (atlas_ptr) {
		spritebatch_internal_atlas_t* atlas = *(spritebatch_internal_atlas_t**)atlas_ptr;
		s.texture_id = atlas->texture_id;

		spritebatch_internal_texture_t* tex = (spritebatch_internal_texture_t*)hashtable_find(&atlas->sprites_to_textures, image_id);
		if (tex) {
			s.maxx = tex->maxx;
			s.maxy = tex->maxy;
			s.minx = tex->minx;
			s.miny = tex->miny;
		}
	} else {
		spritebatch_internal_lonely_sprite(sb, image_id, w, h, &s, 0);
	}

	return s;
}

static int spritebatch_internal_sprite_less_than_or_equal(spritebatch_sprite_t* a, spritebatch_sprite_t* b)
{
	if (a->sort_bits < b->sort_bits) return 1;
	if (a->sort_bits == b->sort_bits && a->texture_id <= b->texture_id) return 1;
	return 0;
}

void spritebatch_internal_merge_sort_iteration(spritebatch_sprite_t* a, int lo, int split, int hi, spritebatch_sprite_t* b)
{
	int i = lo, j = split;
	for (int k = lo; k < hi; k++) {
		if (i < split && (j >= hi || spritebatch_internal_sprite_less_than_or_equal(a + i, a + j))) {
			b[k] = a[i];
			i = i + 1;
		} else {
			b[k] = a[j];
			j = j + 1;
		}
	}
}

void spritebatch_internal_merge_sort_recurse(spritebatch_sprite_t* b, int lo, int hi, spritebatch_sprite_t* a)
{
	if (hi - lo <= 1) return;
	int split = (lo + hi) / 2;
	spritebatch_internal_merge_sort_recurse(a, lo,  split, b);
	spritebatch_internal_merge_sort_recurse(a, split, hi, b);
	spritebatch_internal_merge_sort_iteration(b, lo, split, hi, a);
}

void spritebatch_internal_merge_sort(spritebatch_sprite_t* a, spritebatch_sprite_t* b, int n)
{
	SPRITEBATCH_MEMCPY(b, a, sizeof(spritebatch_sprite_t) * n);
	spritebatch_internal_merge_sort_recurse(b, 0, n, a);
}

void spritebatch_internal_sort_sprites(spritebatch_t* sb)
{
	if (sb->sprites_sorter_callback) sb->sprites_sorter_callback(sb->sprites, sb->sprite_count);
	else spritebatch_internal_merge_sort(sb->sprites, sb->sprites_scratch, sb->sprite_count);
}

static inline void spritebatch_internal_get_pixels(spritebatch_t* sb, SPRITEBATCH_U64 image_id, int w, int h)
{
	int size = sb->atlas_use_border_pixels ? sb->pixel_stride * (w + 2) * (h + 2) : sb->pixel_stride * w * h;
	if (size > sb->pixel_buffer_size)
	{
		SPRITEBATCH_FREE(sb->pixel_buffer, ctx->mem_ctx);
		sb->pixel_buffer_size = size;
		sb->pixel_buffer = SPRITEBATCH_MALLOC(sb->pixel_buffer_size, ctx->mem_ctx);
		if (!sb->pixel_buffer) return;
	}

	SPRITEBATCH_MEMSET(sb->pixel_buffer, 0, size);
	int size_from_user = sb->pixel_stride * w * h;
	sb->get_pixels_callback(image_id, sb->pixel_buffer, size_from_user, sb->udata);

	if (sb->atlas_use_border_pixels) {
		// Expand image from top-left corner, offset by (1, 1).
		int w0 = w;
		int h0 = h;
		w += 2;
		h += 2;
		char* buffer = (char*)sb->pixel_buffer;
		int dst_row_stride = w * sb->pixel_stride;
		int src_row_stride = w0 * sb->pixel_stride;
		int src_row_offset = sb->pixel_stride;
		for (int i = 0; i < h - 2; ++i)
		{
			char* src_row = buffer + (h0 - i - 1) * src_row_stride;
			char* dst_row = buffer + (h - i - 2) * dst_row_stride + src_row_offset;
			SPRITEBATCH_MEMMOVE(dst_row, src_row, src_row_stride);
		}

		// Clear the border pixels.
		int pixel_stride = sb->pixel_stride;
		SPRITEBATCH_MEMSET(buffer, 0, dst_row_stride);
		for (int i = 1; i < h - 1; ++i)
		{
			SPRITEBATCH_MEMSET(buffer + i * dst_row_stride, 0, pixel_stride);
			SPRITEBATCH_MEMSET(buffer + i * dst_row_stride + src_row_stride + src_row_offset, 0, pixel_stride);
		}
		SPRITEBATCH_MEMSET(buffer + (h - 1) * dst_row_stride, 0, dst_row_stride);
	}
}

static inline SPRITEBATCH_U64 spritebatch_internal_generate_texture_handle(spritebatch_t* sb, SPRITEBATCH_U64 image_id, int w, int h)
{
	spritebatch_internal_get_pixels(sb, image_id, w, h);
	if (sb->atlas_use_border_pixels)
	{
		w += 2;
		h += 2;
	}
	return sb->generate_texture_callback(sb->pixel_buffer, w, h, sb->udata);
}

spritebatch_internal_lonely_texture_t* spritebatch_internal_lonelybuffer_push(spritebatch_t* sb, SPRITEBATCH_U64 image_id, int w, int h, int make_tex)
{
	spritebatch_internal_lonely_texture_t texture;
	texture.timestamp = 0;
	texture.w = w;
	texture.h = h;
	texture.image_id = image_id;
	texture.texture_id = make_tex ? spritebatch_internal_generate_texture_handle(sb, image_id, w, h) : ~0;
	return (spritebatch_internal_lonely_texture_t*)hashtable_insert(&sb->sprites_to_lonely_textures, image_id, &texture);
}

int spritebatch_internal_lonely_sprite(spritebatch_t* sb, SPRITEBATCH_U64 image_id, int w, int h, spritebatch_sprite_t* sprite_out, int skip_missing_textures)
{
	spritebatch_internal_lonely_texture_t* tex = (spritebatch_internal_lonely_texture_t*)hashtable_find(&sb->sprites_to_lonely_textures, image_id);

	if (skip_missing_textures)
	{
		if (!tex) spritebatch_internal_lonelybuffer_push(sb, image_id, w, h, 0);
		return 1;
	}

	else
	{
		if (!tex) tex = spritebatch_internal_lonelybuffer_push(sb, image_id, w, h, 1);
		else if (tex->texture_id == ~0) tex->texture_id = spritebatch_internal_generate_texture_handle(sb, image_id, w, h);
		tex->timestamp = 0;

		if (sprite_out) {
			sprite_out->texture_id = tex->texture_id;
			sprite_out->minx = sprite_out->miny = 0;
			sprite_out->maxx = sprite_out->maxy = 1.0f;

			if (SPRITEBATCH_LONELY_FLIP_Y_AXIS_FOR_UV)
			{
				float tmp = sprite_out->miny;
				sprite_out->miny = sprite_out->maxy;
				sprite_out->maxy = tmp;
			}
		}

		return 0;
	}
}

int spritebatch_internal_push_sprite(spritebatch_t* sb, spritebatch_internal_sprite_t* s, int skip_missing_textures)
{
	int skipped_tex = 0;
	spritebatch_sprite_t sprite;
	sprite.image_id = s->image_id;
	sprite.sort_bits = s->sort_bits;
	sprite.x = s->x;
	sprite.y = s->y;
	sprite.w = s->w;
	sprite.h = s->h;
	sprite.sx = s->sx;
	sprite.sy = s->sy;
	sprite.c = s->c;
	sprite.s = s->s;
#ifdef SPRITEBATCH_SPRITE_USERDATA
	sprite.udata = s->udata;
#endif

	void* atlas_ptr = hashtable_find(&sb->sprites_to_atlases, s->image_id);
	if (atlas_ptr)
	{
		spritebatch_internal_atlas_t* atlas = *(spritebatch_internal_atlas_t**)atlas_ptr;
		sprite.texture_id = atlas->texture_id;

		spritebatch_internal_texture_t* tex = (spritebatch_internal_texture_t*)hashtable_find(&atlas->sprites_to_textures, s->image_id);
		SPRITEBATCH_ASSERT(tex);
		tex->timestamp = 0;
		sprite.w = tex->w;
		sprite.h = tex->h;
		sprite.minx = tex->minx;
		sprite.miny = tex->miny;
		sprite.maxx = tex->maxx;
		sprite.maxy = tex->maxy;
	}

	else skipped_tex = spritebatch_internal_lonely_sprite(sb, s->image_id, s->w, s->h, &sprite, skip_missing_textures);

	if (!skipped_tex)
	{
		if (sb->sprite_count >= sb->sprite_capacity) {
			int new_capacity = sb->sprite_capacity * 2;
			void* new_data = SPRITEBATCH_MALLOC(sizeof(spritebatch_sprite_t) * new_capacity, sb->mem_ctx);
			if (!new_data) return 0;
			SPRITEBATCH_MEMCPY(new_data, sb->sprites, sizeof(spritebatch_sprite_t) * sb->sprite_count);
			SPRITEBATCH_FREE(sb->sprites, sb->mem_ctx);
			sb->sprites = (spritebatch_sprite_t*)new_data;
			sb->sprite_capacity = new_capacity;

			if (sb->sprites_scratch)
			{
				SPRITEBATCH_FREE(sb->sprites_scratch, sb->mem_ctx);
			}

			if (sprite_batch_internal_use_scratch_buffer(sb))
			{
				sb->sprites_scratch = (spritebatch_sprite_t*)SPRITEBATCH_MALLOC(sizeof(spritebatch_sprite_t) * new_capacity, sb->mem_ctx);
			}
		}
		sb->sprites[sb->sprite_count++] = sprite;
	}
	return skipped_tex;
}

void spritebatch_internal_process_input(spritebatch_t* sb, int skip_missing_textures)
{
	int skipped_index = 0;
	for (int i = 0; i < sb->input_count; ++i)
	{
		spritebatch_internal_sprite_t* s = sb->input_buffer + i;
		int skipped = spritebatch_internal_push_sprite(sb, s, skip_missing_textures);
		if (skip_missing_textures && skipped) sb->input_buffer[skipped_index++] = *s;
	}

	sb->input_count = skipped_index;
}

void spritebatch_tick(spritebatch_t* sb)
{
	spritebatch_internal_atlas_t* atlas = sb->atlases;
	if (atlas)
	{
		spritebatch_internal_atlas_t* sentinel = atlas;
		do
		{
			int texture_count = hashtable_count(&atlas->sprites_to_textures);
			spritebatch_internal_texture_t* textures = (spritebatch_internal_texture_t*)hashtable_items(&atlas->sprites_to_textures);
			for (int i = 0; i < texture_count; ++i) textures[i].timestamp += 1;
			atlas = atlas->next;
		}
		while (atlas != sentinel);
	}

	int texture_count = hashtable_count(&sb->sprites_to_lonely_textures);
	spritebatch_internal_lonely_texture_t* lonely_textures = (spritebatch_internal_lonely_texture_t*)hashtable_items(&sb->sprites_to_lonely_textures);
	for (int i = 0; i < texture_count; ++i) lonely_textures[i].timestamp += 1;
}

int spritebatch_flush(spritebatch_t* sb)
{
	// process input buffer, make any necessary lonely textures
	// convert user sprites to internal format
	// lookup uv coordinates
	spritebatch_internal_process_input(sb, 0);

	// patchup any missing lonely textures that may have come from atlases decaying and whatnot
	int texture_count = hashtable_count(&sb->sprites_to_lonely_textures);
	spritebatch_internal_lonely_texture_t* lonely_textures = (spritebatch_internal_lonely_texture_t*)hashtable_items(&sb->sprites_to_lonely_textures);
	for (int i = 0; i < texture_count; ++i)
	{
		spritebatch_internal_lonely_texture_t* lonely = lonely_textures + i;
		if (lonely->texture_id == ~0) lonely->texture_id = spritebatch_internal_generate_texture_handle(sb, lonely->image_id, lonely->w, lonely->h);
	}

	// sort internal sprite buffer and submit batches
	spritebatch_internal_sort_sprites(sb);

	int min = 0;
	int max = 0;
	int done = !sb->sprite_count;
	int count = 0;
	while (!done)
	{
		SPRITEBATCH_U64 id = sb->sprites[min].texture_id;
		SPRITEBATCH_U64 image_id = sb->sprites[min].image_id;

		while (1)
		{
			if (max == sb->sprite_count)
			{
				done = 1;
				break;
			}

			if (id != sb->sprites[max].texture_id)
				break;

			++max;
		}

		int batch_count = max - min;
		if (batch_count)
		{
			void* atlas_ptr = hashtable_find(&sb->sprites_to_atlases, image_id);
			int w, h;

			if (atlas_ptr)
			{
				w = sb->atlas_width_in_pixels;
				h = sb->atlas_height_in_pixels;
			}

			else
			{
				spritebatch_internal_lonely_texture_t* tex = (spritebatch_internal_lonely_texture_t*)hashtable_find(&sb->sprites_to_lonely_textures, image_id);
				SPRITEBATCH_ASSERT(tex);
				w = tex->w;
				h = tex->h;
				if (sb->atlas_use_border_pixels)
				{
					w += 2;
					h += 2;
				}
			}

			sb->batch_callback(sb->sprites + min, batch_count, w, h, sb->udata);
			++count;
		}
		min = max;
	}

	sb->sprite_count = 0;
	if (count > 1) {
		SPRITEBATCH_LOG("Flushed %d batches.\n", count);
	}

	return count;
}

typedef struct
{
	int x;
	int y;
} spritebatch_v2_t;

typedef struct
{
	int img_index;
	spritebatch_v2_t size;
	spritebatch_v2_t min;
	spritebatch_v2_t max;
	int fit;
} spritebatch_internal_integer_image_t;

static spritebatch_v2_t spritebatch_v2(int x, int y)
{
	spritebatch_v2_t v;
	v.x = x;
	v.y = y;
	return v;
}

static spritebatch_v2_t spritebatch_sub(spritebatch_v2_t a, spritebatch_v2_t b)
{
	spritebatch_v2_t v;
	v.x = a.x - b.x;
	v.y = a.y - b.y;
	return v;
}

static spritebatch_v2_t spritebatch_add(spritebatch_v2_t a, spritebatch_v2_t b)
{
	spritebatch_v2_t v;
	v.x = a.x + b.x;
	v.y = a.y + b.y;
	return v;
}

typedef struct
{
	spritebatch_v2_t size;
	spritebatch_v2_t min;
	spritebatch_v2_t max;
} spritebatch_internal_atlas_node_t;

static spritebatch_internal_atlas_node_t* spritebatch_best_fit(int sp, int w, int h, spritebatch_internal_atlas_node_t* nodes)
{
	int best_volume = INT_MAX;
	spritebatch_internal_atlas_node_t *best_node = 0;
	int img_volume = w * h;

	for ( int i = 0; i < sp; ++i )
	{
		spritebatch_internal_atlas_node_t *node = nodes + i;
		int can_contain = node->size.x >= w && node->size.y >= h;
		if ( can_contain )
		{
			int node_volume = node->size.x * node->size.y;
			if ( node_volume == img_volume ) return node;
			if ( node_volume < best_volume )
			{
				best_volume = node_volume;
				best_node = node;
			}
		}
	}

	return best_node;
}

static int spritebatch_internal_image_less_than_or_equal(spritebatch_internal_integer_image_t* a, spritebatch_internal_integer_image_t* b)
{
	int perimeterA = 2 * (a->size.x + a->size.y);
	int perimeterB = 2 * (b->size.x + b->size.y);
	return perimeterB <= perimeterA;
}

void spritebatch_internal_image_merge_sort_iteration(spritebatch_internal_integer_image_t* a, int lo, int split, int hi, spritebatch_internal_integer_image_t* b)
{
	int i = lo, j = split;
	for (int k = lo; k < hi; k++) {
		if (i < split && (j >= hi || spritebatch_internal_image_less_than_or_equal(a + i, a + j))) {
			b[k] = a[i];
			i = i + 1;
		} else {
			b[k] = a[j];
			j = j + 1;
		}
	}
}

void spritebatch_internal_image_merge_sort_recurse(spritebatch_internal_integer_image_t* b, int lo, int hi, spritebatch_internal_integer_image_t* a)
{
	if (hi - lo <= 1) return;
	int split = (lo + hi) / 2;
	spritebatch_internal_image_merge_sort_recurse(a, lo,  split, b);
	spritebatch_internal_image_merge_sort_recurse(a, split, hi, b);
	spritebatch_internal_image_merge_sort_iteration(b, lo, split, hi, a);
}

void spritebatch_internal_image_merge_sort(spritebatch_internal_integer_image_t* a, spritebatch_internal_integer_image_t* b, int n)
{
	SPRITEBATCH_MEMCPY(b, a, sizeof(spritebatch_internal_integer_image_t) * n);
	spritebatch_internal_image_merge_sort_recurse(b, 0, n, a);
}

typedef struct
{
	int img_index;    // index into the `imgs` array
	int w, h;         // pixel w/h of original image
	float minx, miny; // u coordinate
	float maxx, maxy; // v coordinate
	int fit;          // non-zero if image fit and was placed into the atlas
} spritebatch_internal_atlas_image_t;

#define SPRITEBATCH_CHECK( X, Y ) do { if ( !(X) ) { SPRITEBATCH_LOG(Y); goto sb_err; } } while ( 0 )

void spritebatch_make_atlas(spritebatch_t* sb, spritebatch_internal_atlas_t* atlas_out, const spritebatch_internal_lonely_texture_t* imgs, int img_count)
{
	float w0, h0, div, wTol, hTol;
	int atlas_image_size, atlas_stride, sp;
	void* atlas_pixels = 0;
	int atlas_node_capacity = img_count * 2;
	spritebatch_internal_integer_image_t* images = 0;
	spritebatch_internal_integer_image_t* images_scratch = 0;
	spritebatch_internal_atlas_node_t* nodes = 0;
	int pixel_stride = sb->pixel_stride;
	int atlas_width = sb->atlas_width_in_pixels;
	int atlas_height = sb->atlas_height_in_pixels;
	float volume_used = 0;

	images = (spritebatch_internal_integer_image_t*)SPRITEBATCH_MALLOC(sizeof(spritebatch_internal_integer_image_t) * img_count, sb->mem_ctx);
	images_scratch = (spritebatch_internal_integer_image_t*)SPRITEBATCH_MALLOC(sizeof(spritebatch_internal_integer_image_t) * img_count, sb->mem_ctx);
	nodes = (spritebatch_internal_atlas_node_t*)SPRITEBATCH_MALLOC(sizeof(spritebatch_internal_atlas_node_t) * atlas_node_capacity, sb->mem_ctx);
	SPRITEBATCH_CHECK(images, "out of mem");
	SPRITEBATCH_CHECK(nodes, "out of mem");

	for (int i = 0; i < img_count; ++i)
	{
		const spritebatch_internal_lonely_texture_t* img = imgs + i;
		spritebatch_internal_integer_image_t* image = images + i;
		image->fit = 0;
		image->size = sb->atlas_use_border_pixels ? spritebatch_v2(img->w + 2, img->h + 2) : spritebatch_v2(img->w, img->h);
		image->img_index = i;
	}

	// Sort PNGs from largest to smallest
	spritebatch_internal_image_merge_sort(images, images_scratch, img_count);

	// stack pointer, the stack is the nodes array which we will
	// allocate nodes from as necessary.
	sp = 1;

	nodes[0].min = spritebatch_v2(0, 0);
	nodes[0].max = spritebatch_v2(atlas_width, atlas_height);
	nodes[0].size = spritebatch_v2(atlas_width, atlas_height);

	// Nodes represent empty space in the atlas. Placing a texture into the
	// atlas involves splitting a node into two smaller pieces (or, if a
	// perfect fit is found, deleting the node).
	for (int i = 0; i < img_count; ++i)
	{
		spritebatch_internal_integer_image_t* image = images + i;
		int width = image->size.x;
		int height = image->size.y;
		spritebatch_internal_atlas_node_t *best_fit = spritebatch_best_fit(sp, width, height, nodes);

		if (!best_fit) {
			image->fit = 0;
			continue;
		}

		image->min = best_fit->min;
		image->max = spritebatch_add(image->min, image->size);

		if (best_fit->size.x == width && best_fit->size.y == height)
		{
			spritebatch_internal_atlas_node_t* last_node = nodes + --sp;
			*best_fit = *last_node;
			image->fit = 1;

			continue;
		}

		image->fit = 1;

		if (sp == atlas_node_capacity)
		{
			int new_capacity = atlas_node_capacity * 2;
			spritebatch_internal_atlas_node_t* new_nodes = (spritebatch_internal_atlas_node_t*)SPRITEBATCH_MALLOC(sizeof(spritebatch_internal_atlas_node_t) * new_capacity, mem_ctx);
			SPRITEBATCH_CHECK(new_nodes, "out of mem");
			memcpy(new_nodes, nodes, sizeof(spritebatch_internal_atlas_node_t) * sp);
			SPRITEBATCH_FREE(nodes, mem_ctx);
			nodes = new_nodes;
			atlas_node_capacity = new_capacity;
		}

		spritebatch_internal_atlas_node_t* new_node = nodes + sp++;
		new_node->min = best_fit->min;

		// Split bestFit along x or y, whichever minimizes
		// fragmentation of empty space
		spritebatch_v2_t d = spritebatch_sub(best_fit->size, spritebatch_v2(width, height));
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

		new_node->max = spritebatch_add(new_node->min, new_node->size);
	}

	// Write the final atlas image, use SPRITEBATCH_ATLAS_EMPTY_COLOR as base color
	atlas_stride = atlas_width * pixel_stride;
	atlas_image_size = atlas_width * atlas_height * pixel_stride;
	atlas_pixels = SPRITEBATCH_MALLOC(atlas_image_size, mem_ctx);
	SPRITEBATCH_CHECK(atlas_image_size, "out of mem");
	SPRITEBATCH_MEMSET(atlas_pixels, SPRITEBATCH_ATLAS_EMPTY_COLOR, atlas_image_size);

	for (int i = 0; i < img_count; ++i)
	{
		spritebatch_internal_integer_image_t* image = images + i;

		if (image->fit)
		{
			const spritebatch_internal_lonely_texture_t* img = imgs + image->img_index;
			spritebatch_internal_get_pixels(sb, img->image_id, img->w, img->h);
			char* pixels = (char*)sb->pixel_buffer;

			spritebatch_v2_t min = image->min;
			spritebatch_v2_t max = image->max;
			int atlas_offset = min.x * pixel_stride;
			int tex_stride = image->size.x * pixel_stride;

			for (int row = min.y, y = 0; row < max.y; ++row, ++y)
			{
				void* row_ptr = (char*)atlas_pixels + (row * atlas_stride + atlas_offset);
				SPRITEBATCH_MEMCPY(row_ptr, pixels + y * tex_stride, tex_stride);
			}
		}
	}

	hashtable_init(&atlas_out->sprites_to_textures, sizeof(spritebatch_internal_texture_t), img_count, sb->mem_ctx);
	atlas_out->texture_id = sb->generate_texture_callback(atlas_pixels, atlas_width, atlas_height, sb->udata);

	// squeeze UVs inward by 128th of a pixel
	// this prevents atlas bleeding. tune as necessary for good results.
	w0 = 1.0f / (float)(atlas_width);
	h0 = 1.0f / (float)(atlas_height);
	div = 1.0f / 128.0f;
	wTol = w0 * div;
	hTol = h0 * div;

	for (int i = 0; i < img_count; ++i)
	{
		spritebatch_internal_integer_image_t* img = images + i;

		if (img->fit)
		{
			spritebatch_v2_t min = img->min;
			spritebatch_v2_t max = img->max;
			volume_used += img->size.x * img->size.y;

			float min_x = (float)min.x * w0 + wTol;
			float min_y = (float)min.y * h0 + hTol;
			float max_x = (float)max.x * w0 - wTol;
			float max_y = (float)max.y * h0 - hTol;

			// flip image on y axis
			if (SPRITEBATCH_ATLAS_FLIP_Y_AXIS_FOR_UV)
			{
				float tmp = min_y;
				min_y = max_y;
				max_y = tmp;
			}

			spritebatch_internal_texture_t texture;
			texture.w = img->size.x;
			texture.h = img->size.y;
			texture.timestamp = 0;
			texture.minx = min_x;
			texture.miny = min_y;
			texture.maxx = max_x;
			texture.maxy = max_y;
			SPRITEBATCH_ASSERT(!(img->size.x < 0));
			SPRITEBATCH_ASSERT(!(img->size.y < 0));
			SPRITEBATCH_ASSERT(!(min_x < 0));
			SPRITEBATCH_ASSERT(!(max_x < 0));
			SPRITEBATCH_ASSERT(!(min_y < 0));
			SPRITEBATCH_ASSERT(!(max_y < 0));
			texture.image_id = imgs[img->img_index].image_id;
			hashtable_insert(&atlas_out->sprites_to_textures, texture.image_id, &texture);
		}
	}

	// Need to adjust atlas_width and atlas_height in config params, as none of the images for this
	// atlas actually fit inside of the atlas! Either adjust the config, or stop sending giant images
	// to the sprite batcher.
	SPRITEBATCH_ASSERT(volume_used > 0);

	atlas_out->volume_ratio = volume_used / (atlas_width * atlas_height);

sb_err:
	// no specific error handling needed here (yet)

	SPRITEBATCH_FREE(atlas_pixels, mem_ctx);
	SPRITEBATCH_FREE(nodes, mem_ctx);
	SPRITEBATCH_FREE(images, mem_ctx);
	SPRITEBATCH_FREE(images_scratch, mem_ctx);
	return;
}

static int spritebatch_internal_lonely_pred(spritebatch_internal_lonely_texture_t* a, spritebatch_internal_lonely_texture_t* b)
{
	return a->timestamp < b->timestamp;
}

static void spritebatch_internal_qsort_lonely(hashtable_t* lonely_table, spritebatch_internal_lonely_texture_t* items, int count)
{
	if (count <= 1) return;

	spritebatch_internal_lonely_texture_t pivot = items[count - 1];
	int low = 0;
	for (int i = 0; i < count - 1; ++i)
	{
		if (spritebatch_internal_lonely_pred(items + i, &pivot))
		{
			hashtable_swap(lonely_table, i, low);
			low++;
		}
	}

	hashtable_swap(lonely_table, low, count - 1);
	spritebatch_internal_qsort_lonely(lonely_table, items, low);
	spritebatch_internal_qsort_lonely(lonely_table, items + low + 1, count - 1 - low);
}

int spritebatch_internal_buffer_key(spritebatch_t* sb, SPRITEBATCH_U64 key)
{
	SPRITEBATCH_CHECK_BUFFER_GROW(sb, key_buffer_count, key_buffer_capacity, key_buffer, SPRITEBATCH_U64);
	sb->key_buffer[sb->key_buffer_count++] = key;
	return 0;
}

void spritebatch_internal_remove_table_entries(spritebatch_t* sb, hashtable_t* table)
{
	for (int i = 0; i < sb->key_buffer_count; ++i) hashtable_remove(table, sb->key_buffer[i]);
	sb->key_buffer_count = 0;
}

void spritebatch_internal_flush_atlas(spritebatch_t* sb, spritebatch_internal_atlas_t* atlas, spritebatch_internal_atlas_t** sentinel, spritebatch_internal_atlas_t** next)
{
	int ticks_to_decay_texture = sb->ticks_to_decay_texture;
	int texture_count = hashtable_count(&atlas->sprites_to_textures);
	spritebatch_internal_texture_t* textures = (spritebatch_internal_texture_t*)hashtable_items(&atlas->sprites_to_textures);

	for (int i = 0; i < texture_count; ++i)
	{
		spritebatch_internal_texture_t* atlas_texture = textures + i;
		if (atlas_texture->timestamp < ticks_to_decay_texture)
		{
			int w = atlas_texture->w;
			int h = atlas_texture->h;
			if (sb->atlas_use_border_pixels)
			{
				w -= 2;
				h -= 2;
			}
			spritebatch_internal_lonely_texture_t* lonely_texture = spritebatch_internal_lonelybuffer_push(sb, atlas_texture->image_id, w, h, 0);
			lonely_texture->timestamp = atlas_texture->timestamp;
		}
		hashtable_remove(&sb->sprites_to_atlases, atlas_texture->image_id);
	}

	if (sb->atlases == atlas)
	{
		if (atlas->next == atlas) sb->atlases = 0;
		else sb->atlases = atlas->prev;
	}

	// handle loop end conditions if sentinel was removed from the chain
	if (sentinel && next)
	{
		if (*sentinel == atlas)
		{
			SPRITEBATCH_LOG("\t\tsentinel was also atlas: %p\n", *sentinel);
			if ((*next)->next != *sentinel)
			{
				SPRITEBATCH_LOG("\t\t*next = (*next)->next : %p = (*next)->%p\n", *next, (*next)->next);
				*next = (*next)->next;
			}

			SPRITEBATCH_LOG("\t\t*sentinel = *next : %p =  %p\n", *sentinel, *next);
 			*sentinel = *next;

		}
	}

	atlas->next->prev = atlas->prev;
	atlas->prev->next = atlas->next;
	hashtable_term(&atlas->sprites_to_textures);
	sb->delete_texture_callback(atlas->texture_id, sb->udata);
	SPRITEBATCH_FREE(atlas, sb->mem_ctx);
}

void spritebatch_internal_log_chain(spritebatch_internal_atlas_t* atlas)
{
	if (atlas)
	{
		spritebatch_internal_atlas_t* sentinel = atlas;
		SPRITEBATCH_LOG("sentinel: %p\n", sentinel);
		do
		{
			spritebatch_internal_atlas_t* next = atlas->next;
			SPRITEBATCH_LOG("\tatlas %p\n", atlas);
			atlas = next;
		}
		while (atlas != sentinel);
	}
}

int spritebatch_defrag(spritebatch_t* sb)
{
	// remove decayed atlases and flush them to the lonely buffer
	// only flush textures that are not decayed
	int ticks_to_decay_texture = sb->ticks_to_decay_texture;
	float ratio_to_decay_atlas = sb->ratio_to_decay_atlas;
	spritebatch_internal_atlas_t* atlas = sb->atlases;
	if (atlas)
	{
		spritebatch_internal_log_chain(atlas);
		spritebatch_internal_atlas_t* sentinel = atlas;
		do
		{
			spritebatch_internal_atlas_t* next = atlas->next;
			int texture_count = hashtable_count(&atlas->sprites_to_textures);
			spritebatch_internal_texture_t* textures = (spritebatch_internal_texture_t*)hashtable_items(&atlas->sprites_to_textures);
			int decayed_texture_count = 0;
			for (int i = 0; i < texture_count; ++i) if (textures[i].timestamp >= ticks_to_decay_texture) decayed_texture_count++;

			float ratio;
			if (!decayed_texture_count) ratio = 0;
			else ratio = (float)texture_count / (float)decayed_texture_count;
			if (ratio > ratio_to_decay_atlas)
			{
				SPRITEBATCH_LOG("flushed atlas %p\n", atlas);
				spritebatch_internal_flush_atlas(sb, atlas, &sentinel, &next);
			}

			atlas = next;
		}
		while (atlas != sentinel);
	}

	// merge mostly empty atlases
	float ratio_to_merge_atlases = sb->ratio_to_merge_atlases;
	atlas = sb->atlases;
	if (atlas)
	{
		int sp = 0;
		spritebatch_internal_atlas_t* merge_stack[2];

		spritebatch_internal_atlas_t* sentinel = atlas;
		do
		{
			spritebatch_internal_atlas_t* next = atlas->next;

			SPRITEBATCH_ASSERT(sp >= 0 && sp <= 2);
			if (sp == 2)
			{
				SPRITEBATCH_LOG("merged 2 atlases\n");
				spritebatch_internal_flush_atlas(sb, merge_stack[0], &sentinel, &next);
				spritebatch_internal_flush_atlas(sb, merge_stack[1], &sentinel, &next);
				sp = 0;
			}

			float ratio = atlas->volume_ratio;
			if (ratio < ratio_to_merge_atlases) merge_stack[sp++] = atlas;

			atlas = next;
		}
		while (atlas != sentinel);

		if (sp == 2)
		{
			SPRITEBATCH_LOG("merged 2 atlases (out of loop)\n");
			spritebatch_internal_flush_atlas(sb, merge_stack[0], 0, 0);
			spritebatch_internal_flush_atlas(sb, merge_stack[1], 0, 0);
		}
	}

	// remove decayed textures from the lonely buffer
	int lonely_buffer_count_till_decay = sb->lonely_buffer_count_till_decay;
	int lonely_count = hashtable_count(&sb->sprites_to_lonely_textures);
	spritebatch_internal_lonely_texture_t* lonely_textures = (spritebatch_internal_lonely_texture_t*)hashtable_items(&sb->sprites_to_lonely_textures);
	if (lonely_count >= lonely_buffer_count_till_decay)
	{
		spritebatch_internal_qsort_lonely(&sb->sprites_to_lonely_textures, lonely_textures, lonely_count);
		int index = 0;
		while (1)
		{
			if (index == lonely_count) break;
			if (lonely_textures[index].timestamp >= ticks_to_decay_texture) break;
			++index;
		}
		for (int i = index; i < lonely_count; ++i)
		{
			SPRITEBATCH_U64 texture_id = lonely_textures[i].texture_id;
			if (texture_id != ~0) sb->delete_texture_callback(texture_id, sb->udata);
			spritebatch_internal_buffer_key(sb, lonely_textures[i].image_id);
			SPRITEBATCH_LOG("lonely texture decayed\n");
		}
		spritebatch_internal_remove_table_entries(sb, &sb->sprites_to_lonely_textures);
		lonely_count -= lonely_count - index;
		SPRITEBATCH_ASSERT(lonely_count == hashtable_count(&sb->sprites_to_lonely_textures));
	}

	// process input, but don't make textures just yet
	spritebatch_internal_process_input(sb, 1);
	lonely_count = hashtable_count(&sb->sprites_to_lonely_textures);

	// while greater than lonely_buffer_count_till_flush elements in lonely buffer
	// grab lonely_buffer_count_till_flush of them and make an atlas
	int lonely_buffer_count_till_flush = sb->lonely_buffer_count_till_flush;
	int stuck = 0;
	while (lonely_count > lonely_buffer_count_till_flush && !stuck)
	{
		atlas = (spritebatch_internal_atlas_t*)SPRITEBATCH_MALLOC(sizeof(spritebatch_internal_atlas_t), sb->mem_ctx);
		if (sb->atlases)
		{
			atlas->prev = sb->atlases;
			atlas->next = sb->atlases->next;
			sb->atlases->next->prev = atlas;
			sb->atlases->next = atlas;
		}

		else
		{
			atlas->next = atlas;
			atlas->prev = atlas;
			sb->atlases = atlas;
		}

		spritebatch_make_atlas(sb, atlas, lonely_textures, lonely_count);
		SPRITEBATCH_LOG("making atlas\n");

		int tex_count_in_atlas = hashtable_count(&atlas->sprites_to_textures);
		if (tex_count_in_atlas != lonely_count)
		{
			int hit_count = 0;
			for (int i = 0; i < lonely_count; ++i)
			{
				SPRITEBATCH_U64 key = lonely_textures[i].image_id;
				if (hashtable_find(&atlas->sprites_to_textures, key))
				{
					spritebatch_internal_buffer_key(sb, key);
					SPRITEBATCH_U64 texture_id = lonely_textures[i].texture_id;
					if (texture_id != ~0) sb->delete_texture_callback(texture_id, sb->udata);
					hashtable_insert(&sb->sprites_to_atlases, key, &atlas);
					SPRITEBATCH_LOG("removing lonely texture for atlas%s\n", texture_id != ~0 ? "" : " (tex was ~0)" );
				}
				else
				{
					hit_count++;
					SPRITEBATCH_ASSERT(lonely_textures[i].w <= sb->atlas_width_in_pixels);
					SPRITEBATCH_ASSERT(lonely_textures[i].h <= sb->atlas_height_in_pixels);
				}
			}
			spritebatch_internal_remove_table_entries(sb, &sb->sprites_to_lonely_textures);

			lonely_count = hashtable_count(&sb->sprites_to_lonely_textures);

			if (!hit_count)
			{
				// TODO
				// handle case where none fit in atlas
				stuck = 1;
				SPRITEBATCH_ASSERT(0);
			}
		}

		else
		{
			for (int i = 0; i < lonely_count; ++i)
			{
				SPRITEBATCH_U64 key = lonely_textures[i].image_id;
				SPRITEBATCH_U64 texture_id = lonely_textures[i].texture_id;
				if (texture_id != ~0) sb->delete_texture_callback(texture_id, sb->udata);
				hashtable_insert(&sb->sprites_to_atlases, key, &atlas);
				SPRITEBATCH_LOG("(fast path) removing lonely texture for atlas%s\n", texture_id != ~0 ? "" : " (tex was ~0)" );
			}
			hashtable_clear(&sb->sprites_to_lonely_textures);
			lonely_count = 0;
			break;
		}
	}

	return 1;
}

#endif // SPRITEBATCH_IMPLEMENTATION_ONCE
#endif // SPRITEBATCH_IMPLEMENTATION

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
