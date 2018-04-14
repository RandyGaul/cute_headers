/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	tinytiled.h - v1.00

	To create implementation (the function definitions)
		#define TINYTILED_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY

		Parses Tiled (http://www.mapeditor.org/) files saved as the JSON file
		format. See http://doc.mapeditor.org/en/latest/reference/json-map-format/
		for a complete description of the JSON Tiled format. An entire map file
		is loaded up in entirety and used to fill in a set of structs. The entire
		struct collection is then handed to the user.

		This header is up to date with Tiled's documentation Revision f205c0b5 and
		verified to work with Tiled stable version 1.1.3.
		http://doc.mapeditor.org/en/latest/reference/json-map-format/

		Here is a past discussion thread on this header:
		https://www.reddit.com/r/gamedev/comments/87680n/tinytiled_tiled_json_map_parserloader_in_c/
*/

/*
	DOCUMENTATION

		Load up a Tiled json exported file, either from disk or memory, like so:

			tinytiled_map_t* map = tinytiled_load_map_from_memory(memory, size, 0);

		Then simply access the map's fields like so:

			// get map width and height
			int w = map->width;
			int h = map->height;

			// loop over the map's layers
			tinytiled_layer_t* layer = map->layers;
			while (layer)
			{
				int* data = layer->data;
				int data_count = layer->data_count;

				// do something with the tile data
				UserFunction_HandleTiles(data, data_count);

				layer = layer->next;
			}

		Finally, free it like so:

			tinytiled_free_map(map);

	LIMITATIONS

		More uncommon fields are not supported, and are annotated in this header.
		Search for "Not currently supported." without quotes to find them. tinytiled
		logs a warning whenever a known unsupported field is encountered, and will
		attempt to gracefully skip the field. If a field with completely unknown
		syntax is encountered (which can happen if tinytiled is used on a newer
		and unsupported version of Tiled), undefined behavior may occur (crashes).

		Compression of the tile GIDs is *not* supported in this header. Exporting
		a map from Tiled will create a JSON file. This JSON file itself can very
		trivially be compressed in its entirety, thus making Tiled's internal
		compression exporting not a useful feature for this header to support.
		Simply wrap calls to `tinytiled_load_map_from_file` in a decompression
		routine. Here is an example (assuming `zlib_uncompress` is already imp-
		lemented somewhere in the user's codebase):

			int size;
			void* uncompressed_data = zlib_uncompress(path_to_zipped_map_file, &size);
			tinytiled_map_t* map = tinytiled_load_map_from_memory(uncompressed_data, size, 0);
*/

#if !defined(TINYTILED_H)

// Read this in the event of errors
extern const char* tinytiled_error_reason;

typedef struct tinytiled_map_t tinytiled_map_t;

/*!
 * Load a map from disk, placed into heap allocated memory.. \p mem_ctx can be
 * NULL. It is used for custom allocations.
 */
tinytiled_map_t* tinytiled_load_map_from_file(const char* path, void* mem_ctx);

/*!
 * Load a map from memory. \p mem_ctx can be NULL. It is used for custom allocations.
 */
tinytiled_map_t* tinytiled_load_map_from_memory(const void* memory, int size_in_bytes, void* mem_ctx);

/*!
 * Free all dynamic memory associated with this map.
 */
void tinytiled_free_map(tinytiled_map_t* map);

#if !defined(TINYTILED_U64)
	#define TINYTILED_U64 unsigned long long
#endif

#if !defined(TINYTILED_INLINE)
	#if defined(_MSC_VER)
		#define TINYTILED_INLINE __inline
	#else
		#define TINYTILED_INLINE __inline__
	#endif
#endif

typedef struct tinytiled_layer_t tinytiled_layer_t;
typedef struct tinytiled_object_t tinytiled_object_t;
typedef struct tinytiled_tileset_t tinytiled_tileset_t;
typedef struct tinytiled_property_t tinytiled_property_t;
typedef union tinytiled_string_t tinytiled_string_t;

/*!
 * To access a string, simply do: object->name.ptr; this union is needed
 * as a workaround for 32-bit builds where the size of a pointer is only
 * 32 bits. 
 *
 * More info:
 * This unions is needed to support a single-pass parser, with string
 * interning, where the parser copies value directly into the user-facing
 * structures. As opposed to the parser copying values into an intermediate
 * structure, and finally copying the intermediate values into the
 * user-facing struct at the end. The latter option requires more code!
 */
union tinytiled_string_t
{
	const char* ptr;
	TINYTILED_U64 hash_id;
};

typedef enum TINYTILED_PROPERTY_TYPE
{
	TINYTILED_PROPERTY_NONE,
	TINYTILED_PROPERTY_INT,
	TINYTILED_PROPERTY_BOOL,
	TINYTILED_PROPERTY_FLOAT,
	TINYTILED_PROPERTY_STRING,

	// Note: currently unused! File properties are reported as strings in
	// this header, and it is up to users to know a-priori which strings
	// contain file paths.
	TINYTILED_PROPERTY_FILE,

	TINYTILED_PROPERTY_COLOR,
} TINYTILED_PROPERTY_TYPE;

struct tinytiled_property_t
{
	union
	{
		int integer;
		int boolean;
		float floating;
		tinytiled_string_t string;
		tinytiled_string_t file;
		int color;
	} data;
	TINYTILED_PROPERTY_TYPE type;
	tinytiled_string_t name;
};

struct tinytiled_object_t
{
	int ellipse;                      // 0 or 1. Used to mark an object as an ellipse.
	int gid;                          // GID, only if object comes from a Tilemap.
	int height;                       // Height in pixels. Ignored if using a gid.
	int id;                           // Incremental id - unique across all objects.
	tinytiled_string_t name;          // String assigned to name field in editor.
	int point;                        // 0 or 1. Used to mark an object as a point.

	// Example to index each vert of a polygon/polyline:
	/*
		float x, y;
		for(int i = 0; i < vert_count * 2; i += 2)
		{
			x = vertices[i];
			y = vertices[i + 1];
		}
	*/
	int vert_count;
	float* vertices;                  // Represents both type `polyline` and `polygon`.
	int vert_type;                    // 1 for `polygon` and 0 for `polyline`.

	int property_count;               // Number of elements in the properties array.
	tinytiled_property_t* properties; // Array of properties.
	float rotation;                   // Angle in degrees clockwise.
	/* text */                        // Not currently supported.
	tinytiled_string_t type;          // String assigned to type field in editor.
	int visible;                      // 0 or 1. Whether object is shown in editor.
	int width;                        // Width in pixels. Ignored if using a gid.
	float x;                          // x coordinate in pixels.
	float y;                          // y coordinate in pixels.
	tinytiled_object_t* next;         // Pointer to next object. NULL if final object.
};

/*!
 * Example of using both helper functions below to process the `data` pointer of a layer,
 * containing an array of `GID`s.
 *
 * for (int i = 0; i < layer->data_count; i++)
 * {
 *     int hflip, vflip, dflip;
 *     int tile_id = layer->data[i];
 *     tinytiled_get_flags(tile_id, &hflip, &vflip, &dflip);
 *     tile_id = tinytiled_unset_flags(tile_id);
 *     DoSomethingWithFlags(tile_id, flip, vflip, dlfip);
 *     DoSomethingWithTileID(tile_id);
 * }
 */

#define TINYTILED_FLIPPED_HORIZONTALLY_FLAG 0x80000000
#define TINYTILED_FLIPPED_VERTICALLY_FLAG   0x40000000
#define TINYTILED_FLIPPED_DIAGONALLY_FLAG   0x20000000

/*!
 * Helper for processing tile data in /ref `tinytiled_layer_t` `data`. Unsets all of
 * the image flipping flags in the higher bit of /p `tile_data_gid`.
 */
TINYTILED_INLINE int tinytiled_unset_flags(int tile_data_gid)
{
	const int flags = ~(TINYTILED_FLIPPED_HORIZONTALLY_FLAG | TINYTILED_FLIPPED_VERTICALLY_FLAG | TINYTILED_FLIPPED_DIAGONALLY_FLAG);
	return tile_data_gid & flags;
}

/*!
 * Helper for processing tile data in /ref `tinytiled_layer_t` `data`. Flags are
 * stored in the GID array `data` for flipping the image. Retrieves all three flag types.
 */
TINYTILED_INLINE void tinytiled_get_flags(int tile_data_gid, int* flip_horizontal, int* flip_vertical, int* flip_diagonal)
{
	*flip_horizontal = !!(tile_data_gid & TINYTILED_FLIPPED_HORIZONTALLY_FLAG);
	*flip_vertical = !!(tile_data_gid & TINYTILED_FLIPPED_VERTICALLY_FLAG);
	*flip_diagonal = !!(tile_data_gid & TINYTILED_FLIPPED_DIAGONALLY_FLAG);
}

struct tinytiled_layer_t
{
	/* compression; */                // Not currently supported.
	int data_count;                   // Number of integers in `data`.
	int* data;                        // Array of GIDs. `tilelayer` only. Only support CSV style exports.
	/* encoding; */                   // Not currently supported.
	tinytiled_string_t draworder;     // `topdown` (default) or `index`. `objectgroup` only.
	int height;                       // Row count. Same as map height for fixed-size maps.
	tinytiled_layer_t* layers;        // Linked list of layers. Only appears if `type` is `group`.
	tinytiled_string_t name;          // Name assigned to this layer.
	tinytiled_object_t* objects;      // Linked list of objects. `objectgroup` only.
	float opacity;                    // Value between 0 and 1.
	int property_count;               // Number of elements in the properties array.
	tinytiled_property_t* properties; // Array of properties.
	tinytiled_string_t type;          // `tilelayer`, `objectgroup`, `imagelayer` or `group`.
	int visible;                      // 0 or 1. Whether layer is shown or hidden in editor.
	int width;                        // Column count. Same as map width for fixed-size maps.
	int x;                            // Horizontal layer offset in tiles. Always 0.
	int y;                            // Vertical layer offset in tiles. Always 0.
	tinytiled_layer_t* next;          // Pointer to the next layer. NULL if final layer.
};

struct tinytiled_tileset_t
{
	int columns;                      // The number of tile columns in the tileset.
	int firstgid;                     // GID corresponding to the first tile in the set.
	/* grid */                        // Not currently supported.
	tinytiled_string_t image;         // Image used for tiles in this set (relative path from map file to source image).
	int imagewidth;                   // Width of source image in pixels.
	int imageheight;                  // Height of source image in pixels.
	int margin;                       // Buffer between image edge and first tile (pixels).
	tinytiled_string_t name;          // Name given to this tileset.
	int property_count;               // Number of elements in the properties array.
	tinytiled_property_t* properties; // Array of properties.
	int spacing;                      // Spacing between adjacent tiles in image (pixels).
	/* terrains */                    // Not currently supported.
	int tilecount;                    // The number of tiles in this tileset.
	int tileheight;                   // Maximum height of tiles in this set.
	/* tileoffset */                  // Not currently supported.
	/* tileproperties */              // Not currently supported.
	/* tiles */                       // Not currently supported.
	int tilewidth;                    // Maximum width of tiles in this set.
	tinytiled_string_t type;          // `tileset` (for tileset files, since 1.0).
	tinytiled_tileset_t* next;        // Pointer to next tileset. NULL if final tileset.
};

struct tinytiled_map_t
{
	int backgroundcolor;              // Hex-formatted color (#RRGGBB or #AARRGGBB) (optional).
	int height;                       // Number of tile rows.
	int infinite;                     // Whether the map has infinite dimensions.
	tinytiled_layer_t* layers;        // Linked list of layers. Can be NULL.
	int nextobjectid;                 // Auto-increments for each placed object.
	tinytiled_string_t orientation;   // `orthogonal`, `isometric`, `staggered` or `hexagonal`.
	int property_count;               // Number of elements in the properties array.
	tinytiled_property_t* properties; // Array of properties.
	tinytiled_string_t renderorder;   // Rendering direction (orthogonal maps only).
	tinytiled_string_t tiledversion;  // The Tiled version used to save the file.
	int tileheight;                   // Map grid height.
	tinytiled_tileset_t* tilesets;    // Linked list of tilesets.
	int tilewidth;                    // Map grid width.
	tinytiled_string_t type;          // `map` (since 1.0).
	int version;                      // The JSON format version.
	int width;                        // Number of tile columns.
};

#define TINYTILED_H
#endif

#ifdef TINYTILED_IMPLEMENTATION

#if !defined(TINYTILED_MALLOC)
	#include <stdlib.h>
	#define TINYTILED_MALLOC(size, ctx) malloc(size)
	#define TINYTILED_FREE(mem, ctx) free(mem)
#endif

#define STRPOOL_IMPLEMENTATION
#define STRPOOL_MALLOC(ctx, size) TINYTILED_MALLOC(size, ctx)
#define STRPOOL_FREE(ctx, ptr) TINYTILED_FREE(ptr, ctx)

#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
	#define _CRT_NONSTDC_NO_DEPRECATE
#endif

/*
	begin embedding strpool.h
*/

/*
------------------------------------------------------------------------------
          Licensing information can be found at the end of the file.
------------------------------------------------------------------------------

strpool.h - v1.4 - Highly efficient string pool for C/C++.

Do this:
    #define STRPOOL_IMPLEMENTATION
before you include this file in *one* C/C++ file to create the implementation.
*/

#ifndef strpool_h
#define strpool_h

#ifndef STRPOOL_U32
    #define STRPOOL_U32 unsigned int
#endif
#ifndef STRPOOL_U64
    #define STRPOOL_U64 unsigned long long
#endif

typedef struct strpool_t strpool_t;

typedef struct strpool_config_t
    {
    void* memctx;
    int ignore_case;
    int counter_bits;
    int index_bits;
    int entry_capacity;
    int block_capacity;
    int block_size;
    int min_length;
    } strpool_config_t;

extern strpool_config_t const strpool_default_config;

void strpool_init( strpool_t* pool, strpool_config_t const* config );
void strpool_term( strpool_t* pool );

void strpool_defrag( strpool_t* pool );

STRPOOL_U64 strpool_inject( strpool_t* pool, char const* string, int length );
void strpool_discard( strpool_t* pool, STRPOOL_U64 handle );

int strpool_incref( strpool_t* pool, STRPOOL_U64 handle );
int strpool_decref( strpool_t* pool, STRPOOL_U64 handle );
int strpool_getref( strpool_t* pool, STRPOOL_U64 handle );

int strpool_isvalid( strpool_t const* pool, STRPOOL_U64 handle );

char const* strpool_cstr( strpool_t const* pool, STRPOOL_U64 handle );
int strpool_length( strpool_t const* pool, STRPOOL_U64 handle );

char* strpool_collate( strpool_t const* pool, int* count );
void strpool_free_collated( strpool_t const* pool, char* collated_ptr );

#endif /* strpool_h */

/*
----------------------
    IMPLEMENTATION
----------------------
*/

#ifndef strpool_impl
#define strpool_impl

struct strpool_internal_hash_slot_t;
struct strpool_internal_entry_t;
struct strpool_internal_handle_t;
struct strpool_internal_block_t;

struct strpool_t
    {
    void* memctx;
    int ignore_case;
    int counter_shift;
    STRPOOL_U64 counter_mask;
    STRPOOL_U64 index_mask;

    int initial_entry_capacity;
    int initial_block_capacity;
    int block_size;
    int min_data_size;

    struct strpool_internal_hash_slot_t* hash_table;
    int hash_capacity;

    struct strpool_internal_entry_t* entries;
    int entry_capacity;
    int entry_count;

    struct strpool_internal_handle_t* handles;
    int handle_capacity;
    int handle_count;
    int handle_freelist_head;
    int handle_freelist_tail;

    struct strpool_internal_block_t* blocks;
    int block_capacity;
    int block_count;
    int current_block;
    };


#endif /* strpool_impl */


#ifdef STRPOOL_IMPLEMENTATION
#undef STRPOOL_IMPLEMENTATION

#include <stddef.h>

#ifndef STRPOOL_ASSERT
    #include <assert.h>
    #define STRPOOL_ASSERT( x ) assert( x )
#endif

#ifndef STRPOOL_MEMSET
    #include <string.h>
    #define STRPOOL_MEMSET( ptr, val, cnt ) ( memset( ptr, val, cnt ) )
#endif 

#ifndef STRPOOL_MEMCPY
    #include <string.h>
    #define STRPOOL_MEMCPY( dst, src, cnt ) ( memcpy( dst, src, cnt ) )
#endif 

#ifndef STRPOOL_MEMCMP
    #include <string.h>
    #define STRPOOL_MEMCMP( pr1, pr2, cnt ) ( memcmp( pr1, pr2, cnt ) )
#endif 

#ifndef STRPOOL_STRNICMP
    #ifdef _WIN32
        #include <string.h>
        #define STRPOOL_STRNICMP( s1, s2, len ) ( strnicmp( s1, s2, len ) )
    #else
        #include <string.h>
        #define STRPOOL_STRNICMP( s1, s2, len ) ( strncasecmp( s1, s2, len ) )        
    #endif
#endif 

#ifndef STRPOOL_MALLOC
    #include <stdlib.h>
    #define STRPOOL_MALLOC( ctx, size ) ( malloc( size ) )
    #define STRPOOL_FREE( ctx, ptr ) ( free( ptr ) )
#endif


typedef struct strpool_internal_hash_slot_t
    {
    STRPOOL_U32 hash_key;
    int entry_index;
    int base_count;
    } strpool_internal_hash_slot_t;


typedef struct strpool_internal_entry_t
    {
    int hash_slot;
    int handle_index;
    char* data;
    int size;
    int length;
    int refcount;
    } strpool_internal_entry_t;


typedef struct strpool_internal_handle_t
    {
    int entry_index;
    int counter;
    } strpool_internal_handle_t;


typedef struct strpool_internal_block_t
    {
    int capacity;
    char* data;
    char* tail;
    int free_list;
    } strpool_internal_block_t;


typedef struct strpool_internal_free_block_t
    {
    int size;
    int next;
    } strpool_internal_free_block_t;


strpool_config_t const strpool_default_config = 
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



static STRPOOL_U32 strpool_internal_pow2ceil( STRPOOL_U32 v )
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


static int strpool_internal_add_block( strpool_t* pool, int size )
    {
    if( pool->block_count >= pool->block_capacity ) 
        {
        pool->block_capacity *= 2;
        strpool_internal_block_t* new_blocks = (strpool_internal_block_t*) STRPOOL_MALLOC( pool->memctx, 
            pool->block_capacity * sizeof( *pool->blocks ) );
        STRPOOL_ASSERT( new_blocks );
        STRPOOL_MEMCPY( new_blocks, pool->blocks, pool->block_count * sizeof( *pool->blocks ) );
        STRPOOL_FREE( pool->memctx, pool->blocks );
        pool->blocks = new_blocks;
        }
    pool->blocks[ pool->block_count ].capacity = size;
    pool->blocks[ pool->block_count ].data = (char*) STRPOOL_MALLOC( pool->memctx, (size_t) size );
    STRPOOL_ASSERT( pool->blocks[ pool->block_count ].data );
    pool->blocks[ pool->block_count ].tail = pool->blocks[ pool->block_count ].data;
    pool->blocks[ pool->block_count ].free_list = -1;
    return pool->block_count++;
    }


void strpool_init( strpool_t* pool, strpool_config_t const* config )
    {
    if( !config ) config = &strpool_default_config;

    pool->memctx = config->memctx;
    pool->ignore_case = config->ignore_case;

    STRPOOL_ASSERT( config->counter_bits + config->index_bits <= 64 );
    pool->counter_shift = config->index_bits;
    pool->counter_mask = ( 1ULL << (STRPOOL_U64) config->counter_bits ) - 1;
    pool->index_mask = ( 1ULL << (STRPOOL_U64) config->index_bits ) - 1;

    pool->initial_entry_capacity = 
        (int) strpool_internal_pow2ceil( config->entry_capacity > 1 ? (STRPOOL_U32)config->entry_capacity : 2U );
    pool->initial_block_capacity = 
        (int) strpool_internal_pow2ceil( config->block_capacity > 1 ? (STRPOOL_U32)config->block_capacity : 2U );
    pool->block_size = 
        (int) strpool_internal_pow2ceil( config->block_size > 256 ? (STRPOOL_U32)config->block_size : 256U );
    pool->min_data_size = 
        (int) ( sizeof( int ) * 2 + 1 + ( config->min_length > 8 ? (STRPOOL_U32)config->min_length : 8U ) );

    pool->hash_capacity = pool->initial_entry_capacity * 2;
    pool->entry_capacity = pool->initial_entry_capacity;
    pool->handle_capacity = pool->initial_entry_capacity;
    pool->block_capacity = pool->initial_block_capacity;    

    pool->handle_freelist_head = -1;
    pool->handle_freelist_tail = -1;
    pool->block_count = 0;
    pool->handle_count = 0;
    pool->entry_count = 0;
    
    pool->hash_table = (strpool_internal_hash_slot_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->hash_capacity * sizeof( *pool->hash_table ) );
    STRPOOL_ASSERT( pool->hash_table );
    STRPOOL_MEMSET( pool->hash_table, 0, pool->hash_capacity * sizeof( *pool->hash_table ) );
    pool->entries = (strpool_internal_entry_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->entry_capacity * sizeof( *pool->entries ) );
    STRPOOL_ASSERT( pool->entries );
    pool->handles = (strpool_internal_handle_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->handle_capacity * sizeof( *pool->handles ) );
    STRPOOL_ASSERT( pool->handles );
    pool->blocks = (strpool_internal_block_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->block_capacity * sizeof( *pool->blocks ) );
    STRPOOL_ASSERT( pool->blocks );

    pool->current_block = strpool_internal_add_block( pool, pool->block_size );
    }


void strpool_term( strpool_t* pool )
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
            strpool_free_block_t* free_entry = (strpool_free_block_t*) ( pool->blocks[ i ].data + fl );
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

    for( int i = 0; i < pool->block_count; ++i ) STRPOOL_FREE( pool->memctx, pool->blocks[ i ].data );
    STRPOOL_FREE( pool->memctx, pool->blocks );         
    STRPOOL_FREE( pool->memctx, pool->handles );            
    STRPOOL_FREE( pool->memctx, pool->entries );            
    STRPOOL_FREE( pool->memctx, pool->hash_table );         
    }


