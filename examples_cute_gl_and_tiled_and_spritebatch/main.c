#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_DEPRECATE
#include <glad/glad.h>
#include <SDL2/SDL.h>

#define CUTE_ALLOC_IMPLEMENTATION
#include <cute_alloc.h>

#define CUTE_TILED_IMPLEMENTATION
#include <cute_tiled.h>

#define CUTE_GL_IMPLEMENTATION
#include "cute_gl.h"

#define CUTE_TIME_IMPLEMENTATION
#include <cute_time.h>

#define CUTE_PNG_IMPLEMENTATION
#include <cute_png.h>

#define SPRITEBATCH_MALLOC(size, ctx) CUTE_ALLOC_ALLOC(size, ctx)
#define SPRITEBATCH_FREE(mem, ctx) CUTE_ALLOC_FREE(mem, ctx)
#define SPRITEBATCH_IMPLEMENTATION
#include <cute_spritebatch.h>

spritebatch_t sb;
gl_context_t* ctx_tg;
SDL_Window* window;
SDL_GLContext ctx_gl;
gl_shader_t sprite_shader;
gl_renderable_t sprite_renderable;
float projection[16];

typedef struct
{
	SPRITEBATCH_U64 image_id;
	int depth;
	float x, y;
	float sx, sy;
	float c, s;
} sprite_t;

void* images[16 * 171];
sprite_t* tiles0;
sprite_t* tiles1;
int tile_count;

typedef struct
{
	float x, y;
	float u, v;
} vertex_t;

#define SPRITE_VERTS_MAX (1024 * 10)
int sprite_verts_count;
vertex_t sprite_verts[SPRITE_VERTS_MAX];

// callbacks for cute_spritebatch.h
void batch_report(spritebatch_sprite_t* sprites, int count, int texture_w, int texture_h, void* udata)
{
	(void)texture_w;
	(void)texture_h;
	(void)udata;
	// build the draw call
	gl_draw_call_t call;
	call.r = &sprite_renderable;
	call.textures[0] = (uint32_t)sprites[0].texture_id;
	call.texture_count = 1;

	// set texture uniform in shader
	gl_send_texture(call.r->program, "u_sprite_texture", 0);

	// NOTE:
	// perform any additional sorting here

	// build vertex buffer of quads from all sprite transforms
	call.verts = sprite_verts + sprite_verts_count;
	call.vert_count = count * 6;
	sprite_verts_count += call.vert_count;
	assert(sprite_verts_count < SPRITE_VERTS_MAX);

	vertex_t* verts = call.verts;
	for (int i = 0; i < count; ++i)
	{
		spritebatch_sprite_t* s = sprites + i;

		typedef struct v2
		{
			float x;
			float y;
		} v2;

		v2 quad[] = {
			{ -0.5f,  0.5f },
			{  0.5f,  0.5f },
			{  0.5f, -0.5f },
			{ -0.5f, -0.5f },
		};

		for (int j = 0; j < 4; ++j)
		{
			float x = quad[j].x;
			float y = quad[j].y;

			// rotate sprite about origin
			float x0 = s->c * x - s->s * y;
			float y0 = s->s * x + s->c * y;
			x = x0;
			y = y0;

			// scale sprite about origin
			x *= s->sx;
			y *= s->sy;

			// translate sprite into the world
			x += s->x;
			y += s->y;

			quad[j].x = x;
			quad[j].y = y;
		}

		// output transformed quad into CPU buffer
		vertex_t* out_verts = verts + i * 6;

		out_verts[0].x = quad[0].x;
		out_verts[0].y = quad[0].y;
		out_verts[0].u = s->minx;
		out_verts[0].v = s->maxy;

		out_verts[1].x = quad[3].x;
		out_verts[1].y = quad[3].y;
		out_verts[1].u = s->minx;
		out_verts[1].v = s->miny;

		out_verts[2].x = quad[1].x;
		out_verts[2].y = quad[1].y;
		out_verts[2].u = s->maxx;
		out_verts[2].v = s->maxy;

		out_verts[3].x = quad[1].x;
		out_verts[3].y = quad[1].y;
		out_verts[3].u = s->maxx;
		out_verts[3].v = s->maxy;

		out_verts[4].x = quad[3].x;
		out_verts[4].y = quad[3].y;
		out_verts[4].u = s->minx;
		out_verts[4].v = s->miny;

		out_verts[5].x = quad[2].x;
		out_verts[5].y = quad[2].y;
		out_verts[5].u = s->maxx;
		out_verts[5].v = s->miny;
	}

	// submit call to cute_gl (does not get flushed to screen until `tgFlush` is called)
	gl_push_draw_call(ctx_tg, call);
}

