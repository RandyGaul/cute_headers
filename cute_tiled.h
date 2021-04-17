/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_tiled.h - v1.06

	To create implementation (the function definitions)
		#define CUTE_TILED_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY

		Parses Tiled (http://www.mapeditor.org/) files saved as the JSON file
		format. See http://doc.mapeditor.org/en/latest/reference/json-map-format/
		for a complete description of the JSON Tiled format. An entire map file
		is loaded up in entirety and used to fill in a set of structs. The entire
		struct collection is then handed to the user.

		This header is up to date with Tiled's documentation Revision 40049fd5 and
		verified to work with Tiled stable version 1.4.1.
		http://doc.mapeditor.org/en/latest/reference/json-map-format/

		Here is a past discussion thread on this header:
		https://www.reddit.com/r/gamedev/comments/87680n/cute_tiled_tiled_json_map_parserloader_in_c/

	Revision history:
		1.00 (03/24/2018) initial release
		1.01 (05/04/2018) tile descriptors in tilesets for collision geometry
		1.02 (05/07/2018) reverse lists for ease of use, incorporate fixes by ZenToad
		1.03 (01/11/2019) support for Tiled 1.2.1 with the help of dpeter99 and tanis2000
		1.04 (04/30/2020) support for Tiled 1.3.3 with the help of aganm
		1.05 (07/19/2020) support for Tiled 1.4.1 and tileset tile animations
		1.06 (04/05/2021) support for Tiled 1.5.0 parallax
*/

/*
	Contributors:
		ZenToad           1.02 - Bug reports and goto statement errors for g++
		dpeter99          1.03 - Help with updating to Tiled 1.2.1 JSON format
		tanis2000         1.03 - Help with updating to Tiled 1.2.1 JSON format
		aganm             1.04 - Help with updating to Tiled 1.3.3 JSON format
*/

/*
	DOCUMENTATION

		Load up a Tiled json exported file, either from disk or memory, like so:

			cute_tiled_map_t* map = cute_tiled_load_map_from_memory(memory, size, 0);

		Then simply access the map's fields like so:

			// get map width and height
			int w = map->width;
			int h = map->height;

			// loop over the map's layers
			cute_tiled_layer_t* layer = map->layers;
			while (layer)
			{
				int* data = layer->data;
				int data_count = layer->data_count;

				// do something with the tile data
				UserFunction_HandleTiles(data, data_count);

				layer = layer->next;
			}

		Finally, free it like so:

			cute_tiled_free_map(map);

	LIMITATIONS

		More uncommon fields are not supported, and are annotated in this header.
		Search for "Not currently supported." without quotes to find them. cute_tiled
		logs a warning whenever a known unsupported field is encountered, and will
		attempt to gracefully skip the field. If a field with completely unknown
		syntax is encountered (which can happen if cute_tiled is used on a newer
		and unsupported version of Tiled), undefined behavior may occur (crashes).

		If you would like a certain feature to be supported simply open an issue on
		GitHub and provide a JSON exported map with the unsupported features. Changing
		the parser to support new fields and objects is quite easy, as long as a map
		file is provided for debugging and testing!

		GitHub : https://github.com/RandyGaul/cute_headers/

		Compression of the tile GIDs is *not* supported in this header. Exporting
		a map from Tiled will create a JSON file. This JSON file itself can very
		trivially be compressed in its entirety, thus making Tiled's internal
		compression exporting not a useful feature for this header to support.
		Simply wrap calls to `cute_tiled_load_map_from_file` in a decompression
		routine. Here is an example (assuming `zlib_uncompress` is already imp-
		lemented somewhere in the user's codebase):

			int size;
			void* uncompressed_data = zlib_uncompress(path_to_zipped_map_file, &size);
			cute_tiled_map_t* map = cute_tiled_load_map_from_memory(uncompressed_data, size, 0);
*/

#if !defined(CUTE_TILED_H)

// Read this in the event of errors
extern const char* cute_tiled_error_reason;
extern int cute_tiled_error_line;


typedef struct cute_tiled_map_t cute_tiled_map_t;
typedef struct cute_tiled_tileset_t cute_tiled_tileset_t;

/*!
 * Load a map from disk, placed into heap allocated memory. \p mem_ctx can be
 * NULL. It is used for custom allocations.
 */
cute_tiled_map_t* cute_tiled_load_map_from_file(const char* path, void* mem_ctx);

/*!
 * Load a map from memory. \p mem_ctx can be NULL. It is used for custom allocations.
 */
cute_tiled_map_t* cute_tiled_load_map_from_memory(const void* memory, int size_in_bytes, void* mem_ctx);

/*!
 * Reverses the layers order, so they appear in reverse-order from what is shown in the Tiled editor.
 */
void cute_tiled_reverse_layers(cute_tiled_map_t* map);

/*!
 * Free all dynamic memory associated with this map.
 */
void cute_tiled_free_map(cute_tiled_map_t* map);

/*!
 * Load an external tileset from disk, placed into heap allocated memory. \p mem_ctx can be
 * NULL. It is used for custom allocations.
 *
 * Please note this function is *entirely optional*, and only useful if you want to intentionally
 * load tilesets externally from your map. If so, please also consider defining
 * `CUTE_TILED_NO_EXTERNAL_TILESET_WARNING` to disable warnings about missing embedded tilesets.
 */
cute_tiled_tileset_t* cute_tiled_load_external_tileset(const char* path, void* mem_ctx);

/*!
 * Load an external tileset from memory. \p mem_ctx can be NULL. It is used for custom allocations.
 *
 * Please note this function is *entirely optional*, and only useful if you want to intentionally
 * load tilesets externally from your map. If so, please also consider defining
 * `CUTE_TILED_NO_EXTERNAL_TILESET_WARNING` to disable warnings about missing embedded tilesets.
 */
cute_tiled_tileset_t* cute_tiled_load_external_tileset_from_memory(const void* memory, int size_in_bytes, void* mem_ctx);

/*!
 * Free all dynamic memory associated with this external tileset.
 */
void cute_tiled_free_external_tileset(cute_tiled_tileset_t* tileset);

#if !defined(CUTE_TILED_U64)
	#define CUTE_TILED_U64 unsigned long long
#endif

#if !defined(CUTE_TILED_INLINE)
	#if defined(_MSC_VER)
		#define CUTE_TILED_INLINE __inline
	#else
		#define CUTE_TILED_INLINE __inline__
	#endif
#endif

typedef struct cute_tiled_layer_t cute_tiled_layer_t;
typedef struct cute_tiled_object_t cute_tiled_object_t;
typedef struct cute_tiled_frame_t cute_tiled_frame_t;
typedef struct cute_tiled_tile_descriptor_t cute_tiled_tile_descriptor_t;
typedef struct cute_tiled_tileset_t cute_tiled_tileset_t;
typedef struct cute_tiled_property_t cute_tiled_property_t;
typedef union cute_tiled_string_t cute_tiled_string_t;

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
union cute_tiled_string_t
{
	const char* ptr;
	CUTE_TILED_U64 hash_id;
};

typedef enum CUTE_TILED_PROPERTY_TYPE
{
	CUTE_TILED_PROPERTY_NONE,
	CUTE_TILED_PROPERTY_INT,
	CUTE_TILED_PROPERTY_BOOL,
	CUTE_TILED_PROPERTY_FLOAT,
	CUTE_TILED_PROPERTY_STRING,

	// Note: currently unused! File properties are reported as strings in
	// this header, and it is up to users to know a-priori which strings
	// contain file paths.
	CUTE_TILED_PROPERTY_FILE,

	CUTE_TILED_PROPERTY_COLOR,
} CUTE_TILED_PROPERTY_TYPE;

struct cute_tiled_property_t
{
	union
	{
		int integer;
		int boolean;
		float floating;
		cute_tiled_string_t string;
		cute_tiled_string_t file;
		int color;
	} data;
	CUTE_TILED_PROPERTY_TYPE type;
	cute_tiled_string_t name;
};

struct cute_tiled_object_t
{
	int ellipse;                         // 0 or 1. Used to mark an object as an ellipse.
	int gid;                             // GID, only if object comes from a Tilemap.
	float height;                        // Height in pixels. Ignored if using a gid.
	int id;                              // Incremental id - unique across all objects.
	cute_tiled_string_t name;            // String assigned to name field in editor.
	int point;                           // 0 or 1. Used to mark an object as a point.

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
	float* vertices;                     // Represents both type `polyline` and `polygon`.
	int vert_type;                       // 1 for `polygon` and 0 for `polyline`.

	int property_count;                  // Number of elements in the `properties` array.
	cute_tiled_property_t* properties;   // Array of properties.
	float rotation;                      // Angle in degrees clockwise.
	/* template */                       // Not currently supported.
	/* text */                           // Not currently supported.
	cute_tiled_string_t type;            // String assigned to type field in editor.
	int visible;                         // 0 or 1. Whether object is shown in editor.
	float width;                         // Width in pixels. Ignored if using a gid.
	float x;                             // x coordinate in pixels.
	float y;                             // y coordinate in pixels.
	cute_tiled_object_t* next;           // Pointer to next object. NULL if final object.
};

/*!
 * Example of using both helper functions below to process the `data` pointer of a layer,
 * containing an array of `GID`s.
 *
 * for (int i = 0; i < layer->data_count; i++)
 * {
 *     int hflip, vflip, dflip;
 *     int tile_id = layer->data[i];
 *     cute_tiled_get_flags(tile_id, &hflip, &vflip, &dflip);
 *     tile_id = cute_tiled_unset_flags(tile_id);
 *     DoSomethingWithFlags(tile_id, flip, vflip, dlfip);
 *     DoSomethingWithTileID(tile_id);
 * }
 */

#define CUTE_TILED_FLIPPED_HORIZONTALLY_FLAG 0x80000000
#define CUTE_TILED_FLIPPED_VERTICALLY_FLAG   0x40000000
#define CUTE_TILED_FLIPPED_DIAGONALLY_FLAG   0x20000000

/*!
 * Helper for processing tile data in /ref `cute_tiled_layer_t` `data`. Unsets all of
 * the image flipping flags in the higher bit of /p `tile_data_gid`.
 */
static CUTE_TILED_INLINE int cute_tiled_unset_flags(int tile_data_gid)
{
	const int flags = ~(CUTE_TILED_FLIPPED_HORIZONTALLY_FLAG | CUTE_TILED_FLIPPED_VERTICALLY_FLAG | CUTE_TILED_FLIPPED_DIAGONALLY_FLAG);
	return tile_data_gid & flags;
}

/*!
 * Helper for processing tile data in /ref `cute_tiled_layer_t` `data`. Flags are
 * stored in the GID array `data` for flipping the image. Retrieves all three flag types.
 */
static CUTE_TILED_INLINE void cute_tiled_get_flags(int tile_data_gid, int* flip_horizontal, int* flip_vertical, int* flip_diagonal)
{
	*flip_horizontal = !!(tile_data_gid & CUTE_TILED_FLIPPED_HORIZONTALLY_FLAG);
	*flip_vertical = !!(tile_data_gid & CUTE_TILED_FLIPPED_VERTICALLY_FLAG);
	*flip_diagonal = !!(tile_data_gid & CUTE_TILED_FLIPPED_DIAGONALLY_FLAG);
}

struct cute_tiled_layer_t
{
	/* chunks */                         // Not currently supported.
	/* compression; */                   // Not currently supported.
	int data_count;                      // Number of integers in `data`.
	int* data;                           // Array of GIDs. `tilelayer` only. Only support CSV style exports.
	cute_tiled_string_t draworder;       // `topdown` (default) or `index`. `objectgroup` only.
	/* encoding; */                      // Not currently supported.
	int height;                          // Row count. Same as map height for fixed-size maps.
	cute_tiled_layer_t* layers;          // Linked list of layers. Only appears if `type` is `group`.
	cute_tiled_string_t name;            // Name assigned to this layer.
	cute_tiled_object_t* objects;        // Linked list of objects. `objectgroup` only.
	float offsetx;                       // Horizontal layer offset.
	float offsety;                       // Vertical layer offset.
	float opacity;                       // Value between 0 and 1.
	int property_count;                  // Number of elements in the `properties` array.
	cute_tiled_property_t* properties;   // Array of properties.
	int transparentcolor;                // Hex-formatted color (#RRGGBB or #AARRGGBB) (optional).
	cute_tiled_string_t type;            // `tilelayer`, `objectgroup`, `imagelayer` or `group`.
	cute_tiled_string_t image;           // An image filepath. Used if layer is type `imagelayer`.
	int visible;                         // 0 or 1. Whether layer is shown or hidden in editor.
	int width;                           // Column count. Same as map width for fixed-size maps.
	int x;                               // Horizontal layer offset in tiles. Always 0.
	int y;                               // Vertical layer offset in tiles. Always 0.
	float parallaxx;                     // X axis parallax factor.
	float parallaxy;                     // Y axis parallax factor.
	int id;                              // ID of the layer.
	cute_tiled_layer_t* next;            // Pointer to the next layer. NULL if final layer.
};

struct cute_tiled_frame_t
{
	int duration;                        // Frame duration in milliseconds.
	int tileid;                          // Local tile ID representing this frame.
};

struct cute_tiled_tile_descriptor_t
{
	int tile_index;                      // ID of the tile local to the associated tileset.
	cute_tiled_string_t type;            // String assigned to type field in editor.
	int frame_count;                     // The number of animation frames in the `animation` array.
	cute_tiled_frame_t* animation;       // An array of `cute_tiled_frame_t`'s. Can be NULL.
	cute_tiled_string_t image;           // Image used for a tile in a tileset of type collection of images (relative path from map file to source image).
					     // Tileset is a collection of images if image.ptr isn't NULL.
	int imageheight;                     // Image height of a tile in a tileset of type collection of images.
	int imagewidth;                      // Image width of a tile in a tileset of type collection of images.
	cute_tiled_layer_t* objectgroup;     // Linked list of layers of type `objectgroup` only. Useful for holding collision info.
	int property_count;                  // Number of elements in the `properties` array.
	cute_tiled_property_t* properties;   // Array of properties.
	/* terrain */                        // Not currently supported.
	float probability;                   // The probability used when painting with the terrain brush in `Random Mode`.
	cute_tiled_tile_descriptor_t* next;  // Pointer to the next tile descriptor. NULL if final tile descriptor.
};

// IMPORTANT NOTE
// If your tileset is not embedded you will get a warning -- to disable this warning simply define
// this macro CUTE_TILED_NO_EXTERNAL_TILESET_WARNING.
//
// Here is an example.
//
//    #define CUTE_TILED_NO_EXTERNAL_TILESET_WARNING
//    #define CUTE_TILED_IMPLEMENTATION
//    #include <cute_tiled.h>
struct cute_tiled_tileset_t
{
	int backgroundcolor;                 // Hex-formatted color (#RRGGBB or #AARRGGBB) (optional).
	int columns;                         // The number of tile columns in the tileset.
	int firstgid;                        // GID corresponding to the first tile in the set.
	/* grid */                           // Not currently supported.
	cute_tiled_string_t image;           // Image used for tiles in this set (relative path from map file to source image).
	int imagewidth;                      // Width of source image in pixels.
	int imageheight;                     // Height of source image in pixels.
	int margin;                          // Buffer between image edge and first tile (pixels).
	cute_tiled_string_t name;            // Name given to this tileset.
	cute_tiled_string_t objectalignment; // Alignment to use for tile objects (unspecified (default), topleft, top, topright, left, center, right, bottomleft, bottom or bottomright) (since 1.4).
	int property_count;                  // Number of elements in the `properties` array.
	cute_tiled_property_t* properties;   // Array of properties.
	int spacing;                         // Spacing between adjacent tiles in image (pixels).
	/* terrains */                       // Not currently supported.
	int tilecount;                       // The number of tiles in this tileset.
	cute_tiled_string_t tiledversion;    // The Tiled version used to save the tileset.
	int tileheight;                      // Maximum height of tiles in this set.
	int tileoffset_x;                    // Pixel offset to align tiles to the grid.
	int tileoffset_y;                    // Pixel offset to align tiles to the grid.
	cute_tiled_tile_descriptor_t* tiles; // Linked list of tile descriptors. Can be NULL.
	int tilewidth;                       // Maximum width of tiles in this set.
	int transparentcolor;                // Hex-formatted color (#RRGGBB or #AARRGGBB) (optional).
	cute_tiled_string_t type;            // `tileset` (for tileset files, since 1.0).
	cute_tiled_string_t source;          // Relative path to tileset, when saved externally from the map file.
	cute_tiled_tileset_t* next;          // Pointer to next tileset. NULL if final tileset.
	float version;                       // The JSON format version (like 1.2).
	void* _internal;                     // For internal use only. Don't touch.
};

struct cute_tiled_map_t
{
	int backgroundcolor;                 // Hex-formatted color (#RRGGBB or #AARRGGBB) (optional).
	int height;                          // Number of tile rows.
	/* hexsidelength */                  // Not currently supported.
	int infinite;                        // Whether the map has infinite dimensions.
	cute_tiled_layer_t* layers;          // Linked list of layers. Can be NULL.
	int nextobjectid;                    // Auto-increments for each placed object.
	cute_tiled_string_t orientation;     // `orthogonal`, `isometric`, `staggered` or `hexagonal`.
	int property_count;                  // Number of elements in the `properties` array.
	cute_tiled_property_t* properties;   // Array of properties.
	cute_tiled_string_t renderorder;     // Rendering direction (orthogonal maps only).
	/* staggeraxis */                    // Not currently supported.
	/* staggerindex */                   // Not currently supported.
	cute_tiled_string_t tiledversion;    // The Tiled version used to save the file.
	int tileheight;                      // Map grid height.
	cute_tiled_tileset_t* tilesets;      // Linked list of tilesets.
	int tilewidth;                       // Map grid width.
	cute_tiled_string_t type;            // `map` (since 1.0).
	float version;                       // The JSON format version (like 1.2).
	int width;                           // Number of tile columns.
	int nextlayerid;                     // The ID of the following layer.
};

#define CUTE_TILED_H
#endif

#ifdef CUTE_TILED_IMPLEMENTATION
#ifndef CUTE_TILED_IMPLEMENTATION_ONCE
#define CUTE_TILED_IMPLEMENTATION_ONCE

#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
	#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#if !defined(CUTE_TILED_ALLOC)
	#include <stdlib.h>
	#define CUTE_TILED_ALLOC(size, ctx) malloc(size)
	#define CUTE_TILED_FREE(mem, ctx) free(mem)
#endif


#ifndef STRPOOL_EMBEDDED_MALLOC
	#define STRPOOL_EMBEDDED_MALLOC(ctx, size) CUTE_TILED_ALLOC(size, ctx)
#endif

#ifndef STRPOOL_EMBEDDED_FREE
	#define STRPOOL_EMBEDDED_FREE(ctx, ptr) CUTE_TILED_FREE(ptr, ctx)
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
        #define STRPOOL_EMBEDDED_STRNICMP( s1, s2, len ) ( _strnicmp( s1, s2, len ) )
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

#if !defined(CUTE_TILED_WARNING)
	#define CUTE_TILED_DEFAULT_WARNING
	#define CUTE_TILED_WARNING(msg) cute_tiled_warning(msg, __LINE__)
#endif

#if !defined(CUTE_TILED_MEMCPY)
	#include <string.h> // memcpy
	#define CUTE_TILED_MEMCPY memcpy
#endif

#if !defined(CUTE_TILED_MEMSET)
	#include <string.h> // memset
	#define CUTE_TILED_MEMSET memset
#endif

#if !defined(CUTE_TILED_UNUSED)
	#if defined(_MSC_VER)
		#define CUTE_TILED_UNUSED(x) (void)x
	#else
		#define CUTE_TILED_UNUSED(x) (void)(sizeof(x))
	#endif
#endif

#if !defined(CUTE_TILED_STDIO)
	#include <stdio.h>  // snprintf, fopen, fclose, etc.
	#define CUTE_TILED_SNPRINTF snprintf
	#define CUTE_TILED_SEEK_SET SEEK_SET
	#define CUTE_TILED_SEEK_END SEEK_END
	#define CUTE_TILED_FILE FILE
	#define CUTE_TILED_FOPEN fopen
	#define CUTE_TILED_FSEEK fseek
	#define CUTE_TILED_FREAD fread
	#define CUTE_TILED_FTELL ftell
	#define CUTE_TILED_FCLOSE fclose
#endif

int cute_tiled_error_cline; 			// The line in cute_tiled.h where the error was triggered.
const char* cute_tiled_error_reason; 		// The error message.
int cute_tiled_error_line;  			// The line where the error happened in the json.
const char* cute_tiled_error_file = NULL; 	// The filepath of the file being parsed. NULL if from memory.

#ifdef CUTE_TILED_DEFAULT_WARNING
	#include <stdio.h>

	void cute_tiled_warning(const char* warning, int line)
	{
		cute_tiled_error_cline = line;
		const char *error_file = cute_tiled_error_file ? cute_tiled_error_file : "MEMORY";
		printf("WARNING (cute_tiled.h:%i): %s (%s:%i)\n", cute_tiled_error_cline, warning, error_file, cute_tiled_error_line);
	}
#endif

#include <stdlib.h>

typedef struct cute_tiled_page_t cute_tiled_page_t;
typedef struct cute_tiled_map_internal_t cute_tiled_map_internal_t;

struct cute_tiled_page_t
{
	cute_tiled_page_t* next;
	void* data;
};

#define CUTE_TILED_INTERNAL_BUFFER_MAX 1024

struct cute_tiled_map_internal_t
{
	char* in;
	char* end;
	cute_tiled_map_t map;
	strpool_embedded_t strpool;
	void* mem_ctx;
	int page_size;
	int bytes_left_on_page;
	cute_tiled_page_t* pages;
	int scratch_len;
	char scratch[CUTE_TILED_INTERNAL_BUFFER_MAX];
};

void* cute_tiled_alloc(cute_tiled_map_internal_t* m, int size)
{
	if (size > m->page_size) return NULL; // Should never happen.
	if (m->bytes_left_on_page < size)
	{
		cute_tiled_page_t* page = (cute_tiled_page_t*)CUTE_TILED_ALLOC(sizeof(cute_tiled_page_t) + m->page_size, m->mem_ctx);
		if (!page) return 0;
		page->next = m->pages;
		page->data = page + 1;
		m->pages = page;
		m->bytes_left_on_page = m->page_size;
	}

	void* data = ((char*)m->pages->data) + (m->page_size - m->bytes_left_on_page);
	m->bytes_left_on_page -= size;
	return data;
}

CUTE_TILED_U64 cute_tiled_FNV1a(const void* buf, int len)
{
	CUTE_TILED_U64 h = (CUTE_TILED_U64)14695981039346656037U;
	const char* str = (const char*)buf;

	while (len--)
	{
		char c = *str++;
		h = h ^ (CUTE_TILED_U64)c;
		h = h * (CUTE_TILED_U64)1099511628211;
	}

	return h;
}

static char* cute_tiled_read_file_to_memory_and_null_terminate(const char* path, int* size, void* mem_ctx)
{
	CUTE_TILED_UNUSED(mem_ctx);
	char* data = 0;
	CUTE_TILED_FILE* fp = CUTE_TILED_FOPEN(path, "rb");
	int sz = 0;

	if (fp)
	{
		CUTE_TILED_FSEEK(fp, 0, CUTE_TILED_SEEK_END);
		sz = CUTE_TILED_FTELL(fp);
		CUTE_TILED_FSEEK(fp, 0, CUTE_TILED_SEEK_SET);
		data = (char*)CUTE_TILED_ALLOC(sz + 1, mem_ctx);
		CUTE_TILED_FREAD(data, sz, 1, fp);
		data[sz] = 0;
		CUTE_TILED_FCLOSE(fp);
	}

	if (size) *size = sz;
	return data;
}

cute_tiled_map_t* cute_tiled_load_map_from_file(const char* path, void* mem_ctx)
{
	cute_tiled_error_file = path;

	int size;
	void* file = cute_tiled_read_file_to_memory_and_null_terminate(path, &size, mem_ctx);
	if (!file) CUTE_TILED_WARNING("Unable to find map file.");
	cute_tiled_map_t* map = cute_tiled_load_map_from_memory(file, size, mem_ctx);
	CUTE_TILED_FREE(file, mem_ctx);

	cute_tiled_error_file = NULL;

	return map;
}

#define CUTE_TILED_CHECK(X, Y) do { if (!(X)) { cute_tiled_error_reason = Y; cute_tiled_error_cline = __LINE__; goto cute_tiled_err; } } while (0)
#define CUTE_TILED_FAIL_IF(X) do { if (X) { goto cute_tiled_err; } } while (0)

static int cute_tiled_isspace(char c)
{
	cute_tiled_error_line += c == '\n';

	return (c == ' ') |
		(c == '\t') |
		(c == '\n') |
		(c == '\v') |
		(c == '\f') |
		(c == '\r');
}

static char cute_tiled_peak(cute_tiled_map_internal_t* m)
{
	while (cute_tiled_isspace(*m->in)) m->in++;
	return *m->in;
}

#ifdef __clang__
    #define CUTE_TILED_CRASH() __builtin_trap()
#else
    #define CUTE_TILED_CRASH() *(int*)0 = 0
#endif

static char cute_tiled_next(cute_tiled_map_internal_t* m)
{
	char c;
	if (m->in == m->end) CUTE_TILED_CRASH();
	while (cute_tiled_isspace(c = *m->in++));
	return c;
}

static int cute_tiled_try(cute_tiled_map_internal_t* m, char expect)
{
	if (m->in == m->end) CUTE_TILED_CRASH();
	if (cute_tiled_peak(m) == expect)
	{
		m->in++;
		return 1;
	}
	return 0;
}

#define cute_tiled_expect(m, expect) \
	do { \
		static char error[128]; \
		CUTE_TILED_SNPRINTF(error, sizeof(error), "Found unexpected token '%c', expected '%c' (is this a valid JSON file?).", *m->in, expect); \
		CUTE_TILED_CHECK(cute_tiled_next(m) == (expect), error); \
	} while (0)

char cute_tiled_parse_char(char c)
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

static int cute_tiled_skip_object_internal(cute_tiled_map_internal_t* m)
{
	int depth = 1;
	cute_tiled_expect(m, '{');

	while (depth) {
		CUTE_TILED_CHECK(m->in <= m->end, "Attempted to read passed input buffer (is this a valid JSON file?).");

		char c = cute_tiled_next(m);

		switch(c)
		{
		case '{':
			depth += 1;
			break;

		case '}':
			depth -= 1;
			break;

		default:
			break;
		}
	}

	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_skip_object(m) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_skip_object_internal(m)); \
	} while (0)

