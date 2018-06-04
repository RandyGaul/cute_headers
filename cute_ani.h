/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_ani.h - v1.00

	SUMMARY

		Implements the lower level bits for frame-based, general purpose, looping
		animations. The user is expected to map image ids to their own rendering code.
		For example, in my own personal code I use `CUTE_ANI_U64` like so:

			push_sprite(cute_ani_current_image(ani), transform);

		The idea is to use this header to implement the lower level bits of
		a higher level animation controller abstraction. For example, an
		animation controller can contain an array of tinyany_t animations, and
		depending on user inputs different animations can be drawn at different
		times.

		Animations can be loaded from memory with `cute_ani_load_from_mem`. The
		file format for the animations is one float, followed by one string,
		followed by a newline. Each line represents a frame of animation and
		the time in seconds of duration for the frame. A final `end` token is
		placed at the end of the list of frames. Here's a typical example of
		a three frame walk animation file:

			"data/my images/npcs/bob/walk0.png" 0.25
			"data/my images/npcs/bob/walk1.png" 0.25
			"data/my images/npcs/bob/walk2.png" 0.25
			"end"

		Animation files are stored as readable text. This header does not contain
		any saving functions for animations, since it would require pulling in
		a bunch of extra dependencies. Instead, the file format is kept extremely
		simple making it trivial to serialize animations with any kind of file writer.
		Here is an example writer using <stdio.h> to stdout:

			void print_ani(cute_ani_map_t* map, cute_ani_t* ani)
			{
				for (int i = 0; i < ani->frame_count; ++i) printf("\"%s\" %f\n", cute_ani_map_cstr(map, ani->frames[i].image_id), ani->frames[i].seconds);
				printf("\"end\"\n");
			}

		The string ids associated with each frame of animation do not necessarily
		need to be paths to files. They can be any globally unique string. Strings
		must be wrapped in quotes, and can contain whitespace. The "end" string is
		reserved for internal use.

	Revision history:
		1.0  (05/09/2018) initial release
*/

#if !defined(CUTE_ANI_H)

#define CUTE_ANI_U64 unsigned long long

// Feel free to modify this define as necessary. These frames do not take up much
// memory, so it is quite safe to increase the max frame count.
#define CUTE_ANI_MAX_FRAMES 16

typedef struct cute_ani_frame_t
{
	float seconds;
	CUTE_ANI_U64 image_id;
} cute_ani_frame_t;

typedef struct cute_ani_t
{
	int current_frame;
	float seconds;
	int paused;

	// 0 - no looping (plays animation forwards one frame at a time, then stops).
	// positive - loop forwards by incrementing N frames.
	// negative - loop backwards by decrementing N frames, starting on the last frame.
	// For backwards looping make sure to call `cute_ani_reset` upon initialization to set animation to last frame.
	int looping;

	int frame_count;
	int play_count;
	cute_ani_frame_t frames[CUTE_ANI_MAX_FRAMES];
} cute_ani_t;

/**
 * Resets an animation's timer and current frame. This needs to be called after loading an
 * animation if `ani->looping` has been set to a negative value, in order for the animation
 * to begin on the final frame.
 */
void cute_ani_reset(cute_ani_t* ani);

/**
 * Increments the timer of `ani`. Will increment the current frame based on `ani->looping`.
 * 1 means normal forwards looping, -1 is backwards looping, 0 means play forwards but then stop.
 * Other numbers will skip a number of frames. For example, `ani->looping` as 3 will play forwards
 * and skip two frames after a frame plays.
 */
void cute_ani_update(cute_ani_t* ani, float dt);

/**
 * Sets the current frame of the animation to `frame_index`. Does nothing for out of bounds indices.
 */
void cute_ani_set_frame(cute_ani_t* ani, int frame_index);

/**
 * Returns the id of the current frame's image. This id can be passed to `cute_ani_map_cstr` to
 * retrieve the string associated with the id.
 */
CUTE_ANI_U64 cute_ani_current_image(cute_ani_t* ani);

typedef enum cute_ani_error_t
{
	CUTE_ANI_SUCCESS,
	CUTE_ANI_PREMATURE_END_OF_BUFFER,
	CUTE_ANI_STRING_TOO_LARGE,
	CUTE_ANI_PARSE_ERROR,
} cute_ani_error_t;

typedef struct cute_ani_map_t cute_ani_map_t;

/**
 * Loads an animation from memory. The file format is described at the top of this file.
 */
cute_ani_error_t cute_ani_load_from_mem(cute_ani_map_t* map, cute_ani_t* ani, const void* mem, int size, int* bytes_read);

/**
 * Creates a map object to map integer ids to strings. The strings represent the names of
 * different animation frame images.
 */
cute_ani_map_t* cute_ani_map_create(void* mem_ctx);
void cute_ani_map_destroy(cute_ani_map_t* map);

/**
 * Manually insert a string into the map. This is not necessary if `cute_ani_load_from_mem` is used, since
 * `cute_ani_load_from_mem` will call this function internally.
 */
CUTE_ANI_U64 cute_ani_map_add(cute_ani_map_t* map, const char* unique_image_path);

/**
 * Converts an animation's frame id to a c-style nul-terminated string.
 */
