/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_filewatch.h - v1.01

	To create implementation (the function definitions)
		#define CUTE_FILEWATCH_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY:

		This header wraps the wonderful assetsys.h header by Mattias Gustavsson and
		provides notifications when files/folders in a watched directory are created,
		removed, or modified. These notifications are especially useful for perform-
		ing asset hotloading or live reloading.

		The reason assetsys.h is used, is to provide access to virtual paths. assetsys.h
		works by mounting a directory or zip file under a virtual path. The rest of the
		application can interface through the virtual path via assetsys. This abstraction
		is important to decouple in-game file paths from on-disk paths. In this way
		zipped up archives can replace typical files and folders once an application is
		ready to ship, without changing any code.

		assetsys.h much be included *before* this header. See here for a copy of assetsys:
		https://github.com/mattiasgustavsson/libs/blob/master/assetsys.h

	MULTITHREADING:

		This header was designed with multi-threaded usecases in-mind, but does no synch-
		ronization itself. It is recommended to call `filewatch_update` from a separate
		thread periodically. It is recommended to sleep this thread, instead of running
		a hot infinite loop. `filewatch_update` will poll watched directories and queue
		up notifications internally.

		Then, from another thread (probably the main thread), `filewatch_notify` can be
		called. This function will dispatch all queued up notifications via user call-
		backs.

		All calls to all filewatch functions should be placed behind a lock. Here's an
		example of a separate polling thread function:

			int poll_watches(void* udata)
			{
				filewatch_t* filewatch = (filewatch_t*)udata;
				while (running)
				{
					your_acquire_lock_function(&your_filewatch_lock);
					int ret = filewatch_update(filewatch);
					your_unlock_function(&your_filewatch_lock);
					if (!ret) return YOUR_ERROR_CODE;
					your_sleep_function(YOUR_SLEEP_COUNT);
				}
				return YOUR_SUCCESS_CODE;
			}

		Similarly, somewhere (like the main thread) can run `filewatch_notify` behind
		a lock.

			your_acquire_lock_function(&your_filewatch_lock);
			filewatch_notify(filewatch);
			your_unlock_function(&your_filewatch_lock);

	CUSTOMIZATION:

		The following macros can be defined before including this header with the
		CUTE_FILEWATCH_IMPLEMENTATION symbol defined, in order to customize the internal
		behavior of cute_spritebatch.h. Search this header to find how each macro is
		defined and used. Note that MALLOC/FREE functions can optionally take a context
		parameter for custom allocation.

		CUTE_FILEWATCH_MALLOC
		CUTE_FILEWATCH_MEMCPY
		CUTE_FILEWATCH_MEMSET
		CUTE_FILEWATCH_ASSERT
		CUTE_FILEWATCH_STRLEN

	EMBEDDED LIBRARIES IN THIS HEADER:

		This header uses some lower level libraries embedded directory inside cute_filewatch.
		Here they are:

		* cute_files.h by Randy Gaul
		* strpool.h (modified) by Mattias Gustavsson
		* hashtable.h by Mattias Gustavsson

		Each embedded header has two license options: zlib and public domain. Licenses are
		attached at the end of each embedded header.

	CREDIT:

		Special thanks to Mattias Gustavsson for collaborating on the initial work-in-progress
		version of this header, and for his wonderful assetsys.h header.

	Revision history:
		1.00 (04/17/2018) initial release
		1.01 (05/07/2017) bugfixes by dseyedm
*/

/*
	Contributors:
		dseyedm           1.01 - various bugfixes
*/

#ifndef CUTE_FILEWATCH_H

/**
 * Read this in the event a function returns 0 signifying an error occurred. This string provides
 * somewhat detailed error reporting.
 */
extern const char* filewatch_error_reason;

typedef struct filewatch_t filewatch_t;

/**
 * Creates the filewatch system. `mem_ctx` can be NULL, and is used for custom allocation. See
 * `CUTE_FILEWATCH_MALLOC` for setting up custom allocations. `assetsys` must be a valid pointer
 * to an initialized assetsys.h system.
 */
filewatch_t* filewatch_create(struct assetsys_t* assetsys, void* mem_ctx);

/**
 * Frees up all memory and shuts down the `filewatch`.
 */
void filewatch_free(filewatch_t* filewatch);

/**
 * Loads up a directory for watching. Also mounts `assetsys` provided by the `filewatch_create`
 * function. In this way, `filewatch_mount` becomes the temporary replacement of `assetsys_mount`.
 */
int filewatch_mount(filewatch_t* filewatch, const char* actual_path, const char* mount_as_virtual_path);

/**
 * Unmounts a directory and stops watches on this directory. Also unmounts the underlying
 * `assetsys` mount.
 */
void filewatch_dismount(filewatch_t* filewatch);

/**
 * Searches all watched directories and stores up notifications internally. This function is often
 * stuck into a separate thread to wake periodically and scan the watched directories. Does not do
 * any notification reporting, and instead only queues up notifications.
 */
int filewatch_update(filewatch_t* filewatch);

/**
 * Looks for internally queued up notifications and then calls them. The notifications call user
 * supplied callbacks. Typically this will be called from the main thread, while `filewatch_update`
 * is called from a separate thread. Please be sure to acquire a lock before calling any filewatch
 * functions when using cute_filewatch in a multi-threaded environment.
 */
void filewatch_notify(filewatch_t* filewatch);

/**
 * Describes all kinds of relevant changes in a watched directory.
 */