void strpool_defrag( strpool_t* pool )
    {
    int data_size = 0;
    int count = 0;
    for( int i = 0; i < pool->entry_count; ++i )
        {
        strpool_internal_entry_t* entry = &pool->entries[ i ];
        if( entry->refcount > 0 )
            {
            data_size += entry->size;
            ++count;
            }
        }

    int data_capacity = data_size < pool->block_size ? 
        pool->block_size : (int)strpool_internal_pow2ceil( (STRPOOL_U32)data_size );

    int hash_capacity = count + count / 2;
    hash_capacity = hash_capacity < ( pool->initial_entry_capacity * 2 ) ? 
        ( pool->initial_entry_capacity * 2 ) : (int)strpool_internal_pow2ceil( (STRPOOL_U32)hash_capacity );
    strpool_internal_hash_slot_t* hash_table = (strpool_internal_hash_slot_t*) STRPOOL_MALLOC( pool->memctx, 
        hash_capacity * sizeof( *hash_table ) );
    STRPOOL_ASSERT( hash_table );
    STRPOOL_MEMSET( hash_table, 0, hash_capacity * sizeof( *hash_table ) );

    char* data = (char*) STRPOOL_MALLOC( pool->memctx, (size_t) data_capacity );
    STRPOOL_ASSERT( data );
    int capacity = count < pool->initial_entry_capacity ? 
        pool->initial_entry_capacity : (int)strpool_internal_pow2ceil( (STRPOOL_U32)count );
    strpool_internal_entry_t* entries = (strpool_internal_entry_t*) STRPOOL_MALLOC( pool->memctx, 
        capacity * sizeof( *entries ) );
    STRPOOL_ASSERT( entries );
    int index = 0;
    char* tail = data;
    for( int i = 0; i < pool->entry_count; ++i )
        {
        strpool_internal_entry_t* entry = &pool->entries[ i ];
        if( entry->refcount > 0 )
            {
            entries[ index ] = *entry;

            STRPOOL_U32 hash = pool->hash_table[ entry->hash_slot ].hash_key;
            int base_slot = (int)( hash & (STRPOOL_U32)( hash_capacity - 1 ) );
            int slot = base_slot;
            while( hash_table[ slot ].hash_key )
                slot = (slot + 1 ) & ( hash_capacity - 1 );
            STRPOOL_ASSERT( hash );
            hash_table[ slot ].hash_key = hash;
            hash_table[ slot ].entry_index = index;
            ++hash_table[ base_slot ].base_count;

            entries[ index ].hash_slot = slot;
            entries[ index ].data = tail;
            entries[ index ].handle_index = entry->handle_index;
            pool->handles[ entry->handle_index ].entry_index = index;
            STRPOOL_MEMCPY( tail, entry->data, entry->length + 1 + 2 * sizeof( STRPOOL_U32 ) );
            tail += entry->size;
            ++index;
            }
        }


    STRPOOL_FREE( pool->memctx, pool->hash_table );
    STRPOOL_FREE( pool->memctx, pool->entries );
    for( int i = 0; i < pool->block_count; ++i ) STRPOOL_FREE( pool->memctx, pool->blocks[ i ].data );

    if( pool->block_capacity != pool->initial_block_capacity )
        {
        STRPOOL_FREE( pool->memctx, pool->blocks );
        pool->blocks = (strpool_internal_block_t*) STRPOOL_MALLOC( pool->memctx, 
            pool->initial_block_capacity * sizeof( *pool->blocks ) );
        STRPOOL_ASSERT( pool->blocks );
        }
    pool->block_capacity = pool->initial_block_capacity;
    pool->block_count = 1;
    pool->current_block = 0;
    pool->blocks[ 0 ].capacity = data_capacity;
    pool->blocks[ 0 ].data = data;
    pool->blocks[ 0 ].tail = tail;
    pool->blocks[ 0 ].free_list = -1;
    
    pool->hash_table = hash_table;
    pool->hash_capacity = hash_capacity;

    pool->entries = entries;
    pool->entry_capacity = capacity;
    pool->entry_count = count;
    }