const char* cute_ani_map_cstr(cute_ani_map_t* map, CUTE_ANI_U64 unique_image_id);

#define CUTE_ANI_H
#endif

#if defined(CUTE_ANI_IMPLEMENTATION)
#if !defined(CUTE_ANI_IMPLEMENTATION_ONCE)
#define CUTE_ANI_IMPLEMENTATION_ONCE

#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
	#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#if !defined(CUTE_ANI_STRTOD)
	#include <stdlib.h>
	#define CUTE_ANI_STRTOD strtod
#endif

#if !defined(CUTE_ANI_STRLEN)
	#include <string.h>
	#define CUTE_ANI_STRLEN strlen
#endif

#if !defined(CUTE_ANI_ALLOC)
	#include <stdlib.h>
	#define CUTE_ANI_ALLOC(size, ctx) malloc(size)
	#define CUTE_ANI_FREE(mem, ctx) free(mem)
#endif

#ifndef STRPOOL_EMBEDDED_MALLOC
	#define STRPOOL_EMBEDDED_MALLOC(ctx, size) CUTE_ANI_ALLOC(size, ctx)
#endif

#ifndef STRPOOL_EMBEDDED_FREE
	#define STRPOOL_EMBEDDED_FREE(ctx, ptr) CUTE_ANI_FREE(ptr, ctx)
#endif

#define STRPOOL_EMBEDDED_IMPLEMENTATION

/*
	begin embedding modified strpool.h 
*/

/*
------------------------------------------------------------------------------
          Licensing information can be found at the end of the file.
------------------------------------------------------------------------------

strpool.h - v1.4 - Highly efficient string pool for C/C++.

Do this:
    #define STRPOOL_EMBEDDED_IMPLEMENTATION
before you include this file in *one* C/C++ file to create the implementation.
*/

#ifndef strpool_embedded_h
#define strpool_embedded_h

#ifndef STRPOOL_EMBEDDED_U32
    #define STRPOOL_EMBEDDED_U32 unsigned int
#endif
#ifndef STRPOOL_EMBEDDED_U64
    #define STRPOOL_EMBEDDED_U64 unsigned long long
#endif

typedef struct strpool_embedded_t strpool_embedded_t;

typedef struct strpool_embedded_config_t
    {
    void* memctx;
    int ignore_case;
    int counter_bits;
    int index_bits;
    int entry_capacity;
    int block_capacity;
    int block_size;
    int min_length;
    } strpool_embedded_config_t;

extern strpool_embedded_config_t const strpool_embedded_default_config;

void strpool_embedded_init( strpool_embedded_t* pool, strpool_embedded_config_t const* config );
void strpool_embedded_term( strpool_embedded_t* pool );

STRPOOL_EMBEDDED_U64 strpool_embedded_inject( strpool_embedded_t* pool, char const* string, int length );
char const* strpool_embedded_cstr( strpool_embedded_t const* pool, STRPOOL_EMBEDDED_U64 handle );

#endif /* strpool_embedded_h */

/*
----------------------
    IMPLEMENTATION
----------------------
*/

#ifndef strpool_embedded_impl
#define strpool_embedded_impl

struct strpool_embedded_internal_hash_slot_t;
struct strpool_embedded_internal_entry_t;
struct strpool_embedded_internal_handle_t;
struct strpool_embedded_internal_block_t;

struct strpool_embedded_t
    {
    void* memctx;
    int ignore_case;
    int counter_shift;
    STRPOOL_EMBEDDED_U64 counter_mask;
    STRPOOL_EMBEDDED_U64 index_mask;

    int initial_entry_capacity;
    int initial_block_capacity;
    int block_size;
    int min_data_size;

    struct strpool_embedded_internal_hash_slot_t* hash_table;
    int hash_capacity;

    struct strpool_embedded_internal_entry_t* entries;
    int entry_capacity;
    int entry_count;

    struct strpool_embedded_internal_handle_t* handles;
    int handle_capacity;
    int handle_count;
    int handle_freelist_head;
    int handle_freelist_tail;

    struct strpool_embedded_internal_block_t* blocks;
    int block_capacity;
    int block_count;
    int current_block;
    };


#endif /* strpool_embedded_impl */


#ifdef STRPOOL_EMBEDDED_IMPLEMENTATION
#ifndef STRPOOL_EMBEDDED_IMPLEMENTATION_ONCE
#define STRPOOL_EMBEDDED_IMPLEMENTATION_ONCE

#include <stddef.h>

#ifndef STRPOOL_EMBEDDED_ASSERT
    #include <assert.h>
    #define STRPOOL_EMBEDDED_ASSERT( x ) assert( x )
#endif

#ifndef STRPOOL_EMBEDDED_MEMSET
    #include <string.h>
    #define STRPOOL_EMBEDDED_MEMSET( ptr, val, cnt ) ( memset( ptr, val, cnt ) )
#endif 

#ifndef STRPOOL_EMBEDDED_MEMCPY
    #include <string.h>
    #define STRPOOL_EMBEDDED_MEMCPY( dst, src, cnt ) ( memcpy( dst, src, cnt ) )
#endif 

#ifndef STRPOOL_EMBEDDED_MEMCMP
    #include <string.h>
    #define STRPOOL_EMBEDDED_MEMCMP( pr1, pr2, cnt ) ( memcmp( pr1, pr2, cnt ) )