void get_pixels(SPRITEBATCH_U64 image_id, void* buffer, int bytes_to_fill, void* udata)
{
	(void)udata;
	memcpy(buffer, images[image_id], bytes_to_fill);
}

SPRITEBATCH_U64 generate_texture_handle(void* pixels, int w, int h, void* udata)
{
	(void)udata;
	GLuint location;
	glGenTextures(1, &location);
	glBindTexture(GL_TEXTURE_2D, location);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
	return (SPRITEBATCH_U64)location;
}

void destroy_texture_handle(SPRITEBATCH_U64 texture_id, void* udata)
{
	(void)udata;
	GLuint id = (GLuint)texture_id;
	glDeleteTextures(1, &id);
}

void setup_SDL_and_glad()
{
	// Setup SDL and OpenGL and a window
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);

	// Request OpenGL 3.2 context.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	// set double buffer
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	// immediate swaps
	SDL_GL_SetSwapInterval(0);

	SDL_DisplayMode dm;
	if (SDL_GetDesktopDisplayMode(0, &dm))
	{
		SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
		return;
	}

	int screen_w = 640;
	int screen_h = 480;
	int centered_x = dm.w / 2 - screen_w / 2;
	int centered_y = dm.h / 2 - screen_h / 2;
	window = SDL_CreateWindow("cute_spritebatch example", centered_x, centered_y, screen_w, screen_h, SDL_WINDOW_OPENGL|SDL_WINDOW_ALLOW_HIGHDPI);
	ctx_gl = SDL_GL_CreateContext(window);

	gladLoadGLLoader(SDL_GL_GetProcAddress);

	int major, minor;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
	printf("SDL says running on OpenGL ES version %d.%d\n", major, minor);
	printf("Glad says OpenGL ES version : %d.%d\n", GLVersion.major, GLVersion.minor);
	printf("OpenGL says : ES %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
}

void setup_cute_gl()
{
	// setup cute_gl
	int clear_bits = GL_COLOR_BUFFER_BIT;
	int settings_bits = 0;
	ctx_tg = gl_make_ctx(1024, clear_bits, settings_bits);

#define STR(x) #x

	const char* vs = STR(
		#version 330\n

		uniform mat4 u_mvp;

		in vec2 in_pos;
		in vec2 in_uv;

		out vec2 v_uv;

		void main( )
		{
			v_uv = in_uv;
			gl_Position = u_mvp * vec4(in_pos, 0, 1);
		}
	);

	const char* ps = STR(
		#version 330\n
		precision mediump float;
	
		uniform sampler2D u_sprite_texture;

		in vec2 v_uv;
		out vec4 out_col;

		void main()
		{
			out_col = texture(u_sprite_texture, v_uv);
		}
	);

	gl_vertex_data_t vd;
	gl_make_vertex_data(&vd, 1024 * 1024, GL_TRIANGLES, sizeof(vertex_t), GL_DYNAMIC_DRAW);
	gl_add_attribute(&vd, "in_pos", 2, CUTE_GL_FLOAT, CUTE_GL_OFFSET_OF(vertex_t, x));
	gl_add_attribute(&vd, "in_uv", 2, CUTE_GL_FLOAT, CUTE_GL_OFFSET_OF(vertex_t, u));

	gl_make_renderable(&sprite_renderable, &vd);
	gl_load_shader(&sprite_shader, vs, ps);
	gl_set_shader(&sprite_renderable, &sprite_shader);
	
	gl_ortho_2d((float)640, (float)480, 0, 0, projection);
	glViewport(0, 0, 640, 480);

	gl_send_matrix(&sprite_shader, "u_mvp", projection);
	gl_line_mvp(ctx_tg, projection);
}

int tab_count = 0;

#define print_tabs() \
	do { for (int j = 0; j < tab_count; ++j) printf("\t"); } while (0)

#define print_category(name) \
	do { print_tabs(); printf(name " : \n"); } while (0)