static int cute_tiled_skip_array_internal(cute_tiled_map_internal_t* m)
{
	int depth = 1;
	cute_tiled_expect(m, '[');

	while (depth)
	{
		CUTE_TILED_CHECK(m->in <= m->end, "Attempted to read passed input buffer (is this a valid JSON file?).");

		char c = cute_tiled_next(m);

		switch(c)
		{
		case '[':
			depth += 1;
			break;

		case ']':
			depth -= 1;
			break;

		default:
			break;
		}
	}

	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_skip_array(m) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_skip_array_internal(m)); \
	} while (0)

static int cute_tiled_read_string_internal(cute_tiled_map_internal_t* m)
{
	int count = 0;
	int done = 0;
	cute_tiled_expect(m, '"');

	while (!done)
	{
		CUTE_TILED_CHECK(count < CUTE_TILED_INTERNAL_BUFFER_MAX, "String exceeded max length of CUTE_TILED_INTERNAL_BUFFER_MAX.");
		char c = cute_tiled_next(m);

		switch (c)
		{
		case '"':
			m->scratch[count] = 0;
			done = 1;
			break;

		case '\\':
		{
			char the_char = cute_tiled_parse_char(cute_tiled_next(m));
			m->scratch[count++] = the_char;
		}	break;

		default:
			m->scratch[count++] = c;
			break;
		}
	}

	m->scratch_len = count;
	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_string(m) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_string_internal(m)); \
	} while (0)