typedef enum filewatch_update_t
{
	FILEWATCH_DIR_ADDED,
	FILEWATCH_DIR_REMOVED,
	FILEWATCH_FILE_ADDED,
	FILEWATCH_FILE_REMOVED,
	FILEWATCH_FILE_MODIFIED,
} filewatch_update_t;

/**
 * User-supplied callback recieved from `filewatch_notify` whenever relevant changes to a watched
 * directory are detected.
 */
typedef void (filewatch_callback_t)(filewatch_update_t change, const char* virtual_path, void* udata);

/**
 * Start watching a specific directory. Does not recursively register sub-directories. The user is
 * expected to provide a valid callback to `cb`. `virtual_path` is directory within an `assetsys` mount.
 */
int filewatch_start_watching(filewatch_t* filewatch, const char* virtual_path, filewatch_callback_t* cb, void* udata);

/**
 * Stop watching a specific directory and cancel any queue'd up notifications.
 */
void filewatch_stop_watching(filewatch_t* filewatch, const char* virtual_path);

/**
 * Helper function for user-convenience. These can be useful when certain files need to be preprocessed
 * when changed or added.
 */
void filewatch_actual_path_to_virtual_path(filewatch_t* filewatch, const char* actual_path, char* virtual_path, int virtual_path_capacity);

/**
 * Helper function for user-convenience. These can be useful when certain files need to be preprocessed
 * when changed or added.
 */
void filewatch_virtual_path_to_actual_path(filewatch_t* filewatch, const char* virtual_path, char* actual_path, int actual_path_capacity);

#define CUTE_FILEWATCH_H
#endif

#ifdef CUTE_FILEWATCH_IMPLEMENTATION
#ifndef CUTE_FILEWATCH_IMPLEMENTATION_ONCE
#define CUTE_FILEWATCH_IMPLEMENTATION_ONCE

#if !defined(assetsys_h)
	#error This header depends on and wraps assetsys.h. See: https://github.com/mattiasgustavsson/libs/blob/master/assetsys.h
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
	#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#ifndef CUTE_FILEWATCH_MALLOC
	#include <stdlib.h>
	#define CUTE_FILEWATCH_MALLOC(size, ctx) malloc(size)
	#define CUTE_FILEWATCH_FREE(ptr, ctx) free(ptr)
#endif

#ifndef CUTE_FILEWATCH_MEMCPY
	#include <string.h>
	#define CUTE_FILEWATCH_MEMCPY(dst, src, n) memcpy(dst, src, n)
#endif

#ifndef CUTE_FILEWATCH_MEMSET
	#include <string.h>
	#define CUTE_FILEWATCH_MEMSET(ptr, val, n) memset(ptr, val, n)
#endif

#ifndef CUTE_FILEWATCH_STRLEN
	#include <string.h>
	#define CUTE_FILEWATCH_STRLEN(str) (int)strlen(str)
#endif

#ifndef CUTE_FILEWATCH_ASSERT
	#include <assert.h>
	#define CUTE_FILEWATCH_ASSERT(condition) assert(condition)
#endif


#ifndef STRPOOL_EMBEDDED_MALLOC
	#define STRPOOL_EMBEDDED_MALLOC(ctx, size) CUTE_FILEWATCH_MALLOC(size, ctx)
#endif

#ifndef STRPOOL_EMBEDDED_FREE
	#define STRPOOL_EMBEDDED_FREE(ctx, ptr) CUTE_FILEWATCH_FREE(ptr, ctx)
#endif

#define STRPOOL_EMBEDDED_IMPLEMENTATION

// being embed modified strpool.h by Mattias Gustavsson
// see: https://github.com/mattiasgustavsson/libs/blob/master/strpool.h

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
#if 0
    // Debug statistics
    printf( "\n\n" );
    printf( "Handles: %d/%d\n", pool->handle_count, pool->handle_capacity );
    printf( "Entries: %d/%d\n", pool->entry_count, pool->entry_capacity );
    printf( "Hashtable: %d/%d\n", pool->entry_count, pool->hash_capacity );
    printf( "Blocks: %d/%d\n", pool->block_count, pool->block_capacity );
    for( int i = 0; i < pool->block_count; ++i )
        {
        printf( "\n" );
        printf( "BLOCK: %d\n", i );
        printf( "Capacity: %d\n", pool->blocks[ i ].capacity );
        printf( "Free: [ %d ]", pool->blocks[ i ].capacity - ( pool->blocks[ i ].tail - pool->blocks[ i ].data ) );
        int fl = pool->blocks[ i ].free_list;
        int count = 0;
        int size = 0;
        int total = 0;
        while( fl >= 0 )
            {
            strpool_embedded_free_block_t* free_entry = (strpool_embedded_free_block_t*) ( pool->blocks[ i ].data + fl );
            total += free_entry->size;
            if( size == 0 ) { size = free_entry->size; }
            if( size != free_entry->size )
                {
                printf( ", %dx%d", count, size );
                count = 1;
                size = free_entry->size;
                }
            else
                {
                ++count;
                }
            fl = free_entry->next;
            }
        if( size != 0 ) printf( ", %dx%d", count, size );
        printf( ", { %d }\n", total );
        }
    printf( "\n\n" );
#endif

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

#define CUTE_FILES_IMPLEMENTATION

// begin embed cute_files.h