#endif 

#ifndef STRPOOL_EMBEDDED_STRNICMP
    #ifdef _WIN32
        #include <string.h>
        #define STRPOOL_EMBEDDED_STRNICMP( s1, s2, len ) ( strnicmp( s1, s2, len ) )
    #else
        #include <string.h>
        #define STRPOOL_EMBEDDED_STRNICMP( s1, s2, len ) ( strncasecmp( s1, s2, len ) )        
    #endif
#endif 

#ifndef STRPOOL_EMBEDDED_MALLOC
    #include <stdlib.h>
    #define STRPOOL_EMBEDDED_MALLOC( ctx, size ) ( malloc( size ) )
    #define STRPOOL_EMBEDDED_FREE( ctx, ptr ) ( free( ptr ) )
#endif


typedef struct strpool_embedded_internal_hash_slot_t
    {
    STRPOOL_EMBEDDED_U32 hash_key;
    int entry_index;
    int base_count;
    } strpool_embedded_internal_hash_slot_t;


typedef struct strpool_embedded_internal_entry_t
    {
    int hash_slot;
    int handle_index;
    char* data;
    int size;
    int length;
    int refcount;
    } strpool_embedded_internal_entry_t;


typedef struct strpool_embedded_internal_handle_t
    {
    int entry_index;
    int counter;
    } strpool_embedded_internal_handle_t;


typedef struct strpool_embedded_internal_block_t
    {
    int capacity;
    char* data;
    char* tail;
    int free_list;
    } strpool_embedded_internal_block_t;


typedef struct strpool_embedded_internal_free_block_t
    {
    int size;
    int next;
    } strpool_embedded_internal_free_block_t;


strpool_embedded_config_t const strpool_embedded_default_config = 
    { 
    /* memctx         = */ 0,
    /* ignore_case    = */ 0,
    /* counter_bits   = */ 32,
    /* index_bits     = */ 32,
    /* entry_capacity = */ 4096, 
    /* block_capacity = */ 32, 
    /* block_size     = */ 256 * 1024, 
    /* min_length     = */ 23,
    };



static STRPOOL_EMBEDDED_U32 strpool_embedded_internal_pow2ceil( STRPOOL_EMBEDDED_U32 v )
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


static int strpool_embedded_internal_add_block( strpool_embedded_t* pool, int size )
    {
    if( pool->block_count >= pool->block_capacity ) 
        {
        pool->block_capacity *= 2;
        strpool_embedded_internal_block_t* new_blocks = (strpool_embedded_internal_block_t*) STRPOOL_EMBEDDED_MALLOC( pool->memctx, 
            pool->block_capacity * sizeof( *pool->blocks ) );
        STRPOOL_EMBEDDED_ASSERT( new_blocks );
        STRPOOL_EMBEDDED_MEMCPY( new_blocks, pool->blocks, pool->block_count * sizeof( *pool->blocks ) );
        STRPOOL_EMBEDDED_FREE( pool->memctx, pool->blocks );
        pool->blocks = new_blocks;
        }
    pool->blocks[ pool->block_count ].capacity = size;
    pool->blocks[ pool->block_count ].data = (char*) STRPOOL_EMBEDDED_MALLOC( pool->memctx, (size_t) size );
    STRPOOL_EMBEDDED_ASSERT( pool->blocks[ pool->block_count ].data );
    pool->blocks[ pool->block_count ].tail = pool->blocks[ pool->block_count ].data;
    pool->blocks[ pool->block_count ].free_list = -1;
    return pool->block_count++;
    }


void strpool_embedded_init( strpool_embedded_t* pool, strpool_embedded_config_t const* config )
    {
    if( !config ) config = &strpool_embedded_default_config;

    pool->memctx = config->memctx;
    pool->ignore_case = config->ignore_case;

    STRPOOL_EMBEDDED_ASSERT( config->counter_bits + config->index_bits <= 64 );
    pool->counter_shift = config->index_bits;
    pool->counter_mask = ( 1ULL << (STRPOOL_EMBEDDED_U64) config->counter_bits ) - 1;
    pool->index_mask = ( 1ULL << (STRPOOL_EMBEDDED_U64) config->index_bits ) - 1;

    pool->initial_entry_capacity = 
        (int) strpool_embedded_internal_pow2ceil( config->entry_capacity > 1 ? (STRPOOL_EMBEDDED_U32)config->entry_capacity : 2U );
    pool->initial_block_capacity = 
        (int) strpool_embedded_internal_pow2ceil( config->block_capacity > 1 ? (STRPOOL_EMBEDDED_U32)config->block_capacity : 2U );
    pool->block_size = 
        (int) strpool_embedded_internal_pow2ceil( config->block_size > 256 ? (STRPOOL_EMBEDDED_U32)config->block_size : 256U );
    pool->min_data_size = 
        (int) ( sizeof( int ) * 2 + 1 + ( config->min_length > 8 ? (STRPOOL_EMBEDDED_U32)config->min_length : 8U ) );

    pool->hash_capacity = pool->initial_entry_capacity * 2;
    pool->entry_capacity = pool->initial_entry_capacity;
    pool->handle_capacity = pool->initial_entry_capacity;
    pool->block_capacity = pool->initial_block_capacity;    

    pool->handle_freelist_head = -1;
    pool->handle_freelist_tail = -1;
    pool->block_count = 0;
    pool->handle_count = 0;
    pool->entry_count = 0;
    
    pool->hash_table = (strpool_embedded_internal_hash_slot_t*) STRPOOL_EMBEDDED_MALLOC( pool->memctx, 
        pool->hash_capacity * sizeof( *pool->hash_table ) );
    STRPOOL_EMBEDDED_ASSERT( pool->hash_table );
    STRPOOL_EMBEDDED_MEMSET( pool->hash_table, 0, pool->hash_capacity * sizeof( *pool->hash_table ) );
    pool->entries = (strpool_embedded_internal_entry_t*) STRPOOL_EMBEDDED_MALLOC( pool->memctx, 
        pool->entry_capacity * sizeof( *pool->entries ) );
    STRPOOL_EMBEDDED_ASSERT( pool->entries );
    pool->handles = (strpool_embedded_internal_handle_t*) STRPOOL_EMBEDDED_MALLOC( pool->memctx, 
        pool->handle_capacity * sizeof( *pool->handles ) );
    STRPOOL_EMBEDDED_ASSERT( pool->handles );
    pool->blocks = (strpool_embedded_internal_block_t*) STRPOOL_EMBEDDED_MALLOC( pool->memctx, 
        pool->block_capacity * sizeof( *pool->blocks ) );
    STRPOOL_EMBEDDED_ASSERT( pool->blocks );

    pool->current_block = strpool_embedded_internal_add_block( pool, pool->block_size );
    }