static int cute_tiled_read_int_internal(cute_tiled_map_internal_t* m, int* out)
{
	char* end;
	int val = (int)strtoll(m->in, &end, 10);
	if (*end == '.') strtod(m->in, &end); // If we're reading a float as an int, then just skip the decimal part.
	CUTE_TILED_CHECK(m->in != end, "Invalid integer found during parse.");
	m->in = end;
	*out = val;
	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_int(m, num) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_int_internal(m, num)); \
	} while (0)

static int cute_tiled_read_hex_int_internal(cute_tiled_map_internal_t* m, int* out)
{
	switch (cute_tiled_peak(m))
	{
	case '#':
		cute_tiled_next(m);
		break;

	case '0':
	{
		cute_tiled_next(m);
		char c = cute_tiled_next(m);
		CUTE_TILED_CHECK((c == 'x') | (c == 'X'), "Expected 'x' or 'X' while parsing a hex number.");
	}	break;
	}

	char* end;
	unsigned long long int val;
	val = strtoull(m->in, &end, 16);
	CUTE_TILED_CHECK(m->in != end, "Invalid integer found during parse.");
	m->in = end;
	*out = (int)val;
	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_hex_int(m, num) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_hex_int_internal(m, num)); \
	} while (0)

static int cute_tiled_read_float_internal(cute_tiled_map_internal_t* m, float* out)
{
	char* end;
	float val = (float)strtod(m->in, &end);
	CUTE_TILED_CHECK(m->in != end, "Invalid integer found during parse.");
	m->in = end;
	*out = val;
	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_float(m, num) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_float_internal(m, num)); \
	} while (0)

