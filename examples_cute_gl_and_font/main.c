#define _CRT_NONSTDC_NO_DEPRECATE
#define _CRT_SECURE_NO_WARNINGS
#include <glad/glad.h>
#include <SDL2/SDL.h>

#define CUTE_GL_IMPLEMENTATION
#include <cute_gl.h>

#define CUTE_FONT_IMPLEMENTATION
#include <cute_font.h>

#define CUTE_PNG_IMPLEMENTATION
#include <cute_png.h>

#include <stdio.h>
#include <stdlib.h>

SDL_Window* window;
SDL_GLContext ctx_gl;
gl_context_t* ctx_tg;
gl_shader_t font_shader;
gl_renderable_t font_renderable;
float projection[16];

int vert_max = 1024;
int vert_count;
cute_font_vert_t* verts;
CUTE_FONT_U64 courier_new_id;
CUTE_FONT_U64 emerald_id;
CUTE_FONT_U64 mitton_id;

CUTE_FONT_U64 generate_texture_handle(void* pixels, int w, int h)
{
	GLuint location;
	glGenTextures(1, &location);
	glBindTexture(GL_TEXTURE_2D, location);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glBindTexture(GL_TEXTURE_2D, 0);
	return (CUTE_FONT_U64)location;
}

void setup_SDL_and_glad(const char* title)
{
	// Setup SDL and OpenGL and a window
	SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);

	// Request OpenGL 3.2 context.
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);

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
	window = SDL_CreateWindow(title, centered_x, centered_y, screen_w, screen_h, SDL_WINDOW_OPENGL|SDL_WINDOW_ALLOW_HIGHDPI);
	ctx_gl = SDL_GL_CreateContext(window);

	gladLoadGLES2Loader(SDL_GL_GetProcAddress);

	int major, minor;
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major);
	SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor);
	printf("SDL says running on OpenGL ES version %d.%d\n", major, minor);
	printf("Glad says OpenGL ES version : %d.%d\n", GLVersion.major, GLVersion.minor);
	printf("OpenGL says : ES %s, GLSL %s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));
}