void strpool_embedded_term( strpool_embedded_t* pool )
    {
    for( int i = 0; i < pool->block_count; ++i ) STRPOOL_EMBEDDED_FREE( pool->memctx, pool->blocks[ i ].data );
    STRPOOL_EMBEDDED_FREE( pool->memctx, pool->blocks );         
    STRPOOL_EMBEDDED_FREE( pool->memctx, pool->handles );            
    STRPOOL_EMBEDDED_FREE( pool->memctx, pool->entries );            
    STRPOOL_EMBEDDED_FREE( pool->memctx, pool->hash_table );         
    }


static STRPOOL_EMBEDDED_U64 strpool_embedded_internal_make_handle( int index, int counter, STRPOOL_EMBEDDED_U64 index_mask, int counter_shift, 
    STRPOOL_EMBEDDED_U64 counter_mask )
    {
    STRPOOL_EMBEDDED_U64 i = (STRPOOL_EMBEDDED_U64) ( index + 1 );
    STRPOOL_EMBEDDED_U64 c = (STRPOOL_EMBEDDED_U64) counter;
    return ( ( c & counter_mask ) << counter_shift ) | ( i & index_mask );
    }


static int strpool_embedded_internal_counter_from_handle( STRPOOL_EMBEDDED_U64 handle, int counter_shift, STRPOOL_EMBEDDED_U64 counter_mask  )
    {
    return (int) ( ( handle >> counter_shift ) & counter_mask ) ;
    }
    

static int strpool_embedded_internal_index_from_handle( STRPOOL_EMBEDDED_U64 handle, STRPOOL_EMBEDDED_U64 index_mask )
    {
    return ( (int) ( handle & index_mask ) ) - 1;
    }


static strpool_embedded_internal_entry_t* strpool_embedded_internal_get_entry( strpool_embedded_t const* pool, STRPOOL_EMBEDDED_U64 handle )
    {
    int index = strpool_embedded_internal_index_from_handle( handle, pool->index_mask );
    int counter = strpool_embedded_internal_counter_from_handle( handle, pool->counter_shift, pool->counter_mask );

    if( index >= 0 && index < pool->handle_count && 
        counter == (int) ( pool->handles[ index ].counter & pool->counter_mask ) )
            return &pool->entries[ pool->handles[ index ].entry_index ];

    return 0;
    }


static STRPOOL_EMBEDDED_U32 strpool_embedded_internal_find_in_blocks( strpool_embedded_t const* pool, char const* string, int length )
    {
    for( int i = 0; i < pool->block_count; ++i )
        {
        strpool_embedded_internal_block_t* block = &pool->blocks[ i ];
        // Check if string comes from pool
        if( string >= block->data + 2 * sizeof( STRPOOL_EMBEDDED_U32 ) && string < block->data + block->capacity ) 
            {
            STRPOOL_EMBEDDED_U32* ptr = (STRPOOL_EMBEDDED_U32*) string;
            int stored_length = (int)( *( ptr - 1 ) ); // Length is stored immediately before string
            if( stored_length != length || string[ length ] != '\0' ) return 0; // Invalid string
            STRPOOL_EMBEDDED_U32 hash = *( ptr - 2 ); // Hash is stored before the length field
            return hash;
            }
        }

    return 0;
    }