static int cute_tiled_read_bool_internal(cute_tiled_map_internal_t* m, int* out)
{
	if ((cute_tiled_peak(m) == 't') | (cute_tiled_peak(m) == 'T'))
	{
		m->in += 4;
		*out = 1;
	}

	else if ((cute_tiled_peak(m) == 'f') | (cute_tiled_peak(m) == 'F'))
	{
		m->in += 5;
		*out = 0;
	}

	else goto cute_tiled_err;

	CUTE_TILED_CHECK(m->in <= m->end, "Attempted to read passed input buffer (is this a valid JSON file?).");
	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_bool(m, b) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_bool_internal(m, b)); \
	} while (0)

int cute_tiled_read_csv_integers_internal(cute_tiled_map_internal_t* m, int* count_out, int** out)
{
	int count = 0;
	int capacity = 1024;
	int* integers = (int*)CUTE_TILED_ALLOC(capacity * sizeof(int), m->mem_ctx);

	char c;
	do
	{
		int val;
		cute_tiled_read_int(m, &val);
		if (count == capacity)
		{
			capacity *= 2;
			int* new_integers = (int*)CUTE_TILED_ALLOC(capacity * sizeof(int), m->mem_ctx);
			CUTE_TILED_MEMCPY(new_integers, integers, sizeof(int) * count);
			CUTE_TILED_FREE(integers, m->mem_ctx);
			integers = new_integers;
		}
		integers[count++] = val;
		c = cute_tiled_next(m);
	}
	while (c != ']');

	*count_out = count;
	*out = integers;
	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_csv_integers(m, count_out, out) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_csv_integers_internal(m, count_out, out)); \
	} while (0)

int cute_tiled_intern_string_internal(cute_tiled_map_internal_t* m, cute_tiled_string_t* out)
{
	STRPOOL_EMBEDDED_U64 id;
	cute_tiled_read_string(m);

	// Store string id inside the memory of the pointer. This is important since
	// the string pool can relocate strings while parsing the map file at any
	// time.

	// Later there will be a second pass to patch all these string
	// pointers by doing: *out = (const char*)strpool_embedded_cstr(&m->strpool, id);

	id = strpool_embedded_inject(&m->strpool, m->scratch, m->scratch_len);
	// if (sizeof(const char*) < sizeof(STRPOOL_EMBEDDED_U64)) *(int*)0 = 0; // sanity check
	out->hash_id = id;

	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_intern_string(m, out) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_intern_string_internal(m, out)); \
	} while (0)

int cute_tiled_read_vertex_array_internal(cute_tiled_map_internal_t* m, int* out_count, float** out_verts)
{
	int vert_count = 0;
	int capacity = 32;
	float *verts;
	cute_tiled_expect(m, '[');

	verts = (float*)CUTE_TILED_ALLOC(sizeof(float) * capacity * 2, m->mem_ctx);

	while (cute_tiled_peak(m) != ']')
	{
		cute_tiled_expect(m, '{');
		cute_tiled_expect(m, '"');

		int swap = cute_tiled_try(m, 'x') ? 0 : 1;
		float x = 0, y = 0;
		cute_tiled_expect(m, '"');
		cute_tiled_expect(m, ':');
		cute_tiled_read_float(m, swap ? &y : &x);
		cute_tiled_expect(m, ',');
		cute_tiled_expect(m, '"');
		cute_tiled_expect(m, swap ? 'x' : 'y');
		cute_tiled_expect(m, '"');
		cute_tiled_expect(m, ':');
		cute_tiled_read_float(m, swap ? &x : &y);
		cute_tiled_expect(m, '}');
		cute_tiled_try(m, ',');

		if (vert_count == capacity)
		{
			capacity *= 2;
			float* new_verts = (float*)CUTE_TILED_ALLOC(sizeof(float) * capacity * 2, m->mem_ctx);
			CUTE_TILED_MEMCPY(new_verts, verts, sizeof(float) * 2 * vert_count);
			CUTE_TILED_FREE(verts, m->mem_ctx);
			verts = new_verts;
		}

		verts[vert_count * 2] = x;
		verts[vert_count * 2 + 1] = y;
		++vert_count;
	}

	cute_tiled_expect(m, ']');
	*out_count = vert_count;
	*out_verts = verts;

	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_vertex_array(m, out_count, out_verts) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_vertex_array_internal(m, out_count, out_verts)); \
	} while (0)

int cute_tiled_skip_until_after_internal(cute_tiled_map_internal_t* m, char c)
{
	while (*m->in != c) {
		cute_tiled_error_line += *m->in == '\n';
		m->in++;
	}
	cute_tiled_expect(m, c);
	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_skip_until_after(m, c) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_skip_until_after_internal(m, c)); \
	} while (0)