/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	tinyfiles.h - v1.0

	To create implementation (the function definitions)
		#define CUTE_FILES_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	Summary:
		Utility header for traversing directories to apply a function on each found file.
		Recursively finds sub-directories. Can also be used to iterate over files in a
		folder manually. All operations done in a cross-platform manner (thx posix!).

		This header does no dynamic memory allocation, and performs internally safe string
		copies as necessary. Strings for paths, file names and file extensions are all
		capped, and intended to use primarily the C run-time stack memory. Feel free to
		modify the defines in this file to adjust string size limitations.

		Read the header for specifics on each function.

	Here's an example to print all files in a folder:
		cf_dir_t dir;
		cf_dir_open(&dir, "a");

		while (dir.has_next)
		{
			cf_file_t file;
			cf_read_file(&dir, &file);
			printf("%s\n", file.name);
			cf_dir_next(&dir);
		}

		cf_dir_close(&dir);
*/

#if !defined(CUTE_FILES_H)

#define CUTE_FILES_WINDOWS	1
#define CUTE_FILES_MAC		2
#define CUTE_FILES_UNIX		3

#if defined(_WIN32)
	#define CUTE_FILES_PLATFORM CUTE_FILES_WINDOWS
	#if !defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
	#endif
#elif defined(__APPLE__)
	#define CUTE_FILES_PLATFORM CUTE_FILES_MAC
#else
	#define CUTE_FILES_PLATFORM CUTE_FILES_UNIX
#endif

#include <string.h> // strerror, strncpy

// change to 0 to compile out any debug checks
#define CUTE_FILES_DEBUG_CHECKS 1

#if CUTE_FILES_DEBUG_CHECKS

	#include <stdio.h>  // printf
	#include <assert.h> // assert
	#include <errno.h>
	#define CUTE_FILES_ASSERT assert
	
#else

	#define CUTE_FILES_ASSERT(...)

#endif // CUTE_FILES_DEBUG_CHECKS

#define CUTE_FILES_MAX_PATH 1024
#define CUTE_FILES_MAX_FILENAME 256
#define CUTE_FILES_MAX_EXT 32

struct cf_file_t;
struct cf_dir_t;
struct cf_time_t;
typedef struct cf_file_t cf_file_t;
typedef struct cf_dir_t cf_dir_t;
typedef struct cf_time_t cf_time_t;
typedef void (cf_callback_t)(cf_file_t* file, void* udata);

// Stores the file extension in cf_file_t::ext, and returns a pointer to
// cf_file_t::ext
const char* cf_get_ext(cf_file_t* file);

// Applies a function (cb) to all files in a directory. Will recursively visit
// all subdirectories. Useful for asset management, file searching, indexing, etc.
void cf_traverse(const char* path, cf_callback_t* cb, void* udata);

// Fills out a cf_file_t struct with file information. Does not actually open the
// file contents, and instead performs more lightweight OS-specific calls.
int cf_read_file(cf_dir_t* dir, cf_file_t* file);

// Once a cf_dir_t is opened, this function can be used to grab another file
// from the operating system.
void cf_dir_next(cf_dir_t* dir);

// Performs lightweight OS-specific call to close internal handle.
void cf_dir_close(cf_dir_t* dir);

// Performs lightweight OS-specific call to open a file handle on a directory.
int cf_dir_open(cf_dir_t* dir, const char* path);

// Compares file last write times. -1 if file at path_a was modified earlier than path_b.
// 0 if they are equal. 1 if file at path_b was modified earlier than path_a.
int cf_compare_file_times_by_path(const char* path_a, const char* path_b);

// Retrieves time file was last modified, returns 0 upon failure
int cf_get_file_time(const char* path, cf_time_t* time);

// Compares file last write times. -1 if time_a was modified earlier than path_b.
// 0 if they are equal. 1 if time_b was modified earlier than path_a.
int cf_compare_file_times(cf_time_t* time_a, cf_time_t* time_b);

// Returns 1 of file exists, otherwise returns 0.
int cf_file_exists(const char* path);

// Returns 1 if the file's extension matches the string in ext
// Returns 0 otherwise
int cf_match_ext(cf_file_t* file, const char* ext);

// Prints detected errors to stdout
void cf_do_unit_tests();

#if CUTE_FILES_PLATFORM == CUTE_FILES_WINDOWS

#if !defined _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <Windows.h>

	struct cf_file_t
	{
		char path[CUTE_FILES_MAX_PATH];
		char name[CUTE_FILES_MAX_FILENAME];
		char ext[CUTE_FILES_MAX_EXT];
		int is_dir;
		int is_reg;
		size_t size;
	};

	struct cf_dir_t
	{
		char path[CUTE_FILES_MAX_PATH];
		int has_next;
		HANDLE handle;
		WIN32_FIND_DATAA fdata;
	};

	struct cf_time_t
	{
		FILETIME time;
	};

#elif CUTE_FILES_PLATFORM == CUTE_FILES_MAC || CUTE_FILES_PLATFORM == CUTE_FILES_UNIX

	#include <sys/stat.h>
	#include <dirent.h>
	#include <unistd.h>
    #include <time.h>

	struct cf_file_t
	{
		char path[CUTE_FILES_MAX_PATH];
		char name[CUTE_FILES_MAX_FILENAME];
		char ext[CUTE_FILES_MAX_EXT];
		int is_dir;
		int is_reg;
		int size;
		struct stat info;
	};

	struct cf_dir_t
	{
		char path[CUTE_FILES_MAX_PATH];
		int has_next;
		DIR* dir;
		struct dirent* entry;
	};

	struct cf_time_t
	{
		time_t time;
	};