static STRPOOL_EMBEDDED_U32 strpool_embedded_internal_calculate_hash( char const* string, int length, int ignore_case )
    {
    STRPOOL_EMBEDDED_U32 hash = 5381U; 

    if( ignore_case) 
        {
        for( int i = 0; i < length; ++i )
            {
            char c = string[ i ];
            c = ( c <= 'z' && c >= 'a' ) ? c - ( 'a' - 'A' ) : c;
            hash = ( ( hash << 5U ) + hash) ^ c;
            }
        }
    else
        {
        for( int i = 0; i < length; ++i )
            {
            char c = string[ i ];
            hash = ( ( hash << 5U ) + hash) ^ c;
            }
        }

    hash = ( hash == 0 ) ? 1 : hash; // We can't allow 0-value hash keys, but dupes are ok
    return hash;
    }


static void strpool_embedded_internal_expand_hash_table( strpool_embedded_t* pool )
    {
    int old_capacity = pool->hash_capacity;
    strpool_embedded_internal_hash_slot_t* old_table = pool->hash_table;

    pool->hash_capacity *= 2;

    pool->hash_table = (strpool_embedded_internal_hash_slot_t*) STRPOOL_EMBEDDED_MALLOC( pool->memctx, 
        pool->hash_capacity * sizeof( *pool->hash_table ) );
    STRPOOL_EMBEDDED_ASSERT( pool->hash_table );
    STRPOOL_EMBEDDED_MEMSET( pool->hash_table, 0, pool->hash_capacity * sizeof( *pool->hash_table ) );

    for( int i = 0; i < old_capacity; ++i )
        {
        STRPOOL_EMBEDDED_U32 hash_key = old_table[ i ].hash_key;
        if( hash_key )
            {
            int base_slot = (int)( hash_key & (STRPOOL_EMBEDDED_U32)( pool->hash_capacity - 1 ) );
            int slot = base_slot;
            while( pool->hash_table[ slot ].hash_key )
                slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );
            STRPOOL_EMBEDDED_ASSERT( hash_key );
            pool->hash_table[ slot ].hash_key = hash_key;
            pool->hash_table[ slot ].entry_index = old_table[ i ].entry_index;  
            pool->entries[ pool->hash_table[ slot ].entry_index ].hash_slot = slot; 
            ++pool->hash_table[ base_slot ].base_count;
            }               
        }

    STRPOOL_EMBEDDED_FREE( pool->memctx, old_table );
    }


static void strpool_embedded_internal_expand_entries( strpool_embedded_t* pool )
    {
    pool->entry_capacity *= 2;
    strpool_embedded_internal_entry_t* new_entries = (strpool_embedded_internal_entry_t*) STRPOOL_EMBEDDED_MALLOC( pool->memctx, 
        pool->entry_capacity * sizeof( *pool->entries ) );
    STRPOOL_EMBEDDED_ASSERT( new_entries );
    STRPOOL_EMBEDDED_MEMCPY( new_entries, pool->entries, pool->entry_count * sizeof( *pool->entries ) );
    STRPOOL_EMBEDDED_FREE( pool->memctx, pool->entries );
    pool->entries = new_entries;    
    }


static void strpool_embedded_internal_expand_handles( strpool_embedded_t* pool )
    {
    pool->handle_capacity *= 2;
    strpool_embedded_internal_handle_t* new_handles = (strpool_embedded_internal_handle_t*) STRPOOL_EMBEDDED_MALLOC( pool->memctx, 
        pool->handle_capacity * sizeof( *pool->handles ) );
    STRPOOL_EMBEDDED_ASSERT( new_handles );
    STRPOOL_EMBEDDED_MEMCPY( new_handles, pool->handles, pool->handle_count * sizeof( *pool->handles ) );
    STRPOOL_EMBEDDED_FREE( pool->memctx, pool->handles );
    pool->handles = new_handles;
    }


static char* strpool_embedded_internal_get_data_storage( strpool_embedded_t* pool, int size, int* alloc_size )
    {
    if( size < (int)sizeof( strpool_embedded_internal_free_block_t ) ) size = sizeof( strpool_embedded_internal_free_block_t );
    if( size < pool->min_data_size ) size = pool->min_data_size;
    size = (int)strpool_embedded_internal_pow2ceil( (STRPOOL_EMBEDDED_U32)size );
    
    // Try to find a large enough free slot in existing blocks
    for( int i = 0; i < pool->block_count; ++i )
        {
        int free_list = pool->blocks[ i ].free_list;
        int prev_list = -1;
        while( free_list >= 0 )
            {
            strpool_embedded_internal_free_block_t* free_entry = 
                (strpool_embedded_internal_free_block_t*) ( pool->blocks[ i ].data + free_list );
            if( free_entry->size / 2 < size ) 
                {
                // At this point, all remaining slots are too small, so bail out if the current slot is not large enough
                if( free_entry->size < size ) break; 

                if( prev_list < 0 )
                    {
                    pool->blocks[ i ].free_list = free_entry->next;         
                    }
                else
                    {
                    strpool_embedded_internal_free_block_t* prev_entry = 
                        (strpool_embedded_internal_free_block_t*) ( pool->blocks[ i ].data + prev_list );
                    prev_entry->next = free_entry->next;
                    }
                *alloc_size = free_entry->size;
                return (char*) free_entry;
                }
            prev_list = free_list;
            free_list = free_entry->next;
            }
        }

    // Use current block, if enough space left
    int offset = (int) ( pool->blocks[ pool->current_block ].tail - pool->blocks[ pool->current_block ].data );
    if( size <= pool->blocks[ pool->current_block ].capacity - offset )
        {
        char* data = pool->blocks[ pool->current_block ].tail;
        pool->blocks[ pool->current_block ].tail += size;
        *alloc_size = size;
        return data;
        }

    // Allocate a new block
    pool->current_block = strpool_embedded_internal_add_block( pool, size > pool->block_size ? size : pool->block_size );
    char* data = pool->blocks[ pool->current_block ].tail;
    pool->blocks[ pool->current_block ].tail += size;
    *alloc_size = size;
    return data;
    }
    