int cute_tiled_read_properties_internal(cute_tiled_map_internal_t* m, cute_tiled_property_t** out_properties, int* out_count)
{
	int count = 0;
	int capacity = 32;
	cute_tiled_property_t* props = (cute_tiled_property_t*)CUTE_TILED_ALLOC(capacity * sizeof(cute_tiled_property_t), m->mem_ctx);

	cute_tiled_expect(m, '[');

	while (cute_tiled_peak(m) != ']')
	{
		cute_tiled_expect(m, '{');

		cute_tiled_property_t prop;
		prop.type = CUTE_TILED_PROPERTY_NONE;

		// Read in the property name.
		cute_tiled_skip_until_after(m, ':');
		cute_tiled_intern_string(m, &prop.name);

		// Read in the property type. The value type is deduced while parsing, this is only used for float because the JSON format omits decimals on round floats.
		cute_tiled_skip_until_after(m, ':');
		cute_tiled_expect(m, '"');
		char type_char = cute_tiled_next(m);

		// Skip extraneous JSON information and go find the actual value data.
		cute_tiled_skip_until_after(m, ':');

		char c = cute_tiled_peak(m);

		if (((c == 't') | (c == 'T')) | ((c == 'f') | (c == 'F')))
		{
			cute_tiled_read_bool(m, &prop.data.boolean);
			prop.type = CUTE_TILED_PROPERTY_BOOL;
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
				cute_tiled_expect(m, '"');
				cute_tiled_read_hex_int(m, &prop.data.integer);
				cute_tiled_expect(m, '"');
				prop.type = CUTE_TILED_PROPERTY_COLOR;
			}

			else
			{
				cute_tiled_intern_string(m, &prop.data.string);
				prop.type = CUTE_TILED_PROPERTY_STRING;
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

			if (is_float || type_char == 'f')
			{
				cute_tiled_read_float(m, &prop.data.floating);
				prop.type = CUTE_TILED_PROPERTY_FLOAT;
			}

			else
			{
				cute_tiled_read_int(m, &prop.data.integer);
				prop.type = CUTE_TILED_PROPERTY_INT;
			}
		}

		if (count == capacity)
		{
			capacity *= 2;
			cute_tiled_property_t* new_props = (cute_tiled_property_t*)CUTE_TILED_ALLOC(capacity * sizeof(cute_tiled_property_t), m->mem_ctx);
			CUTE_TILED_MEMCPY(new_props, props, sizeof(cute_tiled_property_t) * count);
			CUTE_TILED_FREE(props, m->mem_ctx);
			props = new_props;
		}
		props[count++] = prop;

		cute_tiled_expect(m, '}');
		cute_tiled_try(m, ',');
	}

	cute_tiled_expect(m, ']');
	cute_tiled_try(m, ',');

	*out_properties = count ? props : NULL;
        if (!count) CUTE_TILED_FREE(props, m->mem_ctx);
	*out_count = count;

	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_properties(m, out_properties, out_count) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_properties_internal(m, out_properties, out_count)); \
	} while (0)

cute_tiled_object_t* cute_tiled_read_object(cute_tiled_map_internal_t* m)
{
	cute_tiled_object_t* object = (cute_tiled_object_t*)cute_tiled_alloc(m, sizeof(cute_tiled_object_t));
	CUTE_TILED_MEMSET(object, 0, sizeof(cute_tiled_object_t));
	cute_tiled_expect(m, '{');

	while (cute_tiled_peak(m) != '}')
	{
		cute_tiled_read_string(m);
		cute_tiled_expect(m, ':');
		CUTE_TILED_U64 h = cute_tiled_FNV1a(m->scratch, m->scratch_len + 1);

		switch (h)
		{
		case 14479365350473253539U: // ellipse
			cute_tiled_read_bool(m, &object->ellipse);
			break;

		case 14992147199312073281U: // gid
			cute_tiled_read_int(m, &object->gid);
			break;

		case 809651598226485190U: // height
			cute_tiled_read_float(m, &object->height);
			break;

		case 3133932603199444032U: // id
			cute_tiled_read_int(m, &object->id);
			break;

		case 12661511911333414066U: // name
			cute_tiled_intern_string(m, &object->name);
			break;

		case 15925463322410838979U: // point
			cute_tiled_read_bool(m, &object->point);
			break;

		case 11191351929714760271U: // polyline
			cute_tiled_read_vertex_array(m, &object->vert_count, &object->vertices);
			object->vert_type = 0;
			break;

		case 6623316362411997547U: // polygon
			cute_tiled_read_vertex_array(m, &object->vert_count, &object->vertices);
			object->vert_type = 1;
			break;

		case 8368542207491637236U: // properties
			cute_tiled_read_properties(m, &object->properties, &object->property_count);
			break;

		case 17386473859969670701U: // rotation
			cute_tiled_read_float(m, &object->rotation);
			break;

		case 7758770083360183834U: // text
			CUTE_TILED_WARNING("Text field of Tiled objects is not yet supported.");
			while (cute_tiled_peak(m) != '}') cute_tiled_next(m);
			cute_tiled_expect(m, '}');
			break;

		case 13509284784451838071U: // type
			cute_tiled_intern_string(m, &object->type);
			break;

		case 128234417907068947U: // visible
			cute_tiled_read_bool(m, &object->visible);
			break;

		case 7400839267610537869U: // width
			cute_tiled_read_float(m, &object->width);
			break;

		case 644252274336276709U: // x
			cute_tiled_read_float(m, &object->x);
			break;

		case 643295699219922364U: // y
			cute_tiled_read_float(m, &object->y);
			break;

		default:
			CUTE_TILED_CHECK(0, "Unknown identifier found.");
		}

		cute_tiled_try(m, ',');
	}

	cute_tiled_expect(m, '}');
	return object;

cute_tiled_err:
	return 0;
}

cute_tiled_layer_t* cute_tiled_layers(cute_tiled_map_internal_t* m)
{
	cute_tiled_layer_t* layer = (cute_tiled_layer_t*)cute_tiled_alloc(m, sizeof(cute_tiled_layer_t));
	CUTE_TILED_MEMSET(layer, 0, sizeof(cute_tiled_layer_t));
	layer->parallaxx = 1.0f;
	layer->parallaxy = 1.0f;

	cute_tiled_expect(m, '{');

	while (cute_tiled_peak(m) != '}')
	{
		cute_tiled_read_string(m);
		cute_tiled_expect(m, ':');
		CUTE_TILED_U64 h = cute_tiled_FNV1a(m->scratch, m->scratch_len + 1);

		switch (h)
		{
		case 14868627273436340303U: // compression
			CUTE_TILED_CHECK(0, "Compression is not yet supported. The expected tile format is CSV (uncompressed). Please see the docs if you are interested in compression.");
			break;

		case 4430454992770877055U: // data
			CUTE_TILED_CHECK(cute_tiled_peak(m) == '[', "The expected tile format is CSV (uncompressed). It looks like Base64 (uncompressed) was selected. Please see the docs if you are interested in compression.");
			cute_tiled_expect(m, '[');
			cute_tiled_read_csv_integers(m, &layer->data_count, &layer->data);
			break;

		case 1888774307506158416U: // encoding
			CUTE_TILED_CHECK(0, "Encoding is not yet supported. The expected tile format is CSV (uncompressed). Please see the docs if you are interested in compression.");
			break;

		case 2841939415665718447U: // draworder
			cute_tiled_intern_string(m, &layer->draworder);
			break;

		case 809651598226485190U: // height
			cute_tiled_read_int(m, &layer->height);
			break;

		case 13522647194774232494U: // image
			cute_tiled_intern_string(m, &layer->image);
			break;

		case 4566956252693479661U: // layers
		cute_tiled_expect(m, '[');

		while (cute_tiled_peak(m) != ']')
		{
			cute_tiled_layer_t* child_layer = cute_tiled_layers(m);
			CUTE_TILED_FAIL_IF(!child_layer);
			child_layer->next = layer->layers;
			layer->layers = child_layer;
			cute_tiled_try(m, ',');
		}

		cute_tiled_expect(m, ']');
		break;

		case 12661511911333414066U: // name
			cute_tiled_intern_string(m, &layer->name);
			break;

		case 107323337999513585U: // objects
			cute_tiled_expect(m, '[');

			while (cute_tiled_peak(m) != ']')
			{
				cute_tiled_object_t* object = cute_tiled_read_object(m);
				CUTE_TILED_FAIL_IF(!object);
				object->next = layer->objects;
				layer->objects = object;
				cute_tiled_try(m, ',');
			}

			cute_tiled_expect(m, ']');
			break;

		case 5195853646368960386U: // offsetx
			cute_tiled_read_float(m, &layer->offsetx);
			break;

		case 5196810221485314731U: // offsety
			cute_tiled_read_float(m, &layer->offsety);
			break;

		case 11746902372727406098U: // opacity
			cute_tiled_read_float(m, &layer->opacity);
			break;

		case 8368542207491637236U: // properties
			cute_tiled_read_properties(m, &layer->properties, &layer->property_count);
			break;

		case 8489814081865549564U: // transparentcolor
			cute_tiled_expect(m, '"');
			cute_tiled_read_hex_int(m, &layer->transparentcolor);
			cute_tiled_expect(m, '"');
			break;

		case 13509284784451838071U: // type
			cute_tiled_intern_string(m, &layer->type);
			break;

		case 128234417907068947U: // visible
			cute_tiled_read_bool(m, &layer->visible);
			break;

		case 7400839267610537869U: // width
			cute_tiled_read_int(m, &layer->width);
			break;

		case 644252274336276709U: // x
			cute_tiled_read_int(m, &layer->x);
			break;

		case 643295699219922364U: // y
			cute_tiled_read_int(m, &layer->y);
			break;

		case 18212633776084966362U: // parallaxx
			cute_tiled_read_float(m, &layer->parallaxx);
			break;

		case 18213590351201320707U: // parallaxy
			cute_tiled_read_float(m, &layer->parallaxy);
			break;

		case 3133932603199444032U: // id
			cute_tiled_read_int(m, &layer->id);
		break;

		default:
			CUTE_TILED_CHECK(0, "Unknown identifier found.");
		}

		cute_tiled_try(m, ',');
	}

	cute_tiled_expect(m, '}');
	return layer;

cute_tiled_err:
	return 0;
}