void setup_tinygl()
{
	// setup tinygl
	int clear_bits = GL_COLOR_BUFFER_BIT;
	int settings_bits = 0;
	ctx_tg = gl_make_ctx(32, clear_bits, settings_bits);

#define STR(x) #x

	const char* vs = STR(
		#version 300 es\n

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
		#version 300 es\n
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
	gl_make_vertex_data(&vd, 1024 * 1024, GL_TRIANGLES, sizeof(cute_font_vert_t), GL_DYNAMIC_DRAW);
	gl_add_attribute(&vd, "in_pos", 2, CUTE_GL_FLOAT, CUTE_GL_OFFSET_OF(cute_font_vert_t, x));
	gl_add_attribute(&vd, "in_uv", 2, CUTE_GL_FLOAT, CUTE_GL_OFFSET_OF(cute_font_vert_t, u));

	gl_make_renderable(&font_renderable, &vd);
	gl_load_shader(&font_shader, vs, ps);
	gl_set_shader(&font_renderable, &font_shader);
	
	gl_ortho_2d((float)640 / 2.0f, (float)480 / 2.0f, 0, 0, projection);
	glViewport(0, 0, 640, 480);

	gl_send_matrix(&font_shader, "u_mvp", projection);
	gl_line_mvp(ctx_tg, projection);

	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void swap_buffers()
{
	SDL_GL_SwapWindow(window);
}

static void* read_file(const char* path, int* size)
{
	void* data = 0;
	FILE* fp = fopen(path, "rb");
	int sz = 0;

	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		sz = (int)ftell(fp);
		fseek(fp, 0, SEEK_SET);
		data = malloc(sz + 1);
		((char*)data)[sz] = 0;
		fread(data, sz, 1, fp);
		fclose(fp);
	}

	if (size) *size = sz;
	return data;
}

void draw_text(cute_font_t* font, const char* text, float x, float y, float line_height)
{
	float w = (float)cute_font_text_width(font, text);
	float h = (float)cute_font_text_height(font, text);
	cute_font_fill_vertex_buffer(font, text, x + -w / 2, y + h / 2, line_height, verts, 1024, &vert_count);

	gl_draw_call_t call;
	call.textures[0] = (uint32_t)font->atlas_id;
	call.texture_count = 1;
	call.r = &font_renderable;
	call.verts = verts;
	call.vert_count = vert_count;

	gl_push_draw_call(ctx_tg, call);
}

int main(int argc, char** argv)
{
	setup_SDL_and_glad("tinyfont demo");
	setup_tinygl();

	// Load Courier New exported from BMFont.
	// See: http://www.angelcode.com/products/bmfont/
	int courier_new_size = 0;
	void* courier_new_memory = read_file("courier_new.fnt", &courier_new_size);
	cp_image_t img = cp_load_png("courier_new_0.png");
	courier_new_id = generate_texture_handle(img.pix, img.w, img.h);
	cute_font_t* courier_new = cute_font_load_bmfont(courier_new_id, courier_new_memory, courier_new_size, 0);
	if (courier_new->atlas_w != img.w) return -1;
	if (courier_new->atlas_h != img.h) return -1;
	free(courier_new_memory);
	cp_free_png(&img);

	// Load a typical ASCII font of 128 characters. Finds glyphs by a scanning algorithm. Each glyph
	// must be wrapped around a border of the "border color". Open up emerald.png to see what is meant here.
	// Note that each line must have the same height (constant font height), so the scanning algorithm
	// can both work and remain simple. The font is made to look like Pokemon Emerald's font.
	int emerald_size = 0;
	void* emerald_memory = read_file("emerald.png", &emerald_size);
	img = cp_load_png_mem(emerald_memory, emerald_size);
	emerald_id = generate_texture_handle(img.pix, img.w, img.h);
	cute_font_t* emerald = cute_font_load_ascii(emerald_id, img.pix, img.w, img.h, sizeof(cp_pixel_t), 0);
	if (emerald->atlas_w != img.w) return -1;
	if (emerald->atlas_h != img.h) return -1;
	free(emerald_memory);
	cp_free_png(&img);

	// Load up a font for codepage 1252 (extended ASCII). Uses same as above glyph scanning algorithm. The font
	// image is from Mitton's TIGR.
	// See: https://bitbucket.org/rmitton/tigr/src/default/
	int mitton_size = 0;
	void* mitton_memory = read_file("mitton.png", &mitton_size);
	img = cp_load_png_mem(mitton_memory, mitton_size);
	mitton_id = generate_texture_handle(img.pix, img.w, img.h);
	cute_font_t* mitton = cute_font_load_1252(mitton_id, img.pix, img.w, img.h, sizeof(cp_pixel_t), 0);
	if (mitton->atlas_w != img.w) return -1;
	if (mitton->atlas_h != img.h) return -1;
	free(mitton_memory);
	cp_free_png(&img);

	verts = malloc(sizeof(cute_font_vert_t) * 1024);
	const char* sample_text = (const char*)read_file("sample_text.txt", 0);

	int application_running = 1;
	int which = 0;
	while (application_running)
	{
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
				if (key == SDLK_SPACE) which = which + 1 < 3 ? which + 1 : 0;
			}	break;
			}
		}

		switch (which)
		{
		case 0: draw_text(courier_new, sample_text, 0, 0, 1); break;
		case 1: draw_text(emerald, sample_text, 0, 0, 2); break;
		case 2: draw_text(mitton, sample_text, 0, 0, 1); break;
		}

		gl_flush(ctx_tg, swap_buffers, 0, 640, 480);
		CUTE_GL_PRINT_GL_ERRORS();
	}

	return 0;
}