static STRPOOL_U64 strpool_internal_make_handle( int index, int counter, STRPOOL_U64 index_mask, int counter_shift, 
    STRPOOL_U64 counter_mask )
    {
    STRPOOL_U64 i = (STRPOOL_U64) ( index + 1 );
    STRPOOL_U64 c = (STRPOOL_U64) counter;
    return ( ( c & counter_mask ) << counter_shift ) | ( i & index_mask );
    }


static int strpool_internal_counter_from_handle( STRPOOL_U64 handle, int counter_shift, STRPOOL_U64 counter_mask  )
    {
    return (int) ( ( handle >> counter_shift ) & counter_mask ) ;
    }
    

static int strpool_internal_index_from_handle( STRPOOL_U64 handle, STRPOOL_U64 index_mask )
    {
    return ( (int) ( handle & index_mask ) ) - 1;
    }


static strpool_internal_entry_t* strpool_internal_get_entry( strpool_t const* pool, STRPOOL_U64 handle )
    {
    int index = strpool_internal_index_from_handle( handle, pool->index_mask );
    int counter = strpool_internal_counter_from_handle( handle, pool->counter_shift, pool->counter_mask );

    if( index >= 0 && index < pool->handle_count && 
        counter == (int) ( pool->handles[ index ].counter & pool->counter_mask ) )
            return &pool->entries[ pool->handles[ index ].entry_index ];

    return 0;
    }