int cute_tiled_read_animation_frames_internal(cute_tiled_map_internal_t* m, cute_tiled_frame_t** out_frames, int* out_count)
{
	int count = 0;
	int capacity = 32;
	cute_tiled_frame_t* frames = (cute_tiled_frame_t*)CUTE_TILED_ALLOC(capacity * sizeof(cute_tiled_frame_t), m->mem_ctx);

	cute_tiled_expect(m, '[');

	while (cute_tiled_peak(m) != ']')
	{
		cute_tiled_expect(m, '{');

		cute_tiled_frame_t frame;

		// Read in the duration and tileid.
		cute_tiled_skip_until_after(m, ':');
		cute_tiled_read_int(m, &frame.duration);
		cute_tiled_expect(m, ',');
		cute_tiled_skip_until_after(m, ':');
		cute_tiled_read_int(m, &frame.tileid);

		if (count == capacity)
		{
			capacity *= 2;
			cute_tiled_frame_t* new_frames = (cute_tiled_frame_t*)CUTE_TILED_ALLOC(capacity * sizeof(cute_tiled_frame_t), m->mem_ctx);
			CUTE_TILED_MEMCPY(new_frames, frames, sizeof(cute_tiled_frame_t) * count);
			CUTE_TILED_FREE(frames, m->mem_ctx);
			frames = new_frames;
		}
		frames[count++] = frame;

		cute_tiled_expect(m, '}');
		cute_tiled_try(m, ',');
	}

	cute_tiled_expect(m, ']');
	cute_tiled_try(m, ',');

	*out_frames = frames;
	*out_count = count;

	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_animation_frames(m, out_frames, out_count) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_animation_frames_internal(m, out_frames, out_count)); \
	} while (0)

cute_tiled_tile_descriptor_t* cute_tiled_read_tile_descriptor(cute_tiled_map_internal_t* m)
{
	cute_tiled_tile_descriptor_t* tile_descriptor = (cute_tiled_tile_descriptor_t*)cute_tiled_alloc(m, sizeof(cute_tiled_tile_descriptor_t));
	CUTE_TILED_MEMSET(tile_descriptor, 0, sizeof(cute_tiled_tile_descriptor_t));

	cute_tiled_expect(m, '{');
	while (cute_tiled_peak(m) != '}')
	{
		cute_tiled_read_string(m);
		cute_tiled_expect(m, ':');
		CUTE_TILED_U64 h = cute_tiled_FNV1a(m->scratch, m->scratch_len + 1);

		switch (h)
		{
		case 3133932603199444032U: // id
			cute_tiled_read_int(m, &tile_descriptor->tile_index);
			break;

		case 13509284784451838071U: // type
			cute_tiled_intern_string(m, &tile_descriptor->type);
			break;

		case 13522647194774232494U: // image
			cute_tiled_intern_string(m, &tile_descriptor->image);
			break;

		case 7796197983149768626U: // imagewidth
			cute_tiled_read_int(m, &tile_descriptor->imagewidth);
			break;

		case 2114495263010514843U: // imageheight
			cute_tiled_read_int(m, &tile_descriptor->imageheight);
			break;

		case 8368542207491637236U: // properties
			cute_tiled_read_properties(m, &tile_descriptor->properties, &tile_descriptor->property_count);
			break;

		case 6659907350341014391U: // objectgroup
		{
			cute_tiled_layer_t* layer = cute_tiled_layers(m);
			CUTE_TILED_FAIL_IF(!layer);
			layer->next = tile_descriptor->objectgroup;
			tile_descriptor->objectgroup = layer;
		}	break;

		case 6875414612738028948: // probability
			cute_tiled_read_float(m, &tile_descriptor->probability);
			break;

		case 2784044778313316778U: // terrain: used by tiled editor only
			cute_tiled_skip_array(m);
			break;

		case 3115399308714904519U: // animation
			cute_tiled_read_animation_frames(m, &tile_descriptor->animation, &tile_descriptor->frame_count);
			break;

		default:
			CUTE_TILED_CHECK(0, "Unknown identifier found.");
		}

		cute_tiled_try(m, ',');
	}

	cute_tiled_expect(m, '}');
	return tile_descriptor;

cute_tiled_err:
	return 0;
}

int cute_tiled_read_point_internal(cute_tiled_map_internal_t* m, int* point_x, int* point_y)
{
	*point_x = 0;
	*point_y = 0;

	cute_tiled_expect(m, '{');
	while (cute_tiled_peak(m) != '}')
	{
		cute_tiled_read_string(m);
		cute_tiled_expect(m, ':');
		CUTE_TILED_U64 h = cute_tiled_FNV1a(m->scratch, m->scratch_len + 1);

		switch (h)
		{
		case 644252274336276709U: // x
			cute_tiled_read_int(m, point_x);
			break;

		case 643295699219922364U: // y
			cute_tiled_read_int(m, point_y);
			break;

		default:
			CUTE_TILED_CHECK(0, "Unknown identifier found.");
		}

		cute_tiled_try(m, ',');
	}

	cute_tiled_expect(m, '}');

	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_read_point(m, x, y) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_read_point_internal(m, x, y)); \
	} while (0)

static CUTE_TILED_INLINE int cute_tiled_skip_curly_braces_internal(cute_tiled_map_internal_t* m)
{
	int count = 1;
	cute_tiled_expect(m, '{');
	while (count)
	{
		char c = cute_tiled_next(m);
		if (c == '}') --count;
		else if (c == '{') ++count;
	}

	return 0;

cute_tiled_err:
	return 1;
}