#define print(pointer, name, specifier) \
	do { print_tabs(); printf(#name " : " #specifier "\n", pointer -> name); } while (0)

void print_properties(cute_tiled_property_t* properties, int property_count)
{
	print_category("properties");
	++tab_count;
	for (int i = 0; i < property_count; ++i)
	{
		cute_tiled_property_t* p = properties + i;
		print_tabs();
		printf("%s : ", p->name.ptr);
		switch (p->type)
		{
		case CUTE_TILED_PROPERTY_INT: printf("%d\n", p->data.integer); break;
		case CUTE_TILED_PROPERTY_BOOL: printf("%d\n", p->data.boolean); break;
		case CUTE_TILED_PROPERTY_FLOAT: printf("%f\n", p->data.floating); break;
		case CUTE_TILED_PROPERTY_STRING: printf("%s\n", p->data.string.ptr); break;
		case CUTE_TILED_PROPERTY_FILE: printf("%s\n", p->data.file.ptr); break;
		case CUTE_TILED_PROPERTY_COLOR: printf("%d\n", p->data.color); break;
		case CUTE_TILED_PROPERTY_NONE: printf("CUTE_TILED_PROPERTY_NONE\n"); break;
		}
	}
	--tab_count;
}

void print_objects(cute_tiled_object_t* o)
{
	while (o)
	{
		print_category("object");
		++tab_count;
		print(o, ellipse, %d);
		print(o, gid, %d);
		print(o, height, %f);
		print(o, id, %d);
		print(o, name.ptr, %s);
		print(o, point, %d);

		print_category("vertices");
		++tab_count;
		for (int i = 0; i < o->vert_count; i += 2)
		{
			print_tabs();
			printf("%f, %f\n", o->vertices[i], o->vertices[i + 1]);
		}
		--tab_count;

		print(o, vert_type, %d);
		print_properties(o->properties, o->property_count);
		print(o, rotation, %f);
		print(o, type.ptr, %s);
		print(o, visible, %d);
		print(o, width, %f);
		print(o, x, %f);
		print(o, y, %f);

		o = o->next;
		--tab_count;
	}
}

void print_layer(cute_tiled_layer_t* layer)
{
	while (layer)
	{
		print_category("layer"); ++tab_count;
		++tab_count;
		for (int i = 0; i < layer->data_count; ++i) print(layer, data[i], %d);
		--tab_count;
		print(layer, draworder.ptr, %s);
		print(layer, height, %d);
		print(layer, name.ptr, %s);
		print_objects(layer->objects);
		print(layer, opacity, %f);
		print_properties(layer->properties, layer->property_count);
		print(layer, type.ptr, %s);
		print(layer, visible, %d);
		print(layer, width, %d);
		print(layer, x, %d);
		print(layer, y, %d);

		print_layer(layer->layers);

		layer = layer->next;
		--tab_count;
	}
}

void print_tilesets(cute_tiled_tileset_t* tileset)
{
	while (tileset)
	{
		print_category("tileset"); ++tab_count;

		print(tileset, columns, %d);
		print(tileset, firstgid, %d);
		print(tileset, image.ptr, %s);
		print(tileset, imagewidth, %d);
		print(tileset, imageheight, %d);
		print(tileset, margin, %d);
		print(tileset, name.ptr, %s);
		print_properties(tileset->properties, tileset->property_count);
		print(tileset, spacing, %d);
		print(tileset, tilecount, %d);
		print(tileset, tileheight, %d);
		print(tileset, tilewidth, %d);
		print(tileset, type.ptr, %s);
		print(tileset, source.ptr, %s);
		print(tileset, objectalignment.ptr, %s);

		cute_tiled_tile_descriptor_t* tile = tileset->tiles;
		print_category("tiles"); ++tab_count;
		while (tile) {
			print(tile, tile_index, %d);
			print(tile, frame_count, %d);
			print_category("frame"); ++tab_count;
			for (int i = 0; i < tile->frame_count; ++i) {
				cute_tiled_frame_t* frame = tile->animation + i;
				print(frame, duration, %d);
				print(frame, tileid, %d);
			}
			--tab_count;
			print_layer(tile->objectgroup);
			print_properties(tile->properties, tile->property_count);
			print(tile, probability, %f);

			tile = tile->next;
		}
		--tab_count;

		tileset = tileset->next;
		--tab_count;
	}
}

void print_map(cute_tiled_map_t* m)
{
	// print all map contents
	print_category("map"); ++tab_count;
	print(m, backgroundcolor, %d);
	print(m, height, %d);
	print(m, infinite, %d);
	print_layer(m->layers);
	print(m, nextobjectid, %d);
	print(m, orientation.ptr, %s);
	print_properties(m->properties, m->property_count);
	print(m, renderorder.ptr, %s);
	print(m, tiledversion.ptr, %s);
	print(m, tileheight, %d);
	print_tilesets(m->tilesets);
	print(m, tilewidth, %d);
	print(m, type.ptr, %s);
	print(m, version, %f);
	print(m, width, %d);
}

void test_map(const char* path)
{
	cute_tiled_map_t* m = cute_tiled_load_map_from_file(path, 0);
	if (!m) return;
	print_map(m);
	cute_tiled_free_map(m);
}

void swap_buffers()
{
	SDL_GL_SwapWindow(window);
}

// Custom function to pull all tile images out of the cavestory_tiles.png file as separate
// 15x15 images in memory, ready for cute_spritebatch requests as-needed.
void load_images()
{
	int border_padding = 1;
	cp_image_t img = cp_load_png("cavestory_tiles.png");
	int img_count = 0;

	for (int i = 0; i < 171; ++i)
	{
		for (int j = 0; j < 16; ++j)
		{
			int horizontal_index = (border_padding + 15) * j + border_padding;
			int vertical_index = (border_padding + 15) * img.w * i + img.w * border_padding;
			cp_pixel_t* src = img.pix + horizontal_index + vertical_index;
			cp_pixel_t* dst = (cp_pixel_t*)malloc(sizeof(cp_pixel_t) * 15 * 15);

			for (int row = 0; row < 15; ++row)
			{
				cp_pixel_t* src_row = src + row * img.w;
				cp_pixel_t* dst_row = dst + row * 15;
				memcpy(dst_row, src_row, sizeof(cp_pixel_t) * 15);
			}

			images[img_count++] = dst;
		}
	}

	cp_free_png(&img);
}

void free_images()
{
	for (int i = 0; i < sizeof(images) / sizeof(images[0]); ++i)
	{
		free(images[i]);
	}
}

void push_sprite(sprite_t sp)
{
	spritebatch_sprite_t s;
	s.image_id = sp.image_id;
	s.w = 15;
	s.h = 15;
	s.x = sp.x;
	s.y = sp.y;
	s.sx = sp.sx;
	s.sy = sp.sy;
	s.c = sp.c;
	s.s = sp.s;
	s.sort_bits = sp.depth;
	spritebatch_push(&sb, s);
}

void scene0()
{
	for (int i = 0; i < tile_count; ++i)
	{
		push_sprite(tiles0[i]);
	}
}

void scene1()
{
	for (int i = 0; i < tile_count; ++i)
	{
		push_sprite(tiles1[i]);
	}
}

void load_tile_map(sprite_t** tiles, const char* map_path)
{
	cute_tiled_map_t* map = cute_tiled_load_map_from_file(map_path, 0);
	cute_tiled_tileset_t* tileset = map->tilesets;
	if (!map) return;
	print_map(map);

	int map_width = map->width;
	int map_height = map->height;

	*tiles = (sprite_t*)malloc(sizeof(sprite_t) * map_width * map_height);

	// create tiles based off of the parsed map file data
	cute_tiled_layer_t* layer = map->layers;
	int* tile_ids = layer->data;
	tile_count = layer->data_count;

	// assumed true for this test map
	assert(tile_count == map_width * map_height);

	for (int i = 0; i < tile_count; ++i)
	{
		// grab and clear the flipping bits
		int hflip, vflip, dflip;
		int global_tile_id = tile_ids[i];
		cute_tiled_get_flags(global_tile_id, &hflip, &vflip, &dflip);
		global_tile_id = cute_tiled_unset_flags(global_tile_id);

		// assume the map file only has one tileset
		// resolve the tile id based on the matching tileset's first gid
		// the final index can be used on the `images` global array
		int id = global_tile_id - tileset->firstgid;

		sprite_t sprite;
		sprite.image_id = id;
		sprite.depth = 0;
		sprite.x = (float)(i % map_width);
		sprite.y = (float)(map_height - i / map_width);
		sprite.sx = 1.0f;
		sprite.sy = 1.0f;

		// Handle flip flags by enumerating all bit possibilities.
		// Three bits means 2^3 = 8 possible sets. This is to support the
		// flip and rotate tile buttons in Tiled.
		int flags = dflip << 0 | vflip << 1 | hflip << 2;

		// just hard-code the sin/cos values directly since they are all factors of 90 degrees
#define ROTATE_90_CCW(sp) do { sp.c = 0.0f; sp.s = 1.0f; } while (0)
#define ROTATE_90_CW(sp) do { sp.c = 0.0f; sp.s = -1.0f; } while (0)
#define ROTATE_180(sp) do { sp.c = -1.0f; sp.s = 0.0f; } while (0)
#define FLIP_VERTICAL(sp) sp.sy *= -1.0f
#define FLIP_HORIZONTAL(sp) sp.sx *= -1.0f

		sprite.c = 1.0f;
		sprite.s = 0.0f;

		// [hflip, vflip, dflip]
		switch (flags)
		{
		case 1: // 001
			ROTATE_90_CCW(sprite);
			FLIP_VERTICAL(sprite);
			break;

		case 2: // 010
			FLIP_VERTICAL(sprite);
			break;

		case 3: // 011
			ROTATE_90_CCW(sprite);
			break;

		case 4: // 100
			FLIP_HORIZONTAL(sprite);
			break;

		case 5: // 101
			ROTATE_90_CW(sprite);
			break;

		case 6: // 110
			ROTATE_180(sprite);
			break;

		case 7: // 111
			ROTATE_90_CCW(sprite);
			FLIP_HORIZONTAL(sprite);
			break;
		}

		// draw tiles from bottom left corner of each tile
		sprite.x += 0.5f;
		sprite.y -= 0.5f;

		// center all tiles onto the screen
		sprite.x -= (float)(map_width / 2);
		sprite.y -= (float)(map_height / 2);

		// scale by tile size
		sprite.x *= 15;
		sprite.y *= 15;
		sprite.sx *= 15;
		sprite.sy *= 15;

		// scale by factor of two (source pixels are 2x2 pixel blocks on screen)
		sprite.x *= 2.0f;
		sprite.y *= 2.0f;
		sprite.sx *= 2.0f;
		sprite.sy *= 2.0f;

		(*tiles)[i] = sprite;
	}

	cute_tiled_free_map(map);
}

int main(int argc, char** argv)
{
	CUTE_TILED_UNUSED(argc);
	CUTE_TILED_UNUSED(argv);

	test_map("LevelTuto.json");
	return 0;

	setup_SDL_and_glad();
	setup_cute_gl();
	load_images();

	// load up the maps
	load_tile_map(&tiles0, "cavestory_tiles.json");
	load_tile_map(&tiles1, "cavestory_tiles2.json");

	// setup the sprite batcher
	spritebatch_config_t config;
	spritebatch_set_default_config(&config);
	config.pixel_stride = sizeof(cp_pixel_t);
	config.lonely_buffer_count_till_flush = 1;
	config.ticks_to_decay_texture = 1;

	// Assign the four callbacks so the sprite batcher knows how to get
	// pixels whenever it needs them, and assign them GPU texture handles.
	config.batch_callback = batch_report;                       // report batches of sprites from `spritebatch_flush`
	config.get_pixels_callback = get_pixels;                    // used to retrieve image pixels from `spritebatch_flush` and `spritebatch_defrag`
	config.generate_texture_callback = generate_texture_handle; // used to generate a texture handle from `spritebatch_flush` and `spritebatch_defrag`
	config.delete_texture_callback = destroy_texture_handle;    // used to destroy a texture handle from `spritebatch_defrag`

	// initialize cute_spritebatch
	int failed = spritebatch_init(&sb, &config, NULL);
	if (failed)
	{
		printf("spritebatch_init failed due to bad configuration values, or out of memory error.");
		return -1;
	}

	void (*scenes[])() = {
		scene0,
		scene1,
	};
	int scene = 0;

	// game main loop
	int application_running = 1;
	float dt = 0;
	while (application_running)
	{
		dt += ct_time();

		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_QUIT:
				application_running = 0;
				break;

			case SDL_KEYDOWN:
			{
				SDL_Keycode key = event.key.keysym.sym;
				if (key == SDLK_SPACE) scene = (scene + 1) % (sizeof(scenes) / sizeof(*scenes));
			}	break;
			}
		}

		// render 60fps
		if (dt < (1.0f / 60.0f)) continue;
		dt = 0;

		// push some sprites to cute_spritebatch
		// rendering tiles
		scenes[scene]();

		// a typical usage of cute_spritebatch
		// simply defrag each frame, with one tick, and a flush
		// flush will create some cute_gl draw calls
		spritebatch_tick(&sb);
		spritebatch_tick(&sb);
		spritebatch_defrag(&sb);
		spritebatch_flush(&sb);
		sprite_verts_count = 0;

		int calls = gl_draw_call_count(ctx_tg);
		printf("Draw call count: %d\n", calls);

		// sprite batches have been submit to cute_gl, go ahead and flush to screen
		gl_flush(ctx_tg, swap_buffers, 0, 640, 480);
		CUTE_GL_PRINT_GL_ERRORS();
	}

	free_images();
	spritebatch_term(&sb);
	gl_free_ctx(ctx_tg);

	CUTE_ALLOC_CHECK_FOR_LEAKS();

	return 0;
}