STRPOOL_EMBEDDED_U64 strpool_embedded_inject( strpool_embedded_t* pool, char const* string, int length )
    {
    if( !string || length < 0 ) return 0;

    STRPOOL_EMBEDDED_U32 hash = strpool_embedded_internal_find_in_blocks( pool, string, length );
    // If no stored hash, calculate it from data
    if( !hash ) hash = strpool_embedded_internal_calculate_hash( string, length, pool->ignore_case ); 

    // Return handle to existing string, if it is already in pool
    int base_slot = (int)( hash & (STRPOOL_EMBEDDED_U32)( pool->hash_capacity - 1 ) );
    int base_count = pool->hash_table[ base_slot ].base_count;
    int slot = base_slot;
    int first_free = slot;
    while( base_count > 0 )
        {
        STRPOOL_EMBEDDED_U32 slot_hash = pool->hash_table[ slot ].hash_key;
        if( slot_hash == 0 && pool->hash_table[ first_free ].hash_key != 0 ) first_free = slot;
        int slot_base = (int)( slot_hash & (STRPOOL_EMBEDDED_U32)( pool->hash_capacity - 1 ) );
        if( slot_base == base_slot ) 
            {
            STRPOOL_EMBEDDED_ASSERT( base_count > 0 );
            --base_count;
            if( slot_hash == hash )
                {
                int index = pool->hash_table[ slot ].entry_index;
                strpool_embedded_internal_entry_t* entry = &pool->entries[ index ];
                if( entry->length == length && 
                    ( 
                       ( !pool->ignore_case &&   STRPOOL_EMBEDDED_MEMCMP( entry->data + 2 * sizeof( STRPOOL_EMBEDDED_U32 ), string, (size_t)length ) == 0 )
                    || (  pool->ignore_case && STRPOOL_EMBEDDED_STRNICMP( entry->data + 2 * sizeof( STRPOOL_EMBEDDED_U32 ), string, (size_t)length ) == 0 ) 
                    ) 
                  )
                    {
                    int handle_index = entry->handle_index;
                    return strpool_embedded_internal_make_handle( handle_index, pool->handles[ handle_index ].counter, 
                        pool->index_mask, pool->counter_shift, pool->counter_mask );
                    }
                }
            }
        slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );
        }   

    // This is a new string, so let's add it

    if( pool->entry_count >= ( pool->hash_capacity  - pool->hash_capacity / 3 ) )
        {
        strpool_embedded_internal_expand_hash_table( pool );

        base_slot = (int)( hash & (STRPOOL_EMBEDDED_U32)( pool->hash_capacity - 1 ) );
        slot = base_slot;
        first_free = slot;
        while( base_count )
            {
            STRPOOL_EMBEDDED_U32 slot_hash = pool->hash_table[ slot ].hash_key;
            if( slot_hash == 0 && pool->hash_table[ first_free ].hash_key != 0 ) first_free = slot;
            int slot_base = (int)( slot_hash & (STRPOOL_EMBEDDED_U32)( pool->hash_capacity - 1 ) );
            if( slot_base == base_slot )  --base_count;
            slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );
            }       
        }
        
    slot = first_free;
    while( pool->hash_table[ slot ].hash_key )
        slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );

    if( pool->entry_count >= pool->entry_capacity )
        strpool_embedded_internal_expand_entries( pool );

    STRPOOL_EMBEDDED_ASSERT( !pool->hash_table[ slot ].hash_key && ( hash & ( (STRPOOL_EMBEDDED_U32) pool->hash_capacity - 1 ) ) == (STRPOOL_EMBEDDED_U32) base_slot );
    STRPOOL_EMBEDDED_ASSERT( hash );
    pool->hash_table[ slot ].hash_key = hash;
    pool->hash_table[ slot ].entry_index = pool->entry_count;
    ++pool->hash_table[ base_slot ].base_count;

    int handle_index;

    if( pool->handle_count < pool->handle_capacity )
        {
        handle_index = pool->handle_count;
        pool->handles[ pool->handle_count ].counter = 1;
        ++pool->handle_count;           
        }
    else if( pool->handle_freelist_head >= 0 )
        {
        handle_index = pool->handle_freelist_head;
        if( pool->handle_freelist_tail == pool->handle_freelist_head ) 
            pool->handle_freelist_tail = pool->handles[ pool->handle_freelist_head ].entry_index;
        pool->handle_freelist_head = pool->handles[ pool->handle_freelist_head ].entry_index;                       
        }
    else
        {
        strpool_embedded_internal_expand_handles( pool );
        handle_index = pool->handle_count;
        pool->handles[ pool->handle_count ].counter = 1;
        ++pool->handle_count;           
        }

    pool->handles[ handle_index ].entry_index = pool->entry_count;
        
    strpool_embedded_internal_entry_t* entry = &pool->entries[ pool->entry_count ];
    ++pool->entry_count;
        
    int data_size = length + 1 + (int) ( 2 * sizeof( STRPOOL_EMBEDDED_U32 ) );
    char* data = strpool_embedded_internal_get_data_storage( pool, data_size, &data_size );
    entry->hash_slot = slot;
    entry->handle_index = handle_index;
    entry->data = data;
    entry->size = data_size;
    entry->length = length;
    entry->refcount = 0;

    *(STRPOOL_EMBEDDED_U32*)(data) = hash;
    data += sizeof( STRPOOL_EMBEDDED_U32 );
    *(STRPOOL_EMBEDDED_U32*)(data) = (STRPOOL_EMBEDDED_U32) length;
    data += sizeof( STRPOOL_EMBEDDED_U32 );
    STRPOOL_EMBEDDED_MEMCPY( data, string, (size_t) length ); 
    data[ length ] = 0; // Ensure trailing zero

    return strpool_embedded_internal_make_handle( handle_index, pool->handles[ handle_index ].counter, pool->index_mask, 
        pool->counter_shift, pool->counter_mask );
    }