cute_tiled_tileset_t* cute_tiled_tileset(cute_tiled_map_internal_t* m)
{
	cute_tiled_tileset_t* tileset = (cute_tiled_tileset_t*)cute_tiled_alloc(m, sizeof(cute_tiled_tileset_t));
	CUTE_TILED_MEMSET(tileset, 0, sizeof(cute_tiled_tileset_t));
	cute_tiled_expect(m, '{');

	while (cute_tiled_peak(m) != '}')
	{
		cute_tiled_read_string(m);
		cute_tiled_expect(m, ':');
		CUTE_TILED_U64 h = cute_tiled_FNV1a(m->scratch, m->scratch_len + 1);

		switch (h)
		{
		case 17465100621023921744U: // backgroundcolor
			cute_tiled_expect(m, '"');
			cute_tiled_read_hex_int(m, &tileset->backgroundcolor);
			cute_tiled_expect(m, '"');
			break;

		case 12570673734542705940U: // columns
			cute_tiled_read_int(m, &tileset->columns);
			break;

		case 13648382824248632287U: // editorsettings
			cute_tiled_skip_object(m);
			break;

		case 13956389100366699181U: // firstgid
			cute_tiled_read_int(m, &tileset->firstgid);
			break;

		case 16920059161811221315U: // grid: unsupported
			cute_tiled_skip_object(m);
			break;

		case 13522647194774232494U: // image
			cute_tiled_intern_string(m, &tileset->image);
			break;

		case 7796197983149768626U: // imagewidth
			cute_tiled_read_int(m, &tileset->imagewidth);
			break;

		case 2114495263010514843U: // imageheight
			cute_tiled_read_int(m, &tileset->imageheight);
			break;

		case 4864566659847942049U: // margin
			cute_tiled_read_int(m, &tileset->margin);
			break;

		case 12661511911333414066U: // name
			cute_tiled_intern_string(m, &tileset->name);
			break;

		case 1007832939408977147U: // tiledversion
			cute_tiled_intern_string(m, &tileset->tiledversion);
			break;

		case 8196820454517111669U: // version
			cute_tiled_read_float(m, &tileset->version);
			break;

		case 8368542207491637236U: // properties
			cute_tiled_read_properties(m, &tileset->properties, &tileset->property_count);
			break;

		case 6491372721122724890U: // spacing
			cute_tiled_read_int(m, &tileset->spacing);
			break;

		case 4065097716972592720U: // tilecount
			cute_tiled_read_int(m, &tileset->tilecount);
			break;

		case 13337683360624280154U: // tileheight
			cute_tiled_read_int(m, &tileset->tileheight);
			break;

		case 2769630600247906626U: // tileoffset
			cute_tiled_read_point(m, &tileset->tileoffset_x, &tileset->tileoffset_y);
			break;

		case 7277156227374254384U: // tileproperties
			CUTE_TILED_WARNING("`tileproperties` is deprecated. Attempting to skip.");
			CUTE_TILED_FAIL_IF(cute_tiled_skip_curly_braces_internal(m));
			break;

		case 15569462518706435895U: // tilepropertytypes
			CUTE_TILED_WARNING("`tilepropertytypes` is deprecated. Attempting to skip.");
			CUTE_TILED_FAIL_IF(cute_tiled_skip_curly_braces_internal(m));
			break;

		case 6504415465426505561U: // tilewidth
			cute_tiled_read_int(m, &tileset->tilewidth);
			break;

		case 8489814081865549564U: // transparentcolor
			cute_tiled_expect(m, '"');
			cute_tiled_read_hex_int(m, &tileset->transparentcolor);
			cute_tiled_expect(m, '"');
			break;

		case 13509284784451838071U: // type
			cute_tiled_intern_string(m, &tileset->type);
			break;

		case 8053780534892277672U: // source
			cute_tiled_intern_string(m, &tileset->source);
#ifndef CUTE_TILED_NO_EXTERNAL_TILESET_WARNING
			CUTE_TILED_WARNING("You might have forgotten to embed your tileset -- Most fields of `cute_tiled_tileset_t` will be zero'd out (unset).");
#endif /* CUTE_TILED_NO_EXTERNAL_TILESET_WARNING */
			break;

		case 1819203229U: // objectalignment
			cute_tiled_intern_string(m, &tileset->objectalignment);
			break;

		case 104417158474046698U: // tiles
		{
			cute_tiled_expect(m, '[');
			while (cute_tiled_peak(m) != ']')
			{
				cute_tiled_tile_descriptor_t* tile_descriptor = cute_tiled_read_tile_descriptor(m);
				CUTE_TILED_FAIL_IF(!tile_descriptor);
				tile_descriptor->next = tileset->tiles;
				tileset->tiles = tile_descriptor;
				cute_tiled_try(m, ',');
			}
			cute_tiled_expect(m, ']');
		}	break;

		case 14766449174202642533U: // terrains: used by tiled editor only
			cute_tiled_skip_array(m);
			break;

		case 6029584663444593209U: // wangsets: used by tiled editor only
			cute_tiled_skip_array(m);
			break;

		default:
			CUTE_TILED_CHECK(0, "Unknown identifier found.");
		}

		cute_tiled_try(m, ',');
	}

	cute_tiled_expect(m, '}');
	return tileset;

cute_tiled_err:
	return 0;
}

static int cute_tiled_dispatch_map_internal(cute_tiled_map_internal_t* m)
{
	CUTE_TILED_U64 h;
	cute_tiled_read_string(m);
	cute_tiled_expect(m, ':');
	h = cute_tiled_FNV1a(m->scratch, m->scratch_len + 1);

 	switch (h)
	{
	case 17465100621023921744U: // backgroundcolor
		cute_tiled_expect(m, '"');
		cute_tiled_read_hex_int(m, &m->map.backgroundcolor);
		cute_tiled_expect(m, '"');
		break;

	case 5549108793316760247U: // compressionlevel
	{
		int compressionlevel;
		cute_tiled_read_int(m, &compressionlevel);
		CUTE_TILED_CHECK(compressionlevel == -1 || compressionlevel == 0, "Compression is not yet supported.");
	}	break;

	case 13648382824248632287U: // editorsettings
		cute_tiled_skip_object(m);
		break;

	case 809651598226485190U: // height
		cute_tiled_read_int(m, &m->map.height);
		break;

	case 16529928297377797591U: // infinite
		cute_tiled_read_bool(m, &m->map.infinite);
		break;

	case 4566956252693479661U: // layers
		cute_tiled_expect(m, '[');

		while (cute_tiled_peak(m) != ']')
		{
			cute_tiled_layer_t* layer = cute_tiled_layers(m);
			CUTE_TILED_FAIL_IF(!layer);
			layer->next = m->map.layers;
			m->map.layers = layer;
			cute_tiled_try(m, ',');
		}

		cute_tiled_expect(m, ']');
		break;

	case 11291153769551921430U: // nextobjectid
		cute_tiled_read_int(m, &m->map.nextobjectid);
		break;

	case 563935667693078739U: // orientation
		cute_tiled_intern_string(m, &m->map.orientation);
		break;

	case 8368542207491637236U: // properties
		cute_tiled_read_properties(m, &m->map.properties, &m->map.property_count);
		break;

	case 16693886730115578029U: // renderorder
		cute_tiled_intern_string(m, &m->map.renderorder);
		break;

	case 1007832939408977147U: // tiledversion
		cute_tiled_intern_string(m, &m->map.tiledversion);
		break;

	case 13337683360624280154U: // tileheight
		cute_tiled_read_int(m, &m->map.tileheight);
		break;

	case 8310322674355535532U: // tilesets
	{
		cute_tiled_expect(m, '[');

		while (cute_tiled_peak(m) != ']')
		{
			cute_tiled_tileset_t* tileset = cute_tiled_tileset(m);
			CUTE_TILED_FAIL_IF(!tileset);
			tileset->next = m->map.tilesets;
			m->map.tilesets = tileset;
			cute_tiled_try(m, ',');
		}

		cute_tiled_expect(m, ']');
	}	break;

	case 6504415465426505561U: // tilewidth
		cute_tiled_read_int(m, &m->map.tilewidth);
		break;

	case 13509284784451838071U: // type
		cute_tiled_intern_string(m, &m->map.type);
		break;

	case 8196820454517111669U: // version
		cute_tiled_read_float(m, &m->map.version);
		break;

	case 7400839267610537869U: // width
		cute_tiled_read_int(m, &m->map.width);
		break;

	case 2498445529143042872U: // nextlayerid
		cute_tiled_read_int(m, &m->map.nextlayerid);
	break;

	default:
		CUTE_TILED_CHECK(0, "Unknown identifier found.");
	}

	return 1;

cute_tiled_err:
	return 0;
}

#define cute_tiled_dispatch_map(m) \
	do { \
		CUTE_TILED_FAIL_IF(!cute_tiled_dispatch_map_internal(m)); \
	} while (0)

static CUTE_TILED_INLINE void cute_tiled_deintern_string(cute_tiled_map_internal_t* m, cute_tiled_string_t* s)
{
	s->ptr = strpool_embedded_cstr(&m->strpool, s->hash_id);
}

static void cute_tiled_deintern_properties(cute_tiled_map_internal_t* m, cute_tiled_property_t* properties, int property_count)
{
	for (int i = 0; i < property_count; ++i)
	{
		cute_tiled_property_t* p = properties + i;
		cute_tiled_deintern_string(m, &p->name);
		if (p->type == CUTE_TILED_PROPERTY_STRING) cute_tiled_deintern_string(m, &p->data.string);
	}
}

static void cute_tiled_deintern_layer(cute_tiled_map_internal_t* m, cute_tiled_layer_t* layer)
{
	while (layer)
	{
		cute_tiled_deintern_string(m, &layer->draworder);
		cute_tiled_deintern_string(m, &layer->name);
		cute_tiled_deintern_string(m, &layer->type);
		cute_tiled_deintern_string(m, &layer->image);
		cute_tiled_deintern_properties(m, layer->properties, layer->property_count);

		cute_tiled_object_t* object = layer->objects;
		while (object)
		{
			cute_tiled_deintern_string(m, &object->name);
			cute_tiled_deintern_string(m, &object->type);
			cute_tiled_deintern_properties(m, object->properties, object->property_count);
			object = object->next;
		}

		cute_tiled_deintern_layer(m, layer->layers);

		layer = layer->next;
	}
}