#endif

#define CUTE_FILES_H
#endif

#ifdef CUTE_FILES_IMPLEMENTATION
#ifndef CUTE_FILES_IMPLEMENTATION_ONCE
#define CUTE_FILES_IMPLEMENTATION_ONCE

#define cf_safe_strcpy(dst, src, n, max) cf_safe_strcpy_internal(dst, src, n, max, __FILE__, __LINE__)
static int cf_safe_strcpy_internal(char* dst, const char* src, int n, int max, const char* file, int line)
{
	int c;
	const char* original = src;

	do
	{
		if (n >= max)
		{
			if (!CUTE_FILES_DEBUG_CHECKS) break;
			printf("ERROR: String \"%s\" too long to copy on line %d in file %s (max length of %d).\n"
				, original
				, line
				, file
				, max);
			CUTE_FILES_ASSERT(0);
		}

		c = *src++;
		dst[n] = c;
		++n;
	} while (c);

	return n;
}

const char* cf_get_ext(cf_file_t* file)
{
	char* period = file->name;
	char c;
	while ((c = *period++)) if (c == '.') break;
	if (c) cf_safe_strcpy(file->ext, period, 0, CUTE_FILES_MAX_EXT);
	else file->ext[0] = 0;
	return file->ext;
}

void cf_traverse(const char* path, cf_callback_t* cb, void* udata)
{
	cf_dir_t dir;
	cf_dir_open(&dir, path);

	while (dir.has_next)
	{
		cf_file_t file;
		cf_read_file(&dir, &file);

		if (file.is_dir && file.name[0] != '.')
		{
			char path2[CUTE_FILES_MAX_PATH];
			int n = cf_safe_strcpy(path2, path, 0, CUTE_FILES_MAX_PATH);
			n = cf_safe_strcpy(path2, "/", n - 1, CUTE_FILES_MAX_PATH);
			cf_safe_strcpy(path2, file.name, n -1, CUTE_FILES_MAX_PATH);
			cf_traverse(path2, cb, udata);
		}

		if (file.is_reg) cb(&file, udata);
		cf_dir_next(&dir);
	}

	cf_dir_close(&dir);
}

int cf_match_ext(cf_file_t* file, const char* ext)
{
	if (*ext == '.') ++ext;
	return !strcmp(file->ext, ext);
}