static STRPOOL_U32 strpool_internal_find_in_blocks( strpool_t const* pool, char const* string, int length )
    {
    for( int i = 0; i < pool->block_count; ++i )
        {
        strpool_internal_block_t* block = &pool->blocks[ i ];
        // Check if string comes from pool
        if( string >= block->data + 2 * sizeof( STRPOOL_U32 ) && string < block->data + block->capacity ) 
            {
            STRPOOL_U32* ptr = (STRPOOL_U32*) string;
            int stored_length = (int)( *( ptr - 1 ) ); // Length is stored immediately before string
            if( stored_length != length || string[ length ] != '\0' ) return 0; // Invalid string
            STRPOOL_U32 hash = *( ptr - 2 ); // Hash is stored before the length field
            return hash;
            }
        }

    return 0;
    }


static STRPOOL_U32 strpool_internal_calculate_hash( char const* string, int length, int ignore_case )
    {
    STRPOOL_U32 hash = 5381U; 

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


static void strpool_internal_expand_hash_table( strpool_t* pool )
    {
    int old_capacity = pool->hash_capacity;
    strpool_internal_hash_slot_t* old_table = pool->hash_table;

    pool->hash_capacity *= 2;

    pool->hash_table = (strpool_internal_hash_slot_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->hash_capacity * sizeof( *pool->hash_table ) );
    STRPOOL_ASSERT( pool->hash_table );
    STRPOOL_MEMSET( pool->hash_table, 0, pool->hash_capacity * sizeof( *pool->hash_table ) );

    for( int i = 0; i < old_capacity; ++i )
        {
        STRPOOL_U32 hash_key = old_table[ i ].hash_key;
        if( hash_key )
            {
            int base_slot = (int)( hash_key & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
            int slot = base_slot;
            while( pool->hash_table[ slot ].hash_key )
                slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );
            STRPOOL_ASSERT( hash_key );
            pool->hash_table[ slot ].hash_key = hash_key;
            pool->hash_table[ slot ].entry_index = old_table[ i ].entry_index;  
            pool->entries[ pool->hash_table[ slot ].entry_index ].hash_slot = slot; 
            ++pool->hash_table[ base_slot ].base_count;
            }               
        }

    STRPOOL_FREE( pool->memctx, old_table );
    }


static void strpool_internal_expand_entries( strpool_t* pool )
    {
    pool->entry_capacity *= 2;
    strpool_internal_entry_t* new_entries = (strpool_internal_entry_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->entry_capacity * sizeof( *pool->entries ) );
    STRPOOL_ASSERT( new_entries );
    STRPOOL_MEMCPY( new_entries, pool->entries, pool->entry_count * sizeof( *pool->entries ) );
    STRPOOL_FREE( pool->memctx, pool->entries );
    pool->entries = new_entries;    
    }


static void strpool_internal_expand_handles( strpool_t* pool )
    {
    pool->handle_capacity *= 2;
    strpool_internal_handle_t* new_handles = (strpool_internal_handle_t*) STRPOOL_MALLOC( pool->memctx, 
        pool->handle_capacity * sizeof( *pool->handles ) );
    STRPOOL_ASSERT( new_handles );
    STRPOOL_MEMCPY( new_handles, pool->handles, pool->handle_count * sizeof( *pool->handles ) );
    STRPOOL_FREE( pool->memctx, pool->handles );
    pool->handles = new_handles;
    }


static char* strpool_internal_get_data_storage( strpool_t* pool, int size, int* alloc_size )
    {
    if( size < (int)sizeof( strpool_internal_free_block_t ) ) size = sizeof( strpool_internal_free_block_t );
    if( size < pool->min_data_size ) size = pool->min_data_size;
    size = (int)strpool_internal_pow2ceil( (STRPOOL_U32)size );
    
    // Try to find a large enough free slot in existing blocks
    for( int i = 0; i < pool->block_count; ++i )
        {
        int free_list = pool->blocks[ i ].free_list;
        int prev_list = -1;
        while( free_list >= 0 )
            {
            strpool_internal_free_block_t* free_entry = 
                (strpool_internal_free_block_t*) ( pool->blocks[ i ].data + free_list );
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
                    strpool_internal_free_block_t* prev_entry = 
                        (strpool_internal_free_block_t*) ( pool->blocks[ i ].data + prev_list );
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
    pool->current_block = strpool_internal_add_block( pool, size > pool->block_size ? size : pool->block_size );
    char* data = pool->blocks[ pool->current_block ].tail;
    pool->blocks[ pool->current_block ].tail += size;
    *alloc_size = size;
    return data;
    }
    

STRPOOL_U64 strpool_inject( strpool_t* pool, char const* string, int length )
    {
    if( !string || length < 0 ) return 0;

    STRPOOL_U32 hash = strpool_internal_find_in_blocks( pool, string, length );
    // If no stored hash, calculate it from data
    if( !hash ) hash = strpool_internal_calculate_hash( string, length, pool->ignore_case ); 

    // Return handle to existing string, if it is already in pool
    int base_slot = (int)( hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
    int base_count = pool->hash_table[ base_slot ].base_count;
    int slot = base_slot;
    int first_free = slot;
    while( base_count > 0 )
        {
        STRPOOL_U32 slot_hash = pool->hash_table[ slot ].hash_key;
        if( slot_hash == 0 && pool->hash_table[ first_free ].hash_key != 0 ) first_free = slot;
        int slot_base = (int)( slot_hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
        if( slot_base == base_slot ) 
            {
            STRPOOL_ASSERT( base_count > 0 );
            --base_count;
            if( slot_hash == hash )
                {
                int index = pool->hash_table[ slot ].entry_index;
                strpool_internal_entry_t* entry = &pool->entries[ index ];
                if( entry->length == length && 
                    ( 
                       ( !pool->ignore_case &&   STRPOOL_MEMCMP( entry->data + 2 * sizeof( STRPOOL_U32 ), string, (size_t)length ) == 0 )
                    || (  pool->ignore_case && STRPOOL_STRNICMP( entry->data + 2 * sizeof( STRPOOL_U32 ), string, (size_t)length ) == 0 ) 
                    ) 
                  )
                    {
                    int handle_index = entry->handle_index;
                    return strpool_internal_make_handle( handle_index, pool->handles[ handle_index ].counter, 
                        pool->index_mask, pool->counter_shift, pool->counter_mask );
                    }
                }
            }
        slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );
        }   

    // This is a new string, so let's add it

    if( pool->entry_count >= ( pool->hash_capacity  - pool->hash_capacity / 3 ) )
        {
        strpool_internal_expand_hash_table( pool );

        base_slot = (int)( hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
        slot = base_slot;
        first_free = slot;
        while( base_count )
            {
            STRPOOL_U32 slot_hash = pool->hash_table[ slot ].hash_key;
            if( slot_hash == 0 && pool->hash_table[ first_free ].hash_key != 0 ) first_free = slot;
            int slot_base = (int)( slot_hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
            if( slot_base == base_slot )  --base_count;
            slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );
            }       
        }
        
    slot = first_free;
    while( pool->hash_table[ slot ].hash_key )
        slot = ( slot + 1 ) & ( pool->hash_capacity - 1 );

    if( pool->entry_count >= pool->entry_capacity )
        strpool_internal_expand_entries( pool );

    STRPOOL_ASSERT( !pool->hash_table[ slot ].hash_key && ( hash & ( (STRPOOL_U32) pool->hash_capacity - 1 ) ) == (STRPOOL_U32) base_slot );
    STRPOOL_ASSERT( hash );
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
        strpool_internal_expand_handles( pool );
        handle_index = pool->handle_count;
        pool->handles[ pool->handle_count ].counter = 1;
        ++pool->handle_count;           
        }

    pool->handles[ handle_index ].entry_index = pool->entry_count;
        
    strpool_internal_entry_t* entry = &pool->entries[ pool->entry_count ];
    ++pool->entry_count;
        
    int data_size = length + 1 + (int) ( 2 * sizeof( STRPOOL_U32 ) );
    char* data = strpool_internal_get_data_storage( pool, data_size, &data_size );
    entry->hash_slot = slot;
    entry->handle_index = handle_index;
    entry->data = data;
    entry->size = data_size;
    entry->length = length;
    entry->refcount = 0;

    *(STRPOOL_U32*)(data) = hash;
    data += sizeof( STRPOOL_U32 );
    *(STRPOOL_U32*)(data) = (STRPOOL_U32) length;
    data += sizeof( STRPOOL_U32 );
    STRPOOL_MEMCPY( data, string, (size_t) length ); 
    data[ length ] = 0; // Ensure trailing zero

    return strpool_internal_make_handle( handle_index, pool->handles[ handle_index ].counter, pool->index_mask, 
        pool->counter_shift, pool->counter_mask );
    }


void strpool_discard( strpool_t* pool, STRPOOL_U64 handle )
    {   
    strpool_internal_entry_t* entry = strpool_internal_get_entry( pool, handle );
    if( entry && entry->refcount == 0 )
        {
        int entry_index = pool->handles[ entry->handle_index ].entry_index;

        // recycle string mem
        for( int i = 0; i < pool->block_count; ++i )
            {
            strpool_internal_block_t* block = &pool->blocks[ i ];
            if( entry->data >= block->data && entry->data <= block->tail )
                {
                if( block->free_list < 0 )
                    {
                    strpool_internal_free_block_t* new_entry = (strpool_internal_free_block_t*) ( entry->data );
                    block->free_list = (int) ( entry->data - block->data );
                    new_entry->next = -1;
                    new_entry->size = entry->size;
                    }
                else
                    {
                    int free_list = block->free_list;
                    int prev_list = -1;
                    while( free_list >= 0 )
                        {
                        strpool_internal_free_block_t* free_entry = 
                            (strpool_internal_free_block_t*) ( pool->blocks[ i ].data + free_list );
                        if( free_entry->size <= entry->size ) 
                            {
                            strpool_internal_free_block_t* new_entry = (strpool_internal_free_block_t*) ( entry->data );
                            if( prev_list < 0 )
                                {
                                new_entry->next = pool->blocks[ i ].free_list;
                                pool->blocks[ i ].free_list = (int) ( entry->data - block->data );          
                                }
                            else
                                {
                                strpool_internal_free_block_t* prev_entry = 
                                    (strpool_internal_free_block_t*) ( pool->blocks[ i ].data + prev_list );
                                prev_entry->next = (int) ( entry->data - block->data );
                                new_entry->next = free_entry->next;
                                }
                            new_entry->size = entry->size;
                            break;
                            }
                        prev_list = free_list;
                        free_list = free_entry->next;
                        }
                    }
                break;
                }
            }

        // recycle handle
        if( pool->handle_freelist_tail < 0 )
            {
            STRPOOL_ASSERT( pool->handle_freelist_head < 0 );
            pool->handle_freelist_head = entry->handle_index;
            pool->handle_freelist_tail = entry->handle_index;
            }
        else
            {
            pool->handles[ pool->handle_freelist_tail ].entry_index = entry->handle_index;
            pool->handle_freelist_tail = entry->handle_index;
            }
        ++pool->handles[ entry->handle_index ].counter; // invalidate handle via counter
        pool->handles[ entry->handle_index ].entry_index = -1;

        // recycle hash slot
        STRPOOL_U32 hash = pool->hash_table[ entry->hash_slot ].hash_key;
        int base_slot = (int)( hash & (STRPOOL_U32)( pool->hash_capacity - 1 ) );
        STRPOOL_ASSERT( hash );
        --pool->hash_table[ base_slot ].base_count;
        pool->hash_table[ entry->hash_slot ].hash_key = 0;

        // recycle entry
        if( entry_index != pool->entry_count - 1 )
            {
            pool->entries[ entry_index ] = pool->entries[ pool->entry_count - 1 ];
            pool->hash_table[ pool->entries[ entry_index ].hash_slot ].entry_index = entry_index;
            pool->handles[ pool->entries[ entry_index ].handle_index ].entry_index = entry_index;
            }
        --pool->entry_count;
        }       

    }


int strpool_incref( strpool_t* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t* entry = strpool_internal_get_entry( pool, handle );
    if( entry )
        {
        ++entry->refcount;
        return entry->refcount;
        }
    return 0;
    }


int strpool_decref( strpool_t* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t* entry = strpool_internal_get_entry( pool, handle );
    if( entry )
        {
        STRPOOL_ASSERT( entry->refcount > 0 );
        --entry->refcount;
        return entry->refcount;
        }
    return 0;
    }


int strpool_getref( strpool_t* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t* entry = strpool_internal_get_entry( pool, handle );
    if( entry ) return entry->refcount;
    return 0;
    }


int strpool_isvalid( strpool_t const* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t const* entry = strpool_internal_get_entry( pool, handle );
    if( entry ) return 1;
    return 0;
    }


char const* strpool_cstr( strpool_t const* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t const* entry = strpool_internal_get_entry( pool, handle );
    if( entry ) return entry->data + 2 * sizeof( STRPOOL_U32 ); // Skip leading hash value
    return NULL;
    }


int strpool_length( strpool_t const* pool, STRPOOL_U64 handle )
    {
    strpool_internal_entry_t const* entry = strpool_internal_get_entry( pool, handle );
    if( entry ) return entry->length;
    return 0;
    }


char* strpool_collate( strpool_t const* pool, int* count )
    {
    int size = 0;
    for( int i = 0; i < pool->entry_count; ++i ) size += pool->entries[ i ].length + 1;
    if( size == 0 ) return NULL;

    char* strings = (char*) STRPOOL_MALLOC( pool->memctx, (size_t) size );
    STRPOOL_ASSERT( strings );
    *count = pool->entry_count;
    char* ptr = strings;
    for( int i = 0; i < pool->entry_count; ++i )
        {
        int len = pool->entries[ i ].length + 1;
        char* src = pool->entries[ i ].data += 2 * sizeof( STRPOOL_U32 );
        STRPOOL_MEMCPY( ptr, src, (size_t) len );
        ptr += len;
        }
    return strings;
    }


void strpool_free_collated( strpool_t const* pool, char* collated_ptr )
    {
    (void) pool;
    STRPOOL_FREE( pool->memctx, collated_ptr );
    }


#endif /* STRPOOL_IMPLEMENTATION */


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

#if !defined(TINYTILED_WARNING)
	#define TINYTILED_DEFAULT_WARNING
	#define TINYTILED_WARNING tinytiled_warning
#endif

#if !defined(TINYTILED_MEMCPY)
	#include <string.h> // memcpy
	#define TINYTILED_MEMCPY memcpy
#endif

#if !defined(TINYTILED_MEMSET)
	#include <string.h> // memset
	#define TINYTILED_MEMSET memset
#endif

#if !defined(TINYTILED_UNUSED)
	#if defined(_MSC_VER)
		#define TINYTILED_UNUSED(x) (void)x
	#else
		#define TINYTILED_UNUSED(x) (void)(sizeof(x))
	#endif
#endif

const char* tinytiled_error_reason;

#ifdef TINYTILED_DEFAULT_WARNING
	#include <stdio.h>

	void tinytiled_warning(const char* warning)
	{
		printf("WARNING (tinytiled): %s\n", warning);
	}
#endif

typedef struct tinytiled_page_t tinytiled_page_t;
typedef struct tinytiled_map_internal_t tinytiled_map_internal_t;

struct tinytiled_page_t
{
	tinytiled_page_t* next;
	void* data;
};

#define TINYTILED_INTERNAL_BUFFER_MAX 1024

struct tinytiled_map_internal_t
{
	char* in;
	char* end;
	tinytiled_map_t map;
	strpool_t strpool;
	void* mem_ctx;
	int page_size;
	int bytes_left_on_page;
	tinytiled_page_t* pages;
	int scratch_len;
	char scratch[TINYTILED_INTERNAL_BUFFER_MAX];
};

void* tinytiled_alloc(tinytiled_map_internal_t* m, int size)
{
	if (m->bytes_left_on_page < size)
	{
		tinytiled_page_t* page = (tinytiled_page_t*)TINYTILED_MALLOC(sizeof(tinytiled_page_t) + m->page_size, m->mem_ctx);
		if (!page) return 0;
		page->next = m->pages;
		page->data = page + 1;
		m->pages = page;
	}

	void* data = ((char*)m->pages->data) + (m->page_size - m->bytes_left_on_page);
	m->bytes_left_on_page -= size;
	return data;
}

TINYTILED_U64 tinytiled_FNV1a(const void* buf, int len)
{
	TINYTILED_U64 h = (TINYTILED_U64)14695981039346656037U;
	const char* str = (const char*)buf;

	while (len--)
	{
		char c = *str++;
		h = h ^ (TINYTILED_U64)c;
		h = h * (TINYTILED_U64)1099511628211;
	}

	return h;
}

static char* tinytiled_read_file_to_memory_and_null_terminate(const char* path, int* size, void* mem_ctx)
{
	TINYTILED_UNUSED(mem_ctx);
	char* data = 0;
	FILE* fp = fopen(path, "rb");
	int sz = 0;

	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		sz = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		data = (char*)TINYTILED_MALLOC(sz + 1, mem_ctx);
		fread(data, sz, 1, fp);
		data[sz] = 0;
		fclose(fp);
	}

	if (size) *size = sz;
	return data;
}

tinytiled_map_t* tinytiled_load_map_from_file(const char* path, void* mem_ctx)
{
	int size;
	void* file = tinytiled_read_file_to_memory_and_null_terminate(path, &size, mem_ctx);
	tinytiled_map_t* map = tinytiled_load_map_from_memory(file, size, mem_ctx);
	TINYTILED_FREE(file, mem_ctx);
	return map;
}

#define TINYTILED_CHECK(X, Y) do { if (!(X)) { tinytiled_error_reason = Y; goto tinytiled_err; } } while (0)
#define TINYTILED_FAIL_IF(X) do { if (X) { goto tinytiled_err; } } while (0)

static int tinytiled_isspace(char c)
{
	return (c == ' ') |
		(c == '\t') |
		(c == '\n') |
		(c == '\v') |
		(c == '\f') |
		(c == '\r');
}

static char tinytiled_peak(tinytiled_map_internal_t* m)
{
	while (tinytiled_isspace(*m->in)) m->in++;
	return *m->in;
}

static char tinytiled_next(tinytiled_map_internal_t* m)
{
	char c;
	if (m->in == m->end) *(int*)0 = 0;
	while (tinytiled_isspace(c = *m->in++));
	return c;
}

static int tinytiled_try(tinytiled_map_internal_t* m, char expect)
{
	if (m->in == m->end) *(int*)0 = 0;
	if (tinytiled_peak(m) == expect)
	{
		m->in++;
		return 1;
	}
	return 0;
}

#define tinytiled_expect(m, expect) \
	do { \
		TINYTILED_CHECK(tinytiled_next(m) == expect, "Found unexpected token (is this a valid JSON file?)."); \
	} while (0)

char tinytiled_parse_char(char c)
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

static int tinytiled_read_string_internal(tinytiled_map_internal_t* m)
{
	tinytiled_expect(m, '"');

	int count = 0;
	int done = 0;
	while (!done)
	{
		TINYTILED_CHECK(count < TINYTILED_INTERNAL_BUFFER_MAX, "String exceeded max length of TINYTILED_INTERNAL_BUFFER_MAX.");
		char c = tinytiled_next(m);

		switch (c)
		{
		case '"':
			m->scratch[count] = 0;
			done = 1;
			break;

		case '\\':
		{
			char the_char = tinytiled_parse_char(tinytiled_next(m));
			m->scratch[count++] = the_char;
		}	break;

		default:
			m->scratch[count++] = c;
			break;
		}
	}

	m->scratch_len = count;
	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_read_string(m) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_read_string_internal(m)); \
	} while (0)

static int tinytiled_read_int_internal(tinytiled_map_internal_t* m, int* out)
{
	char* end;
	int val = (int)strtoll(m->in, &end, 10);
	TINYTILED_CHECK(m->in != end, "Invalid integer found during parse.");
	m->in = end;
	*out = val;
	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_read_int(m, num) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_read_int_internal(m, num)); \
	} while (0)

static int tinytiled_read_hex_int_internal(tinytiled_map_internal_t* m, int* out)
{
	switch (tinytiled_peak(m))
	{
	case '#':
		tinytiled_next(m);
		break;

	case '0':
	{
		tinytiled_next(m);
		char c = tinytiled_next(m);
		TINYTILED_CHECK((c == 'x') | (c == 'X'), "Expected 'x' or 'X' while parsing a hex number.");
	}	break;
	}

	char* end;
	int val = strtol(m->in, &end, 16);
	TINYTILED_CHECK(m->in != end, "Invalid integer found during parse.");
	m->in = end;
	*out = val;
	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_read_hex_int(m, num) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_read_hex_int_internal(m, num)); \
	} while (0)