char const* strpool_embedded_cstr( strpool_embedded_t const* pool, STRPOOL_EMBEDDED_U64 handle )
    {
    strpool_embedded_internal_entry_t const* entry = strpool_embedded_internal_get_entry( pool, handle );
    if( entry ) return entry->data + 2 * sizeof( STRPOOL_EMBEDDED_U32 ); // Skip leading hash value
    return NULL;
    }

#endif // STRPOOL_EMBEDDED_IMPLEMENTATION_ONCE
#endif /* STRPOOL_EMBEDDED_IMPLEMENTATION */


/*
revision history:
    1.4     fixed find_in_blocks substring bug, removed realloc, added docs
    1.3     fixed typo in mask bit shift
    1.2     made it possible to override standard library functions
    1.1     added is_valid function to query a handles validity
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

/*
	end embedding strpool.h
*/

void cute_ani_reset(cute_ani_t* ani)
{
	ani->play_count = 0;
	ani->seconds = 0;
	if (ani->looping >= 0) ani->current_frame = 0;
	else ani->current_frame = ani->frame_count - 1;
}

int cute_ani_mod(int a, int b)
{
	int r = a % b;
	return r * b < 0 ? r + b : r;
}

void cute_ani_update(cute_ani_t* ani, float dt)
{
	if (ani->paused) return;
	int current_frame = ani->current_frame;
	cute_ani_frame_t* frame = ani->frames + current_frame;
	if (ani->play_count && !ani->looping) return;

	if (frame->seconds <= ani->seconds)
	{
		int next_frame;

		if (ani->looping)
		{
			// Calculate next frame.
			next_frame = cute_ani_mod(current_frame + ani->looping, ani->frame_count);

			// Increment play count.
			int sign = ani->looping >= 0 ? 1 : -1;
			if (ani->looping * sign >= ani->frame_count)
			{
				ani->play_count += ani->looping / ani->frame_count;
			}

			else if (ani->looping > 0 && next_frame < current_frame)
			{
				ani->play_count += 1;
			}

			else if (ani->looping < 0 && next_frame > current_frame)
			{
				ani->play_count += 1;
			}
		}

		else
		{
			if (current_frame + 1 == ani->frame_count)
			{
				next_frame = current_frame;
				ani->play_count += 1;
			}

			else
			{
				next_frame = current_frame + 1;
			}
		}

		ani->current_frame = next_frame;
		ani->seconds = 0;
	}

	else
	{
		ani->seconds += dt;
	}
}

void cute_ani_set_frame(cute_ani_t* ani, int frame_index)
{
	int frame = ani->current_frame;
	if (!(frame >= 0 && frame < ani->frame_count)) return;
	ani->current_frame = frame;
	ani->seconds = 0;
}

CUTE_ANI_U64 cute_ani_current_image(cute_ani_t* ani)
{
	return ani->frames[ani->current_frame].image_id;
}

#define CUTE_ANI_INTERNAL_BUFFER_MAX 1024

typedef struct cute_ani_parse_t
{
	const char* in;
	const char* end;
	int scratch_len;
	char scratch[CUTE_ANI_INTERNAL_BUFFER_MAX];
} cute_ani_parse_t;

#define CUTE_ANI_CHECK(X, Y) do { if (!(X)) { error_code = Y; goto cute_ani_end; } } while (0)
#define CUTE_ANI_FAIL_IF(X) do { if (X) { goto cute_ani_end; } } while (0)

static int cute_ani_isspace(char c)
{
	return (c == ' ') |
		(c == '\t') |
		(c == '\n') |
		(c == '\v') |
		(c == '\f') |
		(c == '\r');
}

static cute_ani_error_t cute_ani_next_internal(cute_ani_parse_t* p, char* c)
{
	cute_ani_error_t error_code = CUTE_ANI_SUCCESS;
	CUTE_ANI_CHECK(p->in < p->end, CUTE_ANI_PREMATURE_END_OF_BUFFER);
	while (cute_ani_isspace(*c = *p->in++)) CUTE_ANI_CHECK(p->in < p->end, CUTE_ANI_PREMATURE_END_OF_BUFFER);

cute_ani_end:
	return error_code;
}