#if CUTE_FILES_PLATFORM == CUTE_FILES_WINDOWS

	int cf_read_file(cf_dir_t* dir, cf_file_t* file)
	{
		CUTE_FILES_ASSERT(dir->handle != INVALID_HANDLE_VALUE);

		int n = 0;
		char* fpath = file->path;
		char* dpath = dir->path;

		n = cf_safe_strcpy(fpath, dpath, 0, CUTE_FILES_MAX_PATH);
		n = cf_safe_strcpy(fpath, "/", n - 1, CUTE_FILES_MAX_PATH);

		char* dname = dir->fdata.cFileName;
		char* fname = file->name;

		cf_safe_strcpy(fname, dname, 0, CUTE_FILES_MAX_FILENAME);
		cf_safe_strcpy(fpath, fname, n - 1, CUTE_FILES_MAX_PATH);

		size_t max_dword = MAXDWORD;
		file->size = ((size_t)dir->fdata.nFileSizeHigh * (max_dword + 1)) + (size_t)dir->fdata.nFileSizeLow;
		cf_get_ext(file);

		file->is_dir = !!(dir->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		file->is_reg = !!(dir->fdata.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) ||
			!(dir->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

		return 1;
	}

	void cf_dir_next(cf_dir_t* dir)
	{
		CUTE_FILES_ASSERT(dir->has_next);

		if (!FindNextFileA(dir->handle, &dir->fdata))
		{
			dir->has_next = 0;
			DWORD err = GetLastError();
			CUTE_FILES_ASSERT(err == ERROR_SUCCESS || err == ERROR_NO_MORE_FILES);
		}
	}

	void cf_dir_close(cf_dir_t* dir)
	{
		dir->path[0] = 0;
		dir->has_next = 0;
		if (dir->handle != INVALID_HANDLE_VALUE) FindClose(dir->handle);
	}

	int cf_dir_open(cf_dir_t* dir, const char* path)
	{
		int n = cf_safe_strcpy(dir->path, path, 0, CUTE_FILES_MAX_PATH);
		n = cf_safe_strcpy(dir->path, "\\*", n - 1, CUTE_FILES_MAX_PATH);
		dir->handle = FindFirstFileA(dir->path, &dir->fdata);
		dir->path[n - 3] = 0;

		if (dir->handle == INVALID_HANDLE_VALUE)
		{
			printf("ERROR: Failed to open directory (%s): %s.\n", path, strerror(errno));
			cf_dir_close(dir);
			CUTE_FILES_ASSERT(0);
			return 0;
		}

		dir->has_next = 1;

		return 1;
	}

	int cf_compare_file_times_by_path(const char* path_a, const char* path_b)
	{
		FILETIME time_a = { 0 };
		FILETIME time_b = { 0 };
		WIN32_FILE_ATTRIBUTE_DATA data;

		if (GetFileAttributesExA(path_a, GetFileExInfoStandard, &data)) time_a = data.ftLastWriteTime;
		if (GetFileAttributesExA(path_b, GetFileExInfoStandard, &data)) time_b = data.ftLastWriteTime;
		return CompareFileTime(&time_a, &time_b);
	}

	int cf_get_file_time(const char* path, cf_time_t* time)
	{
		FILETIME initialized_to_zero = { 0 };
		time->time = initialized_to_zero;
		WIN32_FILE_ATTRIBUTE_DATA data;
		if (GetFileAttributesExA(path, GetFileExInfoStandard, &data))
		{
			time->time = data.ftLastWriteTime;
			return 1;
		}
		return 0;
	}

	int cf_compare_file_times(cf_time_t* time_a, cf_time_t* time_b)
	{
		return CompareFileTime(&time_a->time, &time_b->time);
	}

	int cf_file_exists(const char* path)
	{
		WIN32_FILE_ATTRIBUTE_DATA unused;
		return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
	}

#elif CUTE_FILES_PLATFORM == CUTE_FILES_MAC || CUTE_FILES_PLATFORM == CUTE_FILES_UNIX

	int cf_read_file(cf_dir_t* dir, cf_file_t* file)
	{
		CUTE_FILES_ASSERT(dir->entry);

		int n = 0;
		char* fpath = file->path;
		char* dpath = dir->path;

		n = cf_safe_strcpy(fpath, dpath, 0, CUTE_FILES_MAX_PATH);
		n = cf_safe_strcpy(fpath, "/", n - 1, CUTE_FILES_MAX_PATH);

		char* dname = dir->entry->d_name;
		char* fname = file->name;

		cf_safe_strcpy(fname, dname, 0, CUTE_FILES_MAX_FILENAME);
		cf_safe_strcpy(fpath, fname, n - 1, CUTE_FILES_MAX_PATH);

		if (stat(file->path, &file->info))
			return 0;

		file->size = file->info.st_size;
		cf_get_ext(file);

		file->is_dir = S_ISDIR(file->info.st_mode);
		file->is_reg = S_ISREG(file->info.st_mode);

		return 1;
	}

	void cf_dir_next(cf_dir_t* dir)
	{
		CUTE_FILES_ASSERT(dir->has_next);
		dir->entry = readdir(dir->dir);
		dir->has_next = dir->entry ? 1 : 0;
	}

	void cf_dir_close(cf_dir_t* dir)
	{
		dir->path[0] = 0;
		if (dir->dir) closedir(dir->dir);
		dir->dir = 0;
		dir->has_next = 0;
		dir->entry = 0;
	}

	int cf_dir_open(cf_dir_t* dir, const char* path)
	{
		cf_safe_strcpy(dir->path, path, 0, CUTE_FILES_MAX_PATH);
		dir->dir = opendir(path);

		if (!dir->dir)
		{
			printf("ERROR: Failed to open directory (%s): %s.\n", path, strerror(errno));
			cf_dir_close(dir);
			CUTE_FILES_ASSERT(0);
			return 0;
		}

		dir->has_next = 1;
		dir->entry = readdir(dir->dir);
		if (!dir->dir) dir->has_next = 0;

		return 1;
	}

	// Warning : untested code! (let me know if it breaks)
	int cf_compare_file_times_by_path(const char* path_a, const char* path_b)
	{
		time_t time_a;
		time_t time_b;
		struct stat info;
		if (stat(path_a, &info)) return 0;
		time_a = info.st_mtime;
		if (stat(path_b, &info)) return 0;
		time_b = info.st_mtime;
		return (int)difftime(time_a, time_b);
	}

	// Warning : untested code! (let me know if it breaks)
	int cf_get_file_time(const char* path, cf_time_t* time)
	{
		struct stat info;
		if (stat(path, &info)) return 0;
		time->time = info.st_mtime;
		return 1;
	}

	// Warning : untested code! (let me know if it breaks)
	int cf_compare_file_times(cf_time_t* time_a, cf_time_t* time_b)
	{
		return (int)difftime(time_a->time, time_b->time);
	}

	// Warning : untested code! (let me know if it breaks)
	int cf_file_exists(const char* path)
	{
		return access(path, F_OK) != -1;
	}

#endif // CUTE_FILES_PLATFORM

#endif // CUTE_FILES_IMPLEMENTATION_ONCE
#endif // CUTE_FILES_IMPLEMENTATION

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

// end embed cute_files.h

#ifndef HASHTABLE_MALLOC
	#define HASHTABLE_MALLOC(ctx, size) CUTE_FILEWATCH_MALLOC(size, ctx)
#endif

#ifndef HASHTABLE_FREE
	#define HASHTABLE_FREE(ctx, ptr) CUTE_FILEWATCH_FREE(ptr, ctx)
#endif

#define HASHTABLE_IMPLEMENTATION

// begin embed hashtable.h by Mattias Gustavsson

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

// end embed hashtable.h

const char* filewatch_error_reason;

typedef struct filewatch_path_t
{
	STRPOOL_EMBEDDED_U64 actual_id;
	STRPOOL_EMBEDDED_U64 virtual_id;
} filewatch_path_t;

typedef struct filewatch_entry_internal_t
{
	filewatch_path_t path;
	STRPOOL_EMBEDDED_U64 name_id;
	int is_dir;
	cf_time_t time;
} filewatch_entry_internal_t;

typedef struct filewatch_watched_dir_internal_t
{
	filewatch_path_t dir_path;
	filewatch_callback_t* cb;
	void* udata;
	hashtable_t entries;
} filewatch_watched_dir_internal_t;

typedef struct filewatch_notification_internal_t
{
	filewatch_callback_t* cb;
	void* udata;
	filewatch_update_t change;
	filewatch_path_t path;
} filewatch_notification_internal_t;

struct filewatch_t
{
	assetsys_t* assetsys;
	filewatch_path_t mount_path;
	int mounted;

	int watch_count;
	int watch_capacity;
	filewatch_watched_dir_internal_t* watches;

	int notification_count;
	int notification_capacity;
	filewatch_notification_internal_t* notifications;

	void* mem_ctx;
	strpool_embedded_t strpool;
};

#define CUTE_FILEWATCH_CHECK_BUFFER_GROW(ctx, count, capacity, data, type) \
	do { \
		if (ctx->count == ctx->capacity) \
		{ \
			int new_capacity = ctx->capacity ? ctx->capacity * 2 : 64; \
			void* new_data = CUTE_FILEWATCH_MALLOC(sizeof(type) * new_capacity, ctx->mem_ctx); \
			if (!new_data) { filewatch_error_reason = "Out of memory."; return 0; } \
			CUTE_FILEWATCH_MEMCPY(new_data, ctx->data, sizeof(type) * ctx->count); \
			CUTE_FILEWATCH_FREE(ctx->data, ctx->mem_ctx); \
			ctx->data = (type*)new_data; \
			ctx->capacity = new_capacity; \
		} \
	} while (0)

filewatch_t* filewatch_create(struct assetsys_t* assetsys, void* mem_ctx)
{
	filewatch_t* filewatch = (filewatch_t*)CUTE_FILEWATCH_MALLOC(sizeof(filewatch_t), mem_ctx);
	CUTE_FILEWATCH_MEMSET(filewatch, 0, sizeof(filewatch_t));
	filewatch->mem_ctx = mem_ctx;
	filewatch->assetsys = assetsys;
	strpool_embedded_init(&filewatch->strpool, 0);
	return filewatch;
}

void filewatch_free(filewatch_t* filewatch)
{
	strpool_embedded_term(&filewatch->strpool);
	CUTE_FILEWATCH_FREE(filewatch->watches, filewatch->mem_ctx);
	CUTE_FILEWATCH_FREE(filewatch->notifications, filewatch->mem_ctx);
	CUTE_FILEWATCH_FREE(filewatch, filewatch->mem_ctx);
}

#define CUTE_FILEWATCH_INJECT(filewatch, str, len) strpool_embedded_inject(&filewatch->strpool, str, len)
#define CUTE_FILEWATCH_CSTR(filewatch, id) strpool_embedded_cstr(&filewatch->strpool, id)
#define CUTE_FILEWATCH_CHECK(X, Y) do { if (!X) { filewatch_error_reason = Y; goto cute_filewatch_err; } } while (0)

int filewatch_mount(filewatch_t* filewatch, const char* actual_path, const char* mount_as_virtual_path)
{
	STRPOOL_EMBEDDED_U64 actual_id;
	STRPOOL_EMBEDDED_U64 virtual_id;
	assetsys_error_t ret;

	CUTE_FILEWATCH_CHECK(!filewatch->mounted, "`filewatch_t` is already mounted. Please call `filewatch_dismount` before calling `filewatch_mount` again.");

	actual_id = CUTE_FILEWATCH_INJECT(filewatch, actual_path, CUTE_FILEWATCH_STRLEN(actual_path));
	virtual_id = CUTE_FILEWATCH_INJECT(filewatch, mount_as_virtual_path, CUTE_FILEWATCH_STRLEN(mount_as_virtual_path));
	filewatch->mount_path.virtual_id = virtual_id;
	filewatch->mount_path.actual_id = actual_id;
	ret = assetsys_mount(filewatch->assetsys, actual_path, mount_as_virtual_path);
	CUTE_FILEWATCH_CHECK(ret == ASSETSYS_SUCCESS, "assetsys failed to initialize.");
	filewatch->mounted = 1;

	return 1;

cute_filewatch_err:
	return 0;
}

void filewatch_dismount(filewatch_t* filewatch)
{
	const char* mount_as = CUTE_FILEWATCH_CSTR(filewatch, filewatch->mount_path.virtual_id);
	const char* mount_path = CUTE_FILEWATCH_CSTR(filewatch, filewatch->mount_path.actual_id);
	assetsys_dismount(filewatch->assetsys, mount_path, mount_as);
	filewatch->mounted = 0;

	// remove all watches
}

static int filewatch_strncpy_internal(char* dst, const char* src, int n, int max)
{
	int c;

	do
	{
		if (n >= max - 1)
		{
			dst[max - 1] = 0;
			break;
		}
		c = *src++;
		dst[n] = (char)c;
		++n;
	} while (c);

	return n;
}

void filewatch_path_concat_internal(const char* path_a, const char* path_b, char* out, int max_buffer_length)
{
	int n = filewatch_strncpy_internal(out, path_a, 0, max_buffer_length);
	if (*path_b)
	{
		n = filewatch_strncpy_internal(out, "/", n - 1, max_buffer_length);
		filewatch_strncpy_internal(out, path_b, n - 1, max_buffer_length);
	}
}

filewatch_path_t filewatch_build_path_internal(filewatch_t* filewatch, filewatch_watched_dir_internal_t* watch, const char* path, const char* name)
{
	char virtual_buffer[CUTE_FILES_MAX_PATH];
	filewatch_path_t filewatch_path;
	filewatch_path.actual_id = CUTE_FILEWATCH_INJECT(filewatch, path, CUTE_FILEWATCH_STRLEN(path));
	filewatch_path_concat_internal(CUTE_FILEWATCH_CSTR(filewatch, watch->dir_path.virtual_id), name, virtual_buffer, CUTE_FILES_MAX_PATH);
	filewatch_path.virtual_id = CUTE_FILEWATCH_INJECT(filewatch, virtual_buffer, CUTE_FILEWATCH_STRLEN(virtual_buffer));
	return filewatch_path;
}

int filewatch_add_notification_internal(filewatch_t* filewatch, filewatch_watched_dir_internal_t* watch, filewatch_path_t path, filewatch_update_t change)
{
	CUTE_FILEWATCH_CHECK_BUFFER_GROW(filewatch, notification_count, notification_capacity, notifications, filewatch_notification_internal_t);
	filewatch_notification_internal_t notification;
	notification.cb = watch->cb;
	notification.udata = watch->udata;
	notification.change = change;
	notification.path = path;
	filewatch->notifications[filewatch->notification_count++] = notification;
	return 0;
}

void filewatch_add_entry_internal(filewatch_watched_dir_internal_t* watch, filewatch_path_t path, STRPOOL_EMBEDDED_U64 name_id, const char* actual_buffer, int is_dir)
{
	filewatch_entry_internal_t entry;
	entry.path = path;
	entry.name_id = name_id;
	entry.is_dir = is_dir;
	cf_get_file_time(actual_buffer, &entry.time);
	hashtable_insert(&watch->entries, name_id, &entry);
}

int filewatch_update(filewatch_t* filewatch)
{
	int remount_needed = 0;
	CUTE_FILEWATCH_CHECK(filewatch->mounted, "`filewatch_t` must be mounted before called `filewatch_update`.");

	for (int i = 0; i < filewatch->watch_count; ++i)
	{
		filewatch_watched_dir_internal_t* watch = filewatch->watches + i;

		// look for removed entries
		int entry_count = hashtable_count(&watch->entries);
		filewatch_entry_internal_t* entries = (filewatch_entry_internal_t*)hashtable_items(&watch->entries);

		for (int j = 0; j < entry_count; ++j)
		{
			filewatch_entry_internal_t* entry = entries + j;

			int was_removed = !cf_file_exists(CUTE_FILEWATCH_CSTR(filewatch, entry->path.actual_id));
			if (was_removed)
			{
				// directory removed
				if (entry->is_dir)
				{
					filewatch_add_notification_internal(filewatch, watch, entry->path, FILEWATCH_DIR_REMOVED);
					remount_needed = 1;
				}

				// file removed
				else
				{
					filewatch_add_notification_internal(filewatch, watch, entry->path, FILEWATCH_FILE_REMOVED);
					remount_needed = 1;
				}

				// remove entry from table
				hashtable_remove(&watch->entries, entry->name_id);
				--entry_count;
				--j;
			}
		}

		// watched directory was removed
		int dir_was_removed = !cf_file_exists(CUTE_FILEWATCH_CSTR(filewatch, watch->dir_path.actual_id));
		if (dir_was_removed)
		{
			filewatch_add_notification_internal(filewatch, watch, watch->dir_path, FILEWATCH_DIR_REMOVED);
			hashtable_term(&watch->entries);
			filewatch->watches[i--] = filewatch->watches[--filewatch->watch_count];
			remount_needed = 1;
			continue;
		}

		cf_dir_t dir;
		const char* actual_path = CUTE_FILEWATCH_CSTR(filewatch, watch->dir_path.actual_id);
		cf_dir_open(&dir, actual_path);

		while (dir.has_next)
		{
			cf_file_t file;
			cf_read_file(&dir, &file);

			STRPOOL_EMBEDDED_U64 name_id = CUTE_FILEWATCH_INJECT(filewatch, file.name, CUTE_FILEWATCH_STRLEN(file.name));
			filewatch_entry_internal_t* entry = (filewatch_entry_internal_t*)hashtable_find(&watch->entries, name_id);

			filewatch_path_t path = filewatch_build_path_internal(filewatch, watch, file.path, file.name);

			if (!file.is_dir && file.is_reg)
			{
				if (entry)
				{
					CUTE_FILEWATCH_ASSERT(!entry->is_dir);
					cf_time_t prev_time = entry->time;
					cf_time_t now_time;
					cf_get_file_time(file.path, &now_time);

					// file was modified
					if (cf_compare_file_times(&now_time, &prev_time) && cf_file_exists(file.path))
					{
						filewatch_add_notification_internal(filewatch, watch, path, FILEWATCH_FILE_MODIFIED);
						entry->time = now_time;
					}
				}

				// discovered new file
				else
				{
					filewatch_add_entry_internal(watch, path, name_id, file.path, 0);
					filewatch_add_notification_internal(filewatch, watch, path, FILEWATCH_FILE_ADDED);
					remount_needed = 1;
				}
			}

			else if (file.is_dir && file.name[0] != '.')
			{
				// found new directory
				if (!entry)
				{
					filewatch_add_entry_internal(watch, path, name_id, file.path, 1);
					filewatch_add_notification_internal(filewatch, watch, path, FILEWATCH_DIR_ADDED);
					remount_needed = 1;
				}
			}

			cf_dir_next(&dir);
		}

		cf_dir_close(&dir);
	}

	if (remount_needed)
	{
		const char* actual_path = CUTE_FILEWATCH_CSTR(filewatch, filewatch->mount_path.actual_id);
		const char* virtual_path = CUTE_FILEWATCH_CSTR(filewatch, filewatch->mount_path.virtual_id);
		assetsys_dismount(filewatch->assetsys, actual_path, virtual_path);
		assetsys_mount(filewatch->assetsys, actual_path, virtual_path);
	}

	return 1;

cute_filewatch_err:
	return 0;
}

void filewatch_notify(filewatch_t* filewatch)
{
	for (int i = 0; i < filewatch->notification_count; ++i)
	{
		filewatch_notification_internal_t* notification = filewatch->notifications + i;
		const char* virtual_path = CUTE_FILEWATCH_CSTR(filewatch, notification->path.virtual_id);
		notification->cb(notification->change, virtual_path, notification->udata);
	}
	filewatch->notification_count = 0;
}

int filewatch_start_watching(filewatch_t* filewatch, const char* virtual_path, filewatch_callback_t* cb, void* udata)
{
	filewatch_watched_dir_internal_t* watch;
	int success;
	CUTE_FILEWATCH_CHECK(filewatch->mounted, "`filewatch_t` must be mounted before called `filewatch_update`.");

	CUTE_FILEWATCH_CHECK_BUFFER_GROW(filewatch, watch_count, watch_capacity, watches, filewatch_watched_dir_internal_t);
	watch = filewatch->watches + filewatch->watch_count++;
	watch->cb = cb;
	watch->udata = udata;
	hashtable_init(&watch->entries, sizeof(filewatch_entry_internal_t), 32, filewatch->mem_ctx);

	filewatch_path_t path;
	char actual_path[260];
	filewatch_virtual_path_to_actual_path(filewatch, virtual_path, actual_path, 260);
	path.actual_id = CUTE_FILEWATCH_INJECT(filewatch, actual_path, CUTE_FILEWATCH_STRLEN(actual_path));
	path.virtual_id = CUTE_FILEWATCH_INJECT(filewatch, virtual_path, CUTE_FILEWATCH_STRLEN(virtual_path));
	watch->dir_path = path;

	cf_dir_t dir;
	success = cf_dir_open(&dir, actual_path);
	CUTE_FILEWATCH_CHECK(success, "`virtual_path` is not a valid directory.");

	while (dir.has_next)
	{
		cf_file_t file;
		cf_read_file(&dir, &file);
		STRPOOL_EMBEDDED_U64 name_id = CUTE_FILEWATCH_INJECT(filewatch, file.name, CUTE_FILEWATCH_STRLEN(file.name));
		filewatch_path_t file_path = filewatch_build_path_internal(filewatch, watch, file.path, file.name);

		if (!file.is_dir && file.is_reg)
		{
			filewatch_add_entry_internal(watch, file_path, name_id, file.path, 0);
		}

		else if (file.is_dir && file.name[0] != '.')
		{
			filewatch_add_entry_internal(watch, file_path, name_id, file.path, 1);
		}

		cf_dir_next(&dir);
	}

	cf_dir_close(&dir);

	return 1;

cute_filewatch_err:
	return 0;
}

void filewatch_stop_watching(filewatch_t* filewatch, const char* virtual_path)
{
	STRPOOL_EMBEDDED_U64 virtual_id = CUTE_FILEWATCH_INJECT(filewatch, virtual_path, CUTE_FILEWATCH_STRLEN(virtual_path));

	for (int i = 0; i < filewatch->watch_count; ++i)
	{
		filewatch_watched_dir_internal_t* watch = filewatch->watches + i;
		if (virtual_id == watch->dir_path.virtual_id)
		{
			hashtable_term(&watch->entries);

			// cancel queue'd up notifications for this watch
			for (int j = 0; j < filewatch->notification_count; ++j)
			{
				filewatch_notification_internal_t* notification = filewatch->notifications + j;
				if (notification->path.virtual_id == virtual_id && notification->cb == watch->cb && notification->udata == watch->udata)
				{
					filewatch->notifications[j--] = filewatch->notifications[--filewatch->notification_count];
				}
			}

			filewatch->watches[i] = filewatch->watches[--filewatch->watch_count];
			break;
		}
	}
}

void filewatch_actual_path_to_virtual_path(filewatch_t* filewatch, const char* actual_path, char* virtual_path, int virtual_path_capacity)
{
	const char* mount_path = CUTE_FILEWATCH_CSTR(filewatch, filewatch->mount_path.actual_id);
	const char* mount_as = CUTE_FILEWATCH_CSTR(filewatch, filewatch->mount_path.virtual_id);
	filewatch_path_concat_internal(mount_as, actual_path + CUTE_FILEWATCH_STRLEN(mount_path), virtual_path, virtual_path_capacity);
}

void filewatch_virtual_path_to_actual_path(filewatch_t* filewatch, const char* virtual_path, char* actual_path, int actual_path_capacity)
{
	const char* mount_path = CUTE_FILEWATCH_CSTR(filewatch, filewatch->mount_path.actual_id);
	const char* mount_as = CUTE_FILEWATCH_CSTR(filewatch, filewatch->mount_path.virtual_id);
	filewatch_path_concat_internal(mount_path, virtual_path + CUTE_FILEWATCH_STRLEN(mount_as), actual_path, actual_path_capacity);
}

#endif // CUTE_FILEWATCH_IMPLEMENTATION_ONCE
#endif // CUTE_FILEWATCH_IMPLEMENTATION