static int tinytiled_read_float_internal(tinytiled_map_internal_t* m, float* out)
{
	char* end;
	float val = (float)strtod(m->in, &end);
	TINYTILED_CHECK(m->in != end, "Invalid integer found during parse.");
	m->in = end;
	*out = val;
	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_read_float(m, num) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_read_float_internal(m, num)); \
	} while (0)

static int tinytiled_read_bool_internal(tinytiled_map_internal_t* m, int* out)
{
	if ((tinytiled_peak(m) == 't') | (tinytiled_peak(m) == 'T'))
	{
		m->in += 4;
		*out = 1;
	}

	else if ((tinytiled_peak(m) == 'f') | (tinytiled_peak(m) == 'F'))
	{
		m->in += 5;
		*out = 0;
	}

	else goto tinytiled_err;

	TINYTILED_CHECK(m->in <= m->end, "Attempted to read passed input buffer (is this a valid JSON file?).");
	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_read_bool(m, b) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_read_bool_internal(m, b)); \
	} while (0)

int tinytiled_read_csv_integers_internal(tinytiled_map_internal_t* m, int* count_out, int** out)
{
	int count = 0;
	int capacity = 1024;
	int* integers = (int*)TINYTILED_MALLOC(capacity * sizeof(int), m->mem_ctx);

	char c;
	do
	{
		int val;
		tinytiled_read_int(m, &val);
		if (count == capacity)
		{
			capacity *= 2;
			int* new_integers = (int*)TINYTILED_MALLOC(capacity * sizeof(int), m->mem_ctx);
			TINYTILED_MEMCPY(new_integers, integers, sizeof(int) * count);
			TINYTILED_FREE(integers, m->mem_ctx);
			integers = new_integers;
		}
		integers[count++] = val;
		c = tinytiled_next(m);
	}
	while (c != ']');

	*count_out = count;
	*out = integers;
	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_read_csv_integers(m, count_out, out) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_read_csv_integers_internal(m, count_out, out)); \
	} while (0)