#define cute_ani_next(p, c) \
	do { \
		CUTE_ANI_FAIL_IF(cute_ani_next_internal(p, c) != CUTE_ANI_SUCCESS); \
	} while (0)

static char cute_ani_parse_char(char c)
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

#define cute_ani_expect(p, expect) \
	do { \
		char cute_ani_char; \
		cute_ani_next(p, &cute_ani_char); \
		CUTE_ANI_CHECK(cute_ani_char == expect, CUTE_ANI_PARSE_ERROR); \
	} while (0)

static cute_ani_error_t cute_ani_read_string_internal(cute_ani_parse_t* p)
{
	cute_ani_error_t error_code = CUTE_ANI_SUCCESS;
	int count = 0;
	int done = 0;
	cute_ani_expect(p, '"');

	while (!done)
	{
		CUTE_ANI_CHECK(count < CUTE_ANI_INTERNAL_BUFFER_MAX, CUTE_ANI_STRING_TOO_LARGE);
		char c;
		cute_ani_next(p, &c);

		switch (c)
		{
		case '"':
			p->scratch[count] = 0;
			done = 1;
			break;

		case '\\':
		{
			char the_char;
			cute_ani_next(p, &the_char);
			the_char = cute_ani_parse_char(the_char);
			p->scratch[count++] = the_char;
		}	break;

		default:
			p->scratch[count++] = c;
			break;
		}
	}

	p->scratch_len = count;

cute_ani_end:
	return error_code;
}

#define cute_ani_read_string(p) \
	do { \
		CUTE_ANI_FAIL_IF(cute_ani_read_string_internal(p) != CUTE_ANI_SUCCESS); \
	} while (0)

static int cute_ani_read_float_internal(cute_ani_parse_t* p, float* out)
{
	cute_ani_error_t error_code = CUTE_ANI_SUCCESS;
	char* end;
	float val = (float)CUTE_ANI_STRTOD(p->in, &end);
	CUTE_ANI_CHECK(p->in != end, CUTE_ANI_PARSE_ERROR);
	p->in = end;
	*out = val;

cute_ani_end:
	return error_code;
}

#define cute_ani_read_float(p, num) \
	do { \
		CUTE_ANI_FAIL_IF(cute_ani_read_float_internal(p, num) != CUTE_ANI_SUCCESS); \
	} while (0)

struct cute_ani_map_t
{
	void* mem_ctx;
	CUTE_ANI_U64 end_id;
	strpool_embedded_t pool;
};

cute_ani_error_t cute_ani_load_from_mem(cute_ani_map_t* map, cute_ani_t* ani, const void* mem, int size, int* bytes_read)
{
	cute_ani_error_t error_code = CUTE_ANI_SUCCESS;
	cute_ani_parse_t parse;
	cute_ani_parse_t* p = &parse;
	p->in = (const char*)mem;
	p->end = p->in + size;

	ani->current_frame = 0;
	ani->seconds = 0;
	ani->paused = 0;
	ani->looping = 1;
	ani->frame_count = 0;
	ani->play_count = 0;

	int frame_count = 0;
	while (1)
	{
		cute_ani_read_string(p);
		CUTE_ANI_U64 id = strpool_embedded_inject(&map->pool, p->scratch, p->scratch_len);
		if (id == map->end_id) break;

		float frame_time_in_seconds;
		cute_ani_read_float(p, &frame_time_in_seconds);

		cute_ani_frame_t frame;
		frame.image_id = id;
		frame.seconds = frame_time_in_seconds;
		ani->frames[frame_count] = frame;

		CUTE_ANI_CHECK(frame_count < CUTE_ANI_MAX_FRAMES, CUTE_ANI_PARSE_ERROR);
		++frame_count;
	}

	ani->frame_count = frame_count;
	if (bytes_read) *bytes_read = p->end - (const char*)mem;

cute_ani_end:
	return error_code;
}

cute_ani_map_t* cute_ani_map_create(void* mem_ctx)
{
	cute_ani_map_t* map = (cute_ani_map_t*)CUTE_ANI_ALLOC(sizeof(cute_ani_map_t), mem_ctx);
	map->mem_ctx = mem_ctx;
	strpool_embedded_config_t config = strpool_embedded_default_config;
	config.memctx = mem_ctx;
	strpool_embedded_init(&map->pool, &config);
	map->end_id = cute_ani_map_add(map, "end");
	return map;
}

CUTE_ANI_U64 cute_ani_map_add(cute_ani_map_t* map, const char* unique_image_path)
{
	return strpool_embedded_inject(&map->pool, unique_image_path, CUTE_ANI_STRLEN(unique_image_path));
}

const char* cute_ani_map_cstr(cute_ani_map_t* map, CUTE_ANI_U64 unique_image_id)
{
	return strpool_embedded_cstr(&map->pool, unique_image_id);
}

void cute_ani_map_destroy(cute_ani_map_t* map)
{
	strpool_embedded_term(&map->pool);
	CUTE_ANI_FREE(map, map->mem_ctx);
}

#endif // CUTE_ANI_IMPLEMENTATION_ONCE
#endif // CUTE_ANI_IMPLEMENTATION

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