static void cute_tiled_patch_tileset_strings(cute_tiled_map_internal_t* m, cute_tiled_tileset_t* tileset)
{
	cute_tiled_deintern_string(m, &tileset->image);
	cute_tiled_deintern_string(m, &tileset->name);
	cute_tiled_deintern_string(m, &tileset->type);
	cute_tiled_deintern_string(m, &tileset->source);
	cute_tiled_deintern_string(m, &tileset->tiledversion);
	cute_tiled_deintern_string(m, &tileset->objectalignment);
	cute_tiled_deintern_properties(m, tileset->properties, tileset->property_count);
	cute_tiled_tile_descriptor_t* tile_descriptor = tileset->tiles;
	while (tile_descriptor)
	{
		cute_tiled_deintern_string(m, &tile_descriptor->image);
		cute_tiled_deintern_string(m, &tile_descriptor->type);
		cute_tiled_deintern_properties(m, tile_descriptor->properties, tile_descriptor->property_count);
		tile_descriptor = tile_descriptor->next;
	}
}

static void cute_tiled_patch_interned_strings(cute_tiled_map_internal_t* m)
{
	cute_tiled_deintern_string(m, &m->map.orientation);
	cute_tiled_deintern_string(m, &m->map.renderorder);
	cute_tiled_deintern_string(m, &m->map.tiledversion);
	cute_tiled_deintern_string(m, &m->map.type);
	cute_tiled_deintern_properties(m, m->map.properties, m->map.property_count);

	cute_tiled_tileset_t* tileset = m->map.tilesets;
	while (tileset)
	{
		cute_tiled_patch_tileset_strings(m, tileset);
		tileset = tileset->next;
	}

	cute_tiled_layer_t* layer = m->map.layers;
	cute_tiled_deintern_layer(m, layer);
}

static void cute_tiled_free_objects(cute_tiled_object_t* objects, void* mem_ctx)
{
	CUTE_TILED_UNUSED(mem_ctx);
	cute_tiled_object_t* object = objects;
	while (object)
	{
		if (object->properties) CUTE_TILED_FREE(object->properties, mem_ctx);
		if (object->vertices) CUTE_TILED_FREE(object->vertices, mem_ctx);
		object = object->next;
	}
}

static void cute_tiled_free_layers(cute_tiled_layer_t* layers, void* mem_ctx)
{
	CUTE_TILED_UNUSED(mem_ctx);
	cute_tiled_layer_t* layer = layers;
	while (layer)
	{
		if (layer->data) CUTE_TILED_FREE(layer->data, mem_ctx);
		if (layer->properties) CUTE_TILED_FREE(layer->properties, mem_ctx);
		cute_tiled_free_layers(layer->layers, mem_ctx);
		cute_tiled_free_objects(layer->objects, mem_ctx);
		layer = layer->next;
	}
}

static void cute_tiled_free_map_internal(cute_tiled_map_internal_t* m)
{
	strpool_embedded_term(&m->strpool);

	cute_tiled_free_layers(m->map.layers, m->mem_ctx);
	if (m->map.properties) CUTE_TILED_FREE(m->map.properties, m->mem_ctx);

	cute_tiled_tileset_t* tileset = m->map.tilesets;
	while (tileset)
	{
		if (tileset->properties) CUTE_TILED_FREE(tileset->properties, m->mem_ctx);
		cute_tiled_tile_descriptor_t* desc = tileset->tiles;
		while (desc)
		{
			if (desc->properties) CUTE_TILED_FREE(desc->properties, m->mem_ctx);
			if (desc->animation) CUTE_TILED_FREE(desc->animation, mem_ctx);
			cute_tiled_free_layers(desc->objectgroup, m->mem_ctx);
			desc = desc->next;
		}
		tileset = tileset->next;
	}

	// cute_tiled_read_csv_integers
	// cute_tiled_read_vertex_array
	// cute_tiled_read_properties

	cute_tiled_page_t* page = m->pages;
	while (page)
	{
		cute_tiled_page_t* next = page->next;
		CUTE_TILED_FREE(page, m->mem_ctx);
		page = next;
	}

	CUTE_TILED_FREE(m, m->mem_ctx);
}

#define CUTE_TILED_REVERSE_LIST(T, root) \
	do { \
		T* n = 0; \
		while (root) { \
			T* next = root->next; \
			root->next = n; \
			n = root; \
			root = next; \
		} \
		root = n; \
	} while (0)

void cute_tiled_reverse_layers(cute_tiled_map_t* map)
{
	CUTE_TILED_REVERSE_LIST(cute_tiled_layer_t, map->layers);
}

static cute_tiled_map_internal_t* cute_tiled_map_internal_alloc_internal(void* memory, int size_in_bytes, void* mem_ctx)
{
	cute_tiled_map_internal_t* m = (cute_tiled_map_internal_t*)CUTE_TILED_ALLOC(sizeof(cute_tiled_map_internal_t), mem_ctx);
	CUTE_TILED_MEMSET(m, 0, sizeof(cute_tiled_map_internal_t));
	m->in = (char*)memory;
	m->end = m->in + size_in_bytes;
	m->mem_ctx = mem_ctx;
	m->page_size = 1024 * 10;
	m->bytes_left_on_page = m->page_size;
	m->pages = (cute_tiled_page_t*)CUTE_TILED_ALLOC(sizeof(cute_tiled_page_t) + m->page_size, mem_ctx);
	m->pages->next = 0;
	m->pages->data = m->pages + 1;
	strpool_embedded_config_t config = strpool_embedded_default_config;
	config.memctx = mem_ctx;
	strpool_embedded_init(&m->strpool, &config);
	return m;
}

cute_tiled_map_t* cute_tiled_load_map_from_memory(const void* memory, int size_in_bytes, void* mem_ctx)
{
	cute_tiled_error_line = 1;

	cute_tiled_map_internal_t* m = cute_tiled_map_internal_alloc_internal((void*)memory, size_in_bytes, mem_ctx);
	cute_tiled_layer_t* layer = m->map.layers;
	cute_tiled_tileset_t* tileset = m->map.tilesets;
	cute_tiled_expect(m, '{');
	while (cute_tiled_peak(m) != '}')
	{
		cute_tiled_dispatch_map(m);
		cute_tiled_try(m, ',');
	}
	cute_tiled_expect(m, '}');

	// finalize output by patching strings and reversing singly linked lists
	cute_tiled_patch_interned_strings(m);
	CUTE_TILED_REVERSE_LIST(cute_tiled_layer_t, m->map.layers);
	CUTE_TILED_REVERSE_LIST(cute_tiled_tileset_t, m->map.tilesets);
	while (layer)
	{
		CUTE_TILED_REVERSE_LIST(cute_tiled_object_t, layer->objects);
		layer = layer->next;
	}
	while (tileset)
	{
		CUTE_TILED_REVERSE_LIST(cute_tiled_tile_descriptor_t, tileset->tiles);
		tileset = tileset->next;
	}

	return &m->map;

cute_tiled_err:
	cute_tiled_free_map_internal(m);
	CUTE_TILED_WARNING(cute_tiled_error_reason);
	return 0;
}

void cute_tiled_free_map(cute_tiled_map_t* map)
{
	cute_tiled_map_internal_t* m = (cute_tiled_map_internal_t*)(((char*)map) - (size_t)(&((cute_tiled_map_internal_t*)0)->map));
	cute_tiled_free_map_internal(m);
}

cute_tiled_tileset_t* cute_tiled_load_external_tileset(const char* path, void* mem_ctx)
{
	cute_tiled_error_file = path;

	int size;
	void* file = cute_tiled_read_file_to_memory_and_null_terminate(path, &size, mem_ctx);
	if (!file) CUTE_TILED_WARNING("Unable to find external tileset file.");
	cute_tiled_tileset_t* tileset = cute_tiled_load_external_tileset_from_memory(file, size, mem_ctx);
	CUTE_TILED_FREE(file, mem_ctx);

	cute_tiled_error_file = NULL;

	return tileset;
}

cute_tiled_tileset_t* cute_tiled_load_external_tileset_from_memory(const void* memory, int size_in_bytes, void* mem_ctx)
{
	cute_tiled_map_internal_t* m = cute_tiled_map_internal_alloc_internal((void*)memory, size_in_bytes, mem_ctx);
	cute_tiled_tileset_t* tileset = cute_tiled_tileset(m);
	cute_tiled_patch_tileset_strings(m, tileset);
	CUTE_TILED_REVERSE_LIST(cute_tiled_tile_descriptor_t, tileset->tiles);
	tileset->_internal = m;
	return tileset;
}

void cute_tiled_free_external_tileset(cute_tiled_tileset_t* tileset)
{
	cute_tiled_map_internal_t* m = (cute_tiled_map_internal_t*)tileset->_internal;
	cute_tiled_free_map_internal(m);
}

#endif // CUTE_TILED_IMPLEMENTATION_ONCE
#endif // CUTE_TILED_IMPLEMENTATION

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