int tinytiled_intern_string_internal(tinytiled_map_internal_t* m, tinytiled_string_t* out)
{
	tinytiled_read_string(m);

	// Store string id inside the memory of the pointer. This is important since
	// the string pool can relocate strings while parsing the map file at any
	// time.

	// Later there will be a second pass to patch all these string
	// pointers by doing: *out = (const char*)strpool_cstr(&m->strpool, id);

	STRPOOL_U64 id = strpool_inject(&m->strpool, m->scratch, m->scratch_len);
	// if (sizeof(const char*) < sizeof(STRPOOL_U64)) *(int*)0 = 0; // sanity check
	out->hash_id = id;

	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_intern_string(m, out) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_intern_string_internal(m, out)); \
	} while (0)

int tinytiled_read_vertex_array_internal(tinytiled_map_internal_t* m, int* out_count, float** out_verts)
{
	tinytiled_expect(m, '[');

	int vert_count = 0;
	int capacity = 32;
	float* verts = (float*)TINYTILED_MALLOC(sizeof(float) * capacity * 2, m->mem_ctx);

	while (tinytiled_peak(m) != ']')
	{
		tinytiled_expect(m, '{');
		tinytiled_expect(m, '"');

		int swap = tinytiled_try(m, 'x') ? 0 : 1;
		float x = 0, y = 0;
		tinytiled_expect(m, '"');
		tinytiled_expect(m, ':');
		tinytiled_read_float(m, swap ? &y : &x);
		tinytiled_expect(m, ',');
		tinytiled_expect(m, '"');
		tinytiled_expect(m, swap ? 'y' : 'x');
		tinytiled_expect(m, '"');
		tinytiled_expect(m, ':');
		tinytiled_read_float(m, swap ? &x : &y);
		tinytiled_expect(m, '}');
		tinytiled_try(m, ',');

		if (vert_count == capacity)
		{
			capacity *= 2;
			float* new_verts = (float*)TINYTILED_MALLOC(sizeof(float) * capacity * 2, m->mem_ctx);
			TINYTILED_MEMCPY(new_verts, verts, sizeof(float) * 2 * vert_count);
			TINYTILED_FREE(verts, m->mem_ctx);
			verts = new_verts;
		}

		verts[vert_count * 2] = x;
		verts[vert_count * 2 + 1] = y;
		++vert_count;
	}

	tinytiled_expect(m, ']');
	*out_count = vert_count;
	*out_verts = verts;

	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_read_vertex_array(m, out_count, out_verts) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_read_vertex_array_internal(m, out_count, out_verts)); \
	} while (0)

int tinytiled_read_properties_internal(tinytiled_map_internal_t* m, tinytiled_property_t** out_properties, int* out_count)
{
	int count = 0;
	int capacity = 32;
	tinytiled_property_t* props = (tinytiled_property_t*)TINYTILED_MALLOC(capacity * sizeof(tinytiled_property_t), m->mem_ctx);

	tinytiled_expect(m, '{');

	while (tinytiled_peak(m) != '}')
	{
		tinytiled_property_t prop;
		prop.type = TINYTILED_PROPERTY_NONE;

		tinytiled_intern_string(m, &prop.name);
		tinytiled_expect(m, ':');

		char c = tinytiled_peak(m);

		if (((c == 't') | (c == 'T')) | ((c == 'f') | (c == 'F')))
		{
			tinytiled_read_bool(m, &prop.data.boolean);
			prop.type = TINYTILED_PROPERTY_BOOL;
		}

		else if (c == '"')
		{
			char* s = m->in + 1;
			int is_hex_color = 1;

			if (*s++ != '#') is_hex_color = 0;
			else 
			{
				while ((c = *s++) != '"')
				{
					switch (c)
					{
					case '0': case '1': case '2': case '3': case '4': case '5': case '6':
					case '7': case '8': case '9': case 'a': case 'b': case 'c': case 'd':
					case 'e': case 'f': case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
						break;

					case '\\':
						++s;
						break;

					default:
						is_hex_color = 0;
						break;
					}

					if (!is_hex_color) break;
				}
			}

			if (is_hex_color)
			{
				tinytiled_expect(m, '"');
				tinytiled_read_hex_int(m, &prop.data.integer);
				tinytiled_expect(m, '"');
				prop.type = TINYTILED_PROPERTY_COLOR;
			}

			else
			{
				tinytiled_intern_string(m, &prop.data.string);
				prop.type = TINYTILED_PROPERTY_STRING;
			}
		}

		else
		{
			char* s = m->in;
			int is_float = 0;
			while ((c = *s++) != ',')
			{
				if (c == '.')
				{
					is_float = 1;
					break;
				}
			}

			if (is_float)
			{
				tinytiled_read_float(m, &prop.data.floating);
				prop.type = TINYTILED_PROPERTY_FLOAT;
			}

			else
			{
				tinytiled_read_int(m, &prop.data.integer);
				prop.type = TINYTILED_PROPERTY_INT;
			}
		}

		if (count == capacity)
		{
			capacity *= 2;
			tinytiled_property_t* new_props = (tinytiled_property_t*)TINYTILED_MALLOC(capacity * sizeof(tinytiled_property_t), m->mem_ctx);
			TINYTILED_MEMCPY(new_props, props, sizeof(tinytiled_property_t) * count);
			TINYTILED_FREE(props, m->mem_ctx);
			props = new_props;
		}
		props[count++] = prop;

		tinytiled_try(m, ',');
	}

	tinytiled_expect(m, '}');
	tinytiled_expect(m, ',');
	tinytiled_read_string(m); // should be "properytypes"
	const char* propertytypes = "propertytypes";
	for (int i = 0; i < m->scratch_len; ++i) TINYTILED_CHECK(m->scratch[i] == propertytypes[i], "Expected \"propertytypes\" string here.");
	tinytiled_expect(m, ':');
	tinytiled_expect(m, '{');
	while (tinytiled_next(m) != '}'); // skip propertytypes since it's not needed
	tinytiled_try(m, ',');

	*out_properties = props;
	*out_count = count;

	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_read_properties(m, out_properties, out_count) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_read_properties_internal(m, out_properties, out_count)); \
	} while (0)

tinytiled_object_t* tinytiled_read_object(tinytiled_map_internal_t* m)
{
	tinytiled_object_t* object = (tinytiled_object_t*)tinytiled_alloc(m, sizeof(tinytiled_object_t));
	TINYTILED_MEMSET(object, 0, sizeof(tinytiled_object_t));
	tinytiled_expect(m, '{');

	while (tinytiled_peak(m) != '}')
	{
		tinytiled_read_string(m);
		tinytiled_expect(m, ':');
		TINYTILED_U64 h = tinytiled_FNV1a(m->scratch, m->scratch_len + 1);

		switch (h)
		{
		case 14479365350473253539U: // ellipse
			tinytiled_read_bool(m, &object->ellipse);
			break;

		case 14992147199312073281U: // gid
			tinytiled_read_int(m, &object->gid);
			break;

		case 809651598226485190U: // height
			tinytiled_read_int(m, &object->height);
			break;

		case 3133932603199444032U: // id
			tinytiled_read_int(m, &object->id);
			break;

		case 12661511911333414066U: // name
			tinytiled_intern_string(m, &object->name);
			break;

		case 15925463322410838979U: // point
			tinytiled_read_bool(m, &object->point);
			break;

		case 11191351929714760271U: // polyline
			tinytiled_read_vertex_array(m, &object->vert_count, &object->vertices);
			object->vert_type = 0;
			break;

		case 6623316362411997547U: // polygon
			tinytiled_read_vertex_array(m, &object->vert_count, &object->vertices);
			object->vert_type = 1;
			break;

		case 8368542207491637236U: // properties
			tinytiled_read_properties(m, &object->properties, &object->property_count);
			break;

		case 17386473859969670701U: // rotation
			tinytiled_read_float(m, &object->rotation);
			break;

		case 7758770083360183834U: // text
			TINYTILED_WARNING("Text field of Tiled objects is not yet supported.");
			while (tinytiled_peak(m) != '}') tinytiled_next(m);
			tinytiled_expect(m, '}');
			break;

		case 13509284784451838071U: // type
			tinytiled_intern_string(m, &object->type);
			break;

		case 128234417907068947U: // visible
			tinytiled_read_bool(m, &object->visible);
			break;

		case 7400839267610537869U: // width
			tinytiled_read_int(m, &object->width);
			break;

		case 644252274336276709U: // x
			tinytiled_read_float(m, &object->x);
			break;

		case 643295699219922364U: // y
			tinytiled_read_float(m, &object->y);
			break;

		default:
			TINYTILED_CHECK(0, "Unknown identifier found.");
		}

		tinytiled_try(m, ',');
	}

	tinytiled_expect(m, '}');
	return object;

tinytiled_err:
	return 0;
}

tinytiled_layer_t* tinytiled_layers(tinytiled_map_internal_t* m)
{
	tinytiled_layer_t* layer = (tinytiled_layer_t*)tinytiled_alloc(m, sizeof(tinytiled_layer_t));
	TINYTILED_MEMSET(layer, 0, sizeof(tinytiled_layer_t));
	tinytiled_expect(m, '{');

	while (tinytiled_peak(m) != '}')
	{
		tinytiled_read_string(m);
		tinytiled_expect(m, ':');
		TINYTILED_U64 h = tinytiled_FNV1a(m->scratch, m->scratch_len + 1);

		switch (h)
		{
		case 14868627273436340303U: // compression
			TINYTILED_CHECK(0, "Compression is not yet supported. The expected tile format is CSV (uncompressed). Please see the docs if you are interested in compression.");
			break;

		case 4430454992770877055U: // data
			TINYTILED_CHECK(tinytiled_peak(m) == '[', "The expected tile format is CSV (uncompressed). It looks like Base64 (uncompressed) was selected. Please see the docs if you are interested in compression.");
			tinytiled_expect(m, '[');
			tinytiled_read_csv_integers(m, &layer->data_count, &layer->data);
			break;

		case 1888774307506158416U: // encoding
			TINYTILED_CHECK(0, "Encoding is not yet supported. The expected tile format is CSV (uncompressed). Please see the docs if you are interested in compression.");
			break;

		case 2841939415665718447U: // draworder
			tinytiled_intern_string(m, &layer->draworder);
			break;

		case 809651598226485190U: // height
			tinytiled_read_int(m, &layer->height);
			break;

		case 4566956252693479661U: // layers
		tinytiled_expect(m, '[');

		while (tinytiled_peak(m) != ']')
		{
			tinytiled_layer_t* child_layer = tinytiled_layers(m);
			TINYTILED_FAIL_IF(!child_layer);
			child_layer->next = layer->layers;
			layer->layers = child_layer;
			tinytiled_try(m, ',');
		}

		tinytiled_expect(m, ']');
		break;

		case 12661511911333414066U: // name
			tinytiled_intern_string(m, &layer->name);
			break;

		case 107323337999513585U: // objects
			tinytiled_expect(m, '[');

			while (tinytiled_peak(m) != ']')
			{
				tinytiled_object_t* object = tinytiled_read_object(m);
				TINYTILED_FAIL_IF(!object);
				object->next = layer->objects;
				layer->objects = object;
				tinytiled_try(m, ',');
			}

			tinytiled_expect(m, ']');
			break;

		case 11746902372727406098U: // opacity
			tinytiled_read_float(m, &layer->opacity);
			break;

		case 8368542207491637236U: // properties
			tinytiled_read_properties(m, &layer->properties, &layer->property_count);
			break;

		case 13509284784451838071U: // type
			tinytiled_intern_string(m, &layer->type);
			break;

		case 128234417907068947U: // visible
			tinytiled_read_bool(m, &layer->visible);
			break;

		case 7400839267610537869U: // width
			tinytiled_read_int(m, &layer->width);
			break;

		case 644252274336276709U: // x
			tinytiled_read_int(m, &layer->x);
			break;

		case 643295699219922364U: // y
			tinytiled_read_int(m, &layer->y);
			break;

		default:
			TINYTILED_CHECK(0, "Unknown identifier found.");
		}

		tinytiled_try(m, ',');
	}

	tinytiled_expect(m, '}');
	return layer;

tinytiled_err:
	return 0;
}

tinytiled_tileset_t* tinytiled_tileset(tinytiled_map_internal_t* m)
{
	tinytiled_tileset_t* tileset = (tinytiled_tileset_t*)tinytiled_alloc(m, sizeof(tinytiled_tileset_t));
	TINYTILED_MEMSET(tileset, 0, sizeof(tinytiled_tileset_t));
	tinytiled_expect(m, '{');

	while (tinytiled_peak(m) != '}')
	{
		tinytiled_read_string(m);
		tinytiled_expect(m, ':');
		TINYTILED_U64 h = tinytiled_FNV1a(m->scratch, m->scratch_len + 1);

		switch (h)
		{
		case 12570673734542705940U: // columns
			tinytiled_read_int(m, &tileset->columns);
			break;

		case 13956389100366699181U: // firstgid
			tinytiled_read_int(m, &tileset->firstgid);
			break;

		case 13522647194774232494U: // image
			tinytiled_intern_string(m, &tileset->image);
			break;

		case 7796197983149768626U: // imagewidth
			tinytiled_read_int(m, &tileset->imagewidth);
			break;

		case 2114495263010514843U: // imageheight
			tinytiled_read_int(m, &tileset->imageheight);
			break;

		case 4864566659847942049U: // margin
			tinytiled_read_int(m, &tileset->margin);
			break;

		case 12661511911333414066U: // name
			tinytiled_intern_string(m, &tileset->name);
			break;

		case 8368542207491637236U: // properties
			tinytiled_read_properties(m, &tileset->properties, &tileset->property_count);
			break;

		case 6491372721122724890U: // spacing
			tinytiled_read_int(m, &tileset->spacing);
			break;

		case 4065097716972592720U: // tilecount
			tinytiled_read_int(m, &tileset->tilecount);
			break;

		case 13337683360624280154U: // tileheight
			tinytiled_read_int(m, &tileset->tileheight);
			break;

		case 6504415465426505561U: // tilewidth
			tinytiled_read_int(m, &tileset->tilewidth);
			break;

		case 13509284784451838071U: // type
			tinytiled_intern_string(m, &tileset->type);
			break;

		default:
			TINYTILED_CHECK(0, "Unknown identifier found.");
		}

		tinytiled_try(m, ',');
	}

	tinytiled_expect(m, '}');
	return tileset;

tinytiled_err:
	return 0;
}

static int tinytiled_dispatch_map_internal(tinytiled_map_internal_t* m)
{
	tinytiled_read_string(m);
	tinytiled_expect(m, ':');
	TINYTILED_U64 h = tinytiled_FNV1a(m->scratch, m->scratch_len + 1);

 	switch (h)
	{
	case 17465100621023921744U: // backgroundcolor
		tinytiled_expect(m, '"');
		tinytiled_read_hex_int(m, &m->map.backgroundcolor);
		tinytiled_expect(m, '"');
		break;

	case 809651598226485190U: // height
		tinytiled_read_int(m, &m->map.height);
		break;

	case 16529928297377797591U: // infinite
		tinytiled_read_bool(m, &m->map.infinite);
		break;

	case 4566956252693479661U: // layers
		tinytiled_expect(m, '[');

		while (tinytiled_peak(m) != ']')
		{
			tinytiled_layer_t* layer = tinytiled_layers(m);
			TINYTILED_FAIL_IF(!layer);
			layer->next = m->map.layers;
			m->map.layers = layer;
			tinytiled_try(m, ',');
		}

		tinytiled_expect(m, ']');
		break;

	case 11291153769551921430U: // nextobjectid
		tinytiled_read_int(m, &m->map.nextobjectid);
		break;

	case 563935667693078739U: // orientation
		tinytiled_intern_string(m, &m->map.orientation);
		break;

	case 8368542207491637236U: // properties
		tinytiled_read_properties(m, &m->map.properties, &m->map.property_count);
		break;

	case 16693886730115578029U: // renderorder
		tinytiled_intern_string(m, &m->map.renderorder);
		break;

	case 1007832939408977147U: // tiledversion
		tinytiled_intern_string(m, &m->map.tiledversion);
		break;

	case 13337683360624280154U: // tileheight
		tinytiled_read_int(m, &m->map.tileheight);
		break;

	case 8310322674355535532U: // tilesets
	{
		tinytiled_expect(m, '[');

		while (tinytiled_peak(m) != ']')
		{
			tinytiled_tileset_t* tileset = tinytiled_tileset(m);
			tileset->next = m->map.tilesets;
			m->map.tilesets = tileset;
			tinytiled_try(m, ',');
		}

		tinytiled_expect(m, ']');
	}	break;

	case 6504415465426505561U: // tilewidth
		tinytiled_read_int(m, &m->map.tilewidth);
		break;

	case 13509284784451838071U: // type
		tinytiled_intern_string(m, &m->map.type);
		break;

	case 8196820454517111669U: // version
		tinytiled_read_int(m, &m->map.version);
		break;

	case 7400839267610537869U: // width
		tinytiled_read_int(m, &m->map.width);
		break;

	default:
		TINYTILED_CHECK(0, "Unknown identifier found.");
	}

	return 1;

tinytiled_err:
	return 0;
}

#define tinytiled_dispatch_map(m) \
	do { \
		TINYTILED_FAIL_IF(!tinytiled_dispatch_map_internal(m)); \
	} while (0)

static TINYTILED_INLINE void tinytiled_string_deintern(tinytiled_map_internal_t* m, tinytiled_string_t* s)
{
	s->ptr = strpool_cstr(&m->strpool, s->hash_id);
}

static void tinytiled_deintern_properties(tinytiled_map_internal_t* m, tinytiled_property_t* properties, int property_count)
{
	for (int i = 0; i < property_count; ++i)
	{
		tinytiled_property_t* p = properties + i;
		tinytiled_string_deintern(m, &p->name);
		if (p->type == TINYTILED_PROPERTY_STRING) tinytiled_string_deintern(m, &p->data.string);
	}
}

static void tinytiled_deintern_layer(tinytiled_map_internal_t* m, tinytiled_layer_t* layer)
{
	while (layer)
	{
		tinytiled_string_deintern(m, &layer->draworder);
		tinytiled_string_deintern(m, &layer->name);
		tinytiled_string_deintern(m, &layer->type);
		tinytiled_deintern_properties(m, layer->properties, layer->property_count);

		tinytiled_object_t* object = layer->objects;
		while (object)
		{
			tinytiled_string_deintern(m, &object->name);
			tinytiled_string_deintern(m, &object->type);
			tinytiled_deintern_properties(m, object->properties, object->property_count);
			object = object->next;
		}

		tinytiled_deintern_layer(m, layer->layers);

		layer = layer->next;
	}
}

static void tinytiled_patch_interned_strings(tinytiled_map_internal_t* m)
{
	tinytiled_string_deintern(m, &m->map.orientation);
	tinytiled_string_deintern(m, &m->map.renderorder);
	tinytiled_string_deintern(m, &m->map.tiledversion);
	tinytiled_string_deintern(m, &m->map.type);
	tinytiled_deintern_properties(m, m->map.properties, m->map.property_count);

	tinytiled_tileset_t* tileset = m->map.tilesets;
	while (tileset)
	{
		tinytiled_string_deintern(m, &tileset->image);
		tinytiled_string_deintern(m, &tileset->name);
		tinytiled_string_deintern(m, &tileset->type);
		tinytiled_deintern_properties(m, tileset->properties, tileset->property_count);
		tileset = tileset->next;
	}

	tinytiled_layer_t* layer = m->map.layers;
	tinytiled_deintern_layer(m, layer);
}

static void tinytiled_free_map_internal(tinytiled_map_internal_t* m)
{
	strpool_term(&m->strpool);

	tinytiled_page_t* page = m->pages;
	while (page)
	{
		tinytiled_page_t* next = page->next;
		TINYTILED_FREE(page, m->mem_ctx);
		page = next;
	}

	TINYTILED_FREE(m, m->mem_ctx);
}

tinytiled_map_t* tinytiled_load_map_from_memory(const void* memory, int size_in_bytes, void* mem_ctx)
{
	tinytiled_map_internal_t* m = (tinytiled_map_internal_t*)TINYTILED_MALLOC(sizeof(tinytiled_map_internal_t), mem_ctx);
	TINYTILED_MEMSET(m, 0, sizeof(tinytiled_map_internal_t));
	m->in = (char*)memory;
	m->end = m->in + size_in_bytes;
	m->mem_ctx = mem_ctx;
	m->page_size = 1024 * 10;
	m->bytes_left_on_page = m->page_size;
	m->pages = (tinytiled_page_t*)TINYTILED_MALLOC(sizeof(tinytiled_page_t) + m->page_size, mem_ctx);
	m->pages->next = 0;
	m->pages->data = m->pages + 1;
	strpool_config_t config = strpool_default_config;
	config.memctx = mem_ctx;
	strpool_init(&m->strpool, &config);

	tinytiled_expect(m, '{');

	while (tinytiled_peak(m) != '}')
	{
		tinytiled_dispatch_map(m);
		tinytiled_try(m, ',');
	}

	tinytiled_expect(m, '}');

	tinytiled_patch_interned_strings(m);
	return &m->map;

tinytiled_err:
	tinytiled_free_map_internal(m);
	TINYTILED_WARNING(tinytiled_error_reason);
	return 0;
}

void tinytiled_free_map(tinytiled_map_t* map)
{
	tinytiled_map_internal_t* m = (tinytiled_map_internal_t*)(((char*)map) - (size_t)(&((tinytiled_map_internal_t*)0)->map));
	tinytiled_free_map_internal(m);
}

#undef TINYTILED_IMPLEMENTATION
#endif

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
