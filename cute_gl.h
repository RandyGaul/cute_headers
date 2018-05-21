/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_gl.h - v1.02

	To create implementation (the function definitions)
		#define CUTE_GL_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	Summary:
	Wrapper for OpenGL ES 3.0+ for vertex attributes, shader creation, draw calls,
	and post-processing FX. The API was carefully designed to facilitate trying out
	one-off techniques or experiments, of which are critical for certain kinds
	of development styles (i.e. fast iteration). Credit goes to thatgamecompany
	for the original designs + idea of this API style.
*/

/*
	DOCUMENTATION (quick intro):
	1. create context
	2. define vertices
	3. load shader
	4. create renderable
	5. compose draw calls
	6. flush

	1. void* ctx = gl_make_ctx(max_draw_calls_per_flush, clear_bits, settings_bits)
	2. gl_make_vertex_data(...) and gl_add_attribute(...)
	3. gl_load_shader(&shader_instance, vertex_shader_string, pixel_shader_string)
	4. gl_make_renderable(&renderable_instance, &vertex_data_instance)
	5. gl_push_draw_call(ctx, draw_call)
	6. gl_flush(ctx, swap_buffer_func, use_post_fx ? &frame_buffer : 0, viewport_w, viewport_h)

	DETAILS:
	cute_gl only renders triangles. Triangles can be fed to draw calls with static
	or dynamic geometry. Dynamic geometry is sent to the GPU upon each draw call,
	while static geometry is sent only upon the first draw call. Dynamic vertices
	are triple buffered using fence syncs for optimal performance and screen tear-
	ing avoidance.

	cute_gl does come with a debug line renderer built in! Can be turned off at
	compile time if not used (see CUTE_GL_LINE_RENDERER). This is perfect for creating
	a global line rendering utility for debugging purposes.

	Post processing fx are done with a frame buffer, and are done only once with
	one shader, after all draw calls have been processed.

	For full examples of use please visit either of these links:
		example to render various 2d shapes + post fx
			https://github.com/RandyGaul/tinyheaders/tree/master/examples_cute_gl_and_tinyc2

		example of rendering 3d geometry + post fx, a full game from a game jam (windows only)
			https://github.com/RandyGaul/pook
*/

/*
	Revision history:
		1.0  (02/09/2017) initial release
		1.01 (03/23/2017) memory leak fix and swap to realloc for gl_line stuff
		1.02 (11/02/2017) change to GLES 3.0+, removed glu debug dependency
*/

/*
	Contributors:
		Andrew Hung       1.0  - initial frame buffer implementation, various bug fixes
		to-miz            1.01 - memory leak fix and swap to realloc for gl_line stuff
*/

/*
	Some Current Limitations
		* No index support. Adding indices would not be too hard and come down to a
			matter of adding in some more triple buffer/single buffer code for
			GL_STATIC_DRAW vs GL_DYNAMIC_DRAW.
		* GLES 3.0+ support only
		* Full support for array uniforms is not quite tested and hammered out.
*/

#if !defined(CUTE_GL_H)

#include <stdint.h>

// recommended to leave this on as long as possible (perhaps until release)
#define CUTE_GL_DEBUG_CHECKS 1

// feel free to turn this off if unused.
// This is used to render lines on-top of all other rendering (besides post fx); rendered with no depth testing.
#define CUTE_GL_LINE_RENDERER 1

enum
{
	CUTE_GL_FLOAT,
	CUTE_GL_INT,
	CUTE_GL_BOOL,
	CUTE_GL_SAMPLER,
	CUTE_GL_UNKNOWN,
};

typedef struct gl_vertex_attribute_t
{
	const char* name;
	uint64_t hash;
	uint32_t size;
	uint32_t type;
	uint32_t offset;
	uint32_t location;
} gl_vertex_attribute_t;

#define CUTE_GL_ATTRIBUTE_MAX_COUNT 16
typedef struct gl_vertex_data_t
{
	uint32_t buffer_size;
	uint32_t vertex_stride;
	uint32_t primitive;
	uint32_t usage;

	uint32_t attribute_count;
	gl_vertex_attribute_t attributes[CUTE_GL_ATTRIBUTE_MAX_COUNT];
} gl_vertex_data_t;

// adjust this as necessary to create your own draw call ordering
// see: http://realtimecollisiondetection.net/blog/?p=86
typedef struct gl_render_internal_state_t
{
	union
	{
		struct
		{
			int fullscreen    : 2;
			int hud           : 5;
			int depth         : 25;
			int translucency  : 32;
		} bits;

		uint64_t key;
	} u;
} gl_render_internal_state_t;

struct gl_shader_t;
typedef struct gl_shader_t gl_shader_t;

typedef struct gl_renderable_t
{
	gl_vertex_data_t data;
	gl_shader_t* program;
	gl_render_internal_state_t state;
	uint32_t attribute_count;

	uint32_t index0;
	uint32_t index1;
	uint32_t buffer_number;
	uint32_t need_new_sync;
	uint32_t buffer_count;
	uint32_t buffers[3];
	GLsync fences[3];
} gl_renderable_t;

#define CUTE_GL_UNIFORM_NAME_LENGTH 64
#define CUTE_GL_UNIFORM_MAX_COUNT 16

typedef struct gl_uniform_t
{
	char name[CUTE_GL_UNIFORM_NAME_LENGTH];
	uint32_t id;
	uint64_t hash;
	uint32_t size;
	uint32_t type;
	uint32_t location;
} gl_uniform_t;

struct gl_shader_t
{
	uint32_t program;
	uint32_t uniform_count;
	gl_uniform_t uniforms[CUTE_GL_UNIFORM_MAX_COUNT];
};

typedef struct gl_framebuffer_t
{
	uint32_t fb_id;
	uint32_t tex_id;
	uint32_t rb_id;
	uint32_t quad_id;
	gl_shader_t* shader;
	int w, h;
} gl_framebuffer_t;

typedef struct
{
	uint32_t vert_count;
	void* verts;
	gl_renderable_t* r;
	uint32_t texture_count;
	uint32_t textures[8];
} gl_draw_call_t;

struct gl_context_t;
typedef struct gl_context_t gl_context_t;

gl_context_t* gl_make_ctx(uint32_t max_draw_calls, uint32_t clear_bits, uint32_t settings_bits);
void gl_free_ctx(void* ctx);

void gl_line_mvp(void* context, float* mvp);
void gl_line_color(void* context, float r, float g, float b);
void gl_line(void* context, float ax, float ay, float az, float bx, float by, float bz);
void gl_line_width(float width);
void gl_line_depth_test(void* context, int zero_for_off);

void gl_make_frame_buffer(gl_framebuffer_t* fb, gl_shader_t* shader, int w, int h, int use_depth_test);
void gl_free_frame_buffer(gl_framebuffer_t* fb);

void gl_make_vertex_data(gl_vertex_data_t* vd, uint32_t buffer_size, uint32_t primitive, uint32_t vertex_stride, uint32_t usage);
void gl_add_attribute(gl_vertex_data_t* vd, char* name, uint32_t size, uint32_t type, uint32_t offset);
void gl_make_renderable(gl_renderable_t* r, gl_vertex_data_t* vd);

// Must be called after gl_make_renderable
void gl_set_shader(gl_renderable_t* r, gl_shader_t* s);
void gl_load_shader(gl_shader_t* s, const char* vertex, const char* pixel);
void gl_free_shader(gl_shader_t* s);

void gl_set_active_shader(gl_shader_t* s);
void gl_deactivate_shader();
void gl_send_f32(gl_shader_t* s, char* uniform_name, uint32_t size, float* floats, uint32_t count);
void gl_send_matrix(gl_shader_t* s, char* uniform_name, float* floats);
void gl_send_texture(gl_shader_t* s, char* uniform_name, uint32_t index);

void gl_push_draw_call(void* ctx, gl_draw_call_t call);

typedef void (gl_func_t)();
void gl_flush(void* ctx, gl_func_t* swap, gl_framebuffer_t* fb, int w, int h);
int gl_draw_call_tCount(void* ctx);

// 4x4 matrix helper functions
void gl_ortho_2d(float w, float h, float x, float y, float* m);
void gl_perspective(float* m, float y_fov_radians, float aspect, float n, float f);
void gl_mul(float* a, float* b, float* out); // perform a * b, stores result in out
void gl_identity(float* m);
void gl_copy(float* dst, float* src);

#if CUTE_GL_DEBUG_CHECKS

	#define CUTE_GL_PRINT_GL_ERRORS() gl_print_errors_internal(__FILE__, __LINE__)
	void gl_print_errors_internal(char* file, uint32_t line);

#endif

#define CUTE_GL_H
#endif

#ifdef CUTE_GL_IMPLEMENTATION
#ifndef CUTE_GL_IMPLEMENTATION_ONCE
#define CUTE_GL_IMPLEMENTATION_ONCE

#define CUTE_GL_OFFSET_OF(type, member) ((uint32_t)((size_t)(&((type*)0)->member)))

#if CUTE_GL_DEBUG_CHECKS

	#include <stdio.h>
	#include <assert.h>
	#define CUTE_GL_ASSERT assert
	#define CUTE_GL_WARN printf

#else

	#define CUTE_GL_ASSERT(...)
	#define CUTE_GL_WARN(...)

#endif

struct gl_context_t
{
	uint32_t clear_bits;
	uint32_t settings_bits;
	uint32_t max_draw_calls;
	uint32_t count;
	gl_draw_call_t* calls;

#if CUTE_GL_LINE_RENDERER
	gl_renderable_t line_r;
	gl_shader_t line_s;
	uint32_t line_vert_count;
	uint32_t line_vert_capacity;
	float* line_verts;
	float r, g, b;
	int line_depth_test;
#endif
};

#if !defined(CUTE_GL_MALLOC)
	#include <stdlib.h> // malloc, free, NULL
	#define CUTE_GL_MALLOC(size) malloc(size)
	#define CUTE_GL_FREE(mem) free(mem)
#endif

#include <string.h> // memset

gl_context_t* gl_make_ctx(uint32_t max_draw_calls, uint32_t clear_bits, uint32_t settings_bits)
{
	gl_context_t* ctx = (gl_context_t*)CUTE_GL_MALLOC(sizeof(gl_context_t));
	ctx->clear_bits = clear_bits;
	ctx->settings_bits = settings_bits;
	ctx->max_draw_calls = max_draw_calls;
	ctx->count = 0;
	ctx->calls = (gl_draw_call_t*)CUTE_GL_MALLOC(sizeof(gl_draw_call_t) * max_draw_calls);
	if (!ctx->calls)
	{
		CUTE_GL_FREE(ctx);
		return 0;
	}
	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

#if CUTE_GL_LINE_RENDERER
	#define CUTE_GL_LINE_STRIDE (sizeof(float) * 3 * 2)
	gl_vertex_data_t vd;
	gl_make_vertex_data(&vd, 1024 * 1024, GL_LINES, CUTE_GL_LINE_STRIDE, GL_DYNAMIC_DRAW);
	gl_add_attribute(&vd, "in_pos", 3, CUTE_GL_FLOAT, 0);
	gl_add_attribute(&vd, "in_col", 3, CUTE_GL_FLOAT, CUTE_GL_LINE_STRIDE / 2);
	gl_make_renderable(&ctx->line_r, &vd);
	const char* vs = "#version 300 es\nuniform mat4 u_mvp;in vec3 in_pos;in vec3 in_col;out vec3 v_col;void main(){v_col = in_col;gl_Position = u_mvp * vec4(in_pos, 1);}";
	const char* ps = "#version 300 es\nprecision mediump float;in vec3 v_col;out vec4 out_col;void main(){out_col = vec4(v_col, 1);}";
	gl_load_shader(&ctx->line_s, vs, ps);
	gl_set_shader(&ctx->line_r, &ctx->line_s);
	gl_line_color(ctx, 1.0f, 1.0f, 1.0f);
	ctx->line_vert_count = 0;
	ctx->line_vert_capacity = 1024 * 1024;
	ctx->line_verts = (float*)CUTE_GL_MALLOC(CUTE_GL_LINE_STRIDE * ctx->line_vert_capacity);
	ctx->line_depth_test = 0;
#endif

	return ctx;
}

void gl_free_ctx(void* ctx)
{
	gl_context_t* context = (gl_context_t*)ctx;
	CUTE_GL_FREE(context->calls);
#if CUTE_GL_LINE_RENDERER
	CUTE_GL_FREE(context->line_verts);
#endif
	CUTE_GL_FREE(context);
}

#if CUTE_GL_LINE_RENDERER
void gl_line_mvp(void* context, float* mvp)
{
	gl_context_t* ctx = (gl_context_t*)context;
	gl_send_matrix(&ctx->line_s, "u_mvp", mvp);
}

void gl_line_color(void* context, float r, float g, float b)
{
	gl_context_t* ctx = (gl_context_t*)context;
	ctx->r = r; ctx->g = g; ctx->b = b;
}

void gl_line(void* context, float ax, float ay, float az, float bx, float by, float bz)
{
	gl_context_t* ctx = (gl_context_t*)context;
	if (ctx->line_vert_count + 2 > ctx->line_vert_capacity)
	{
		ctx->line_vert_capacity *= 2;
		void* old_verts = ctx->line_verts;
		ctx->line_verts = (float*)CUTE_GL_MALLOC(CUTE_GL_LINE_STRIDE * ctx->line_vert_capacity);
		memcpy(ctx->line_verts, old_verts, CUTE_GL_LINE_STRIDE * ctx->line_vert_count);
		CUTE_GL_FREE(old_verts);
	}
	float verts[12];
	verts[0] = ax; verts[1] = ay; verts[2] = az; verts[3] = ctx->r; verts[4] = ctx->g; verts[5] = ctx->b; verts[6] = bx;
	verts[7] = by; verts[8] = bz; verts[9] = ctx->r; verts[10] = ctx->g; verts[11] = ctx->b;
	memcpy(ctx->line_verts + ctx->line_vert_count * (CUTE_GL_LINE_STRIDE / sizeof(float)), verts, sizeof(verts));
	ctx->line_vert_count += 2;
}

void gl_line_width(float width)
{
	glLineWidth(width);
	CUTE_GL_PRINT_GL_ERRORS(); // common errors here unfortunately
}

void gl_line_depth_test(void* context, int zero_for_off)
{
	gl_context_t* ctx = (gl_context_t*)context;
	ctx->line_depth_test = zero_for_off;
}
#else
#define CUTE_GL_UNUSED(x) (void)(x);
void gl_line_mvp(void* context, float* mvp) { CUTE_GL_UNUSED(context); CUTE_GL_UNUSED(mvp); }
void gl_line_color(void* context, float r, float g, float b) { CUTE_GL_UNUSED(context); CUTE_GL_UNUSED(r); CUTE_GL_UNUSED(g); CUTE_GL_UNUSED(b); }
void gl_line(void* context, float ax, float ay, float az, float bx, float by, float bz)
{ CUTE_GL_UNUSED(context); CUTE_GL_UNUSED(ax); CUTE_GL_UNUSED(ay); CUTE_GL_UNUSED(az); CUTE_GL_UNUSED(bx); CUTE_GL_UNUSED(by); CUTE_GL_UNUSED(bz); }
void gl_line_width(float width) { CUTE_GL_UNUSED(width); }
void gl_line_depth_test(void* context, int zero_for_off) { CUTE_GL_UNUSED(context); CUTE_GL_UNUSED(zero_for_off); }
#endif

void gl_make_frame_buffer(gl_framebuffer_t* fb, gl_shader_t* shader, int w, int h, int use_depth_test)
{
	// Generate the frame buffer
	GLuint fb_id;
	glGenFramebuffers(1, &fb_id);
	glBindFramebuffer(GL_FRAMEBUFFER, fb_id);

	// Generate a texture to use as the color buffer.
	GLuint tex_id;
	glGenTextures(1, &tex_id);
	glBindTexture(GL_TEXTURE_2D, tex_id);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	// Attach color buffer to frame buffer
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0);

	// Generate depth and stencil attachments for the fb using a RenderBuffer.
	GLuint rb_id = (GLuint)~0;
	if (use_depth_test)
	{
		glGenRenderbuffers(1, &rb_id);
		glBindRenderbuffer(GL_RENDERBUFFER, rb_id);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb_id);
	}

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		CUTE_GL_WARN("WARNING cute_gl: failed to generate framebuffer\n");

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Prepare quad
	GLuint quad_id;
	glGenBuffers(1, &quad_id);
	glBindBuffer(GL_ARRAY_BUFFER, quad_id);
	static GLfloat quad[] = {
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,

		-1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f
	};
	glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	fb->fb_id = fb_id;
	fb->tex_id = tex_id;
	fb->rb_id = rb_id;
	fb->quad_id = quad_id;
	fb->shader = shader;
	fb->w = w;
	fb->h = h;
}

void gl_free_frame_buffer(gl_framebuffer_t* fb)
{
	glDeleteTextures(1, &fb->tex_id);
	glDeleteRenderbuffers(1, &fb->rb_id);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fb->fb_id);
	glDeleteBuffers(1, &fb->quad_id);
	memset(fb, 0, sizeof(gl_framebuffer_t));
}


uint64_t gl_FNV1a(const char* str)
{
	uint64_t h = (uint64_t)14695981039346656037;
	char c;

	while (c = *str++)
	{
		h = h ^ (uint64_t)c;
		h = h * (uint64_t)1099511628211;
	}

	return h;
}

void gl_make_vertex_data(gl_vertex_data_t* vd, uint32_t buffer_size, uint32_t primitive, uint32_t vertex_stride, uint32_t usage)
{
	vd->buffer_size = buffer_size;
	vd->vertex_stride = vertex_stride;
	vd->primitive = primitive;
	vd->usage = usage;
	vd->attribute_count = 0;
}

static uint32_t gl_get_gl_type_internal(uint32_t type)
{
	switch (type)
	{
	case GL_INT:
	case GL_INT_VEC2:
	case GL_INT_VEC3:
	case GL_INT_VEC4:
		return CUTE_GL_INT;

	case GL_FLOAT:
	case GL_FLOAT_VEC2:
	case GL_FLOAT_VEC3:
	case GL_FLOAT_VEC4:
	case GL_FLOAT_MAT2:
	case GL_FLOAT_MAT3:
	case GL_FLOAT_MAT4:
		return CUTE_GL_FLOAT;

	case GL_BOOL:
	case GL_BOOL_VEC2:
	case GL_BOOL_VEC3:
	case GL_BOOL_VEC4:
		return CUTE_GL_BOOL;

// seems undefined for GLES
#if GL_SAMPLER_1D
	case GL_SAMPLER_1D:
#endif
	case GL_SAMPLER_2D:
	case GL_SAMPLER_3D:
		return CUTE_GL_SAMPLER;

	default:
		return CUTE_GL_UNKNOWN;
	}
}

void gl_add_attribute(gl_vertex_data_t* vd, char* name, uint32_t size, uint32_t type, uint32_t offset)
{
	gl_vertex_attribute_t va;
	va.name = name;
	va.hash = gl_FNV1a(name);
	va.size = size;
	va.type = type;
	va.offset = offset;

	CUTE_GL_ASSERT(vd->attribute_count < CUTE_GL_ATTRIBUTE_MAX_COUNT);
	vd->attributes[vd->attribute_count++] = va;
}

void gl_make_renderable(gl_renderable_t* r, gl_vertex_data_t* vd)
{
	r->data = *vd;
	r->index0 = 0;
	r->index1 = 0;
	r->buffer_number = 0;
	r->need_new_sync = 0;
	r->program = 0;
	r->state.u.key = 0;

	if (vd->usage == GL_STATIC_DRAW)
	{
		r->buffer_count = 1;
		r->need_new_sync = 1;
	}
	else r->buffer_count = 3;
}

// WARNING: Messes with GL global state via glUnmapBuffer(GL_ARRAY_BUFFER) and
// glBindBuffer(GL_ARRAY_BUFFER, ...), so call gl_map_internal, fill in data, then call gl_unmap_internal.
void* gl_map_internal(gl_renderable_t* r, uint32_t count)
{
	// Cannot map a buffer when the buffer is too small
	// Make your buffer is bigger or draw less data
	CUTE_GL_ASSERT(count <= r->data.buffer_size);

	uint32_t newIndex = r->index1 + count;

	if (newIndex > r->data.buffer_size)
	{
		// should never overflow a static buffer
		CUTE_GL_ASSERT(r->data.usage != GL_STATIC_DRAW);

		++r->buffer_number;
		r->buffer_number %= r->buffer_count;
		GLsync fence = r->fences[r->buffer_number];

		// Ensure buffer is not in use by GPU
		// If we stall here we are GPU bound
		GLenum result = glClientWaitSync(fence, 0, (GLuint64)1000000000);
		CUTE_GL_ASSERT(result != GL_TIMEOUT_EXPIRED);
		CUTE_GL_ASSERT(result != GL_WAIT_FAILED);
		glDeleteSync(fence);

		r->index0 = 0;
		r->index1 = count;
		r->need_new_sync = 1;
	}

	else
	{
		r->index0 = r->index1;
		r->index1 = newIndex;
	}

	glBindBuffer(GL_ARRAY_BUFFER, r->buffers[r->buffer_number]);
	uint32_t stream_size = (r->index1 - r->index0) * r->data.vertex_stride;
	void* memory = glMapBufferRange(GL_ARRAY_BUFFER, r->index0 * r->data.vertex_stride, stream_size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT);

#if CUTE_GL_DEBUG_CHECKS
	if (!memory)
	{
		CUTE_GL_WARN("\n%u\n", glGetError());
		CUTE_GL_ASSERT(memory);
	}
#endif

	return memory;
}

void gl_unmap_internal()
{
	glUnmapBuffer(GL_ARRAY_BUFFER);
}

void gl_set_shader(gl_renderable_t* r, gl_shader_t* program)
{
	// Cannot set the shader of a Renderable more than once
	CUTE_GL_ASSERT(!r->program);

	r->program = program;
	glGetProgramiv(program->program, GL_ACTIVE_ATTRIBUTES, (GLint*)&r->attribute_count);

#if CUTE_GL_DEBUG_CHECKS
	if (r->attribute_count != r->data.attribute_count)
	{
		CUTE_GL_WARN("Mismatch between VertexData attribute count (%d), and shader attribute count (%d).\n",
				r->attribute_count,
				r->data.attribute_count);
	}
#endif

	uint32_t size;
	uint32_t type;
	char buffer[256];
	uint64_t hash;

	// Query and set all attribute locations as defined by the shader linking
	for (uint32_t i = 0; i < r->attribute_count; ++i)
	{
		glGetActiveAttrib(program->program, i, 256, 0, (GLint*)&size, (GLenum*)&type, buffer);
		hash = gl_FNV1a(buffer);
		type = gl_get_gl_type_internal(type);

#if CUTE_GL_DEBUG_CHECKS
		gl_vertex_attribute_t* a = 0;

		// Make sure data.AddAttribute(name, ...) has matching named attribute
		// Also make sure the GL::Type matches
		// This helps to catch common mismatch errors between glsl and C++
		for (uint32_t j = 0; j < r->data.attribute_count; ++j)
		{
			gl_vertex_attribute_t* b = r->data.attributes + j;

			if (b->hash == hash)
			{
				a = b;
				break;
			}
		}
#endif

		// Make sure the user did not have a mismatch between VertexData
		// attributes and the attributes defined in the vertex shader
		CUTE_GL_ASSERT(a);
		CUTE_GL_ASSERT(a->type == type);

		a->location = glGetAttribLocation(program->program, buffer);
	}

	// Generate VBOs and initialize fences
	uint32_t usage = r->data.usage;

	for (uint32_t i = 0; i < r->buffer_count; ++i)
	{
		GLuint* data_buffer = (GLuint*)r->buffers + i;

		glGenBuffers(1, data_buffer);
		glBindBuffer(GL_ARRAY_BUFFER, *data_buffer);
		glBufferData(GL_ARRAY_BUFFER, r->data.buffer_size * r->data.vertex_stride, NULL, usage);
		r->fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}

GLuint gl_compile_shader_internal(const char* Shader, uint32_t type)
{
	GLuint handle = glCreateShader(type);
	glShaderSource(handle, 1, (const GLchar**)&Shader, NULL);
	glCompileShader(handle);

	uint32_t compiled;
	glGetShaderiv(handle, GL_COMPILE_STATUS, (GLint*)&compiled);

#if CUTE_GL_DEBUG_CHECKS
	if (!compiled)
	{
		CUTE_GL_WARN("Shader of type %d failed compilation.\n", type);
		char out[2000];
		GLsizei outLen;
		glGetShaderInfoLog(handle, 2000, &outLen, out);
		CUTE_GL_WARN("%s\n", out);
		CUTE_GL_ASSERT(0);
	}
#endif

	return handle;
}

void gl_load_shader(gl_shader_t* s, const char* vertex, const char* pixel)
{
	// Compile vertex and pixel Shader
	uint32_t program = glCreateProgram();
	uint32_t vs = gl_compile_shader_internal(vertex, GL_VERTEX_SHADER);
	uint32_t ps = gl_compile_shader_internal(pixel, GL_FRAGMENT_SHADER);
	glAttachShader(program, vs);
	glAttachShader(program, ps);

	// Link the Shader to form a program
	glLinkProgram(program);

	uint32_t linked;
	glGetProgramiv(program, GL_LINK_STATUS, (GLint*)&linked);

#if CUTE_GL_DEBUG_CHECKS
	if (!linked)
	{
		CUTE_GL_WARN("Shaders failed to link.\n");
		char out[2000];
		GLsizei outLen;
		glGetProgramInfoLog(program, 2000, &outLen, out);
		CUTE_GL_WARN("%s\n", out);
		CUTE_GL_ASSERT(0);
	}
#endif

	glDetachShader(program, vs);
	glDetachShader(program, ps);
	glDeleteShader(vs);
	glDeleteShader(ps);

	// Insert Shader into the Shaders array for future lookups
	s->program = program;

	// Query Uniform information and fill out the Shader Uniforms
	GLint uniform_count;
	uint32_t nameSize = sizeof(char) * CUTE_GL_UNIFORM_NAME_LENGTH;
	glGetProgramiv(program, GL_ACTIVE_UNIFORMS, &uniform_count);
	CUTE_GL_ASSERT(uniform_count < CUTE_GL_UNIFORM_MAX_COUNT);
	s->uniform_count = uniform_count;

	for (uint32_t i = 0; i < (uint32_t)uniform_count; ++i)
	{
		uint32_t nameLength;
		gl_uniform_t u;

		glGetActiveUniform(program, (GLint)i, nameSize, (GLsizei*)&nameLength, (GLsizei*)&u.size, (GLenum*)&u.type, u.name);

		// Uniform named in a Shader is too long for the UNIFORM_NAME_LENGTH constant
		CUTE_GL_ASSERT(nameLength <= CUTE_GL_UNIFORM_NAME_LENGTH);

		u.location = glGetUniformLocation(program, u.name);
		u.type = gl_get_gl_type_internal(u.type);
		u.hash = gl_FNV1a(u.name);
		u.id = i;

		// @TODO: Perhaps need to handle appended [0] to Uniform names?

		s->uniforms[i] = u;
	}

#if CUTE_GL_DEBUG_CHECKS
	// prevent hash collisions
	for (uint32_t i = 0; i < (uint32_t)uniform_count; ++i)
		for (uint32_t j = i + 1; j < (uint32_t)uniform_count; ++j)
			CUTE_GL_ASSERT(s->uniforms[i].hash != s->uniforms[j].hash);
#endif
}

void gl_free_shader(gl_shader_t* s)
{
	glDeleteProgram(s->program);
	memset(s, 0, sizeof(gl_shader_t));
}

gl_uniform_t* gl_find_uniform_internal(gl_shader_t* s, char* name)
{
	uint32_t uniform_count = s->uniform_count;
	gl_uniform_t* uniforms = s->uniforms;
	uint64_t hash = gl_FNV1a(name);

	for (uint32_t i = 0; i < uniform_count; ++i)
	{
		gl_uniform_t* u = uniforms + i;

		if (u->hash == hash)
		{
			return u;
		}
	}

	return 0;
}

void gl_set_active_shader(gl_shader_t* s)
{
	glUseProgram(s->program);
}

void gl_deactivate_shader()
{
	glUseProgram(0);
}

void gl_send_f32(gl_shader_t* s, char* uniform_name, uint32_t size, float* floats, uint32_t count)
{
	gl_uniform_t* u = gl_find_uniform_internal(s, uniform_name);

	if (!u)
	{
		CUTE_GL_WARN("Unable to find uniform: %s\n", uniform_name);
		return;
	}

	CUTE_GL_ASSERT(size == u->size);
	CUTE_GL_ASSERT(u->type == CUTE_GL_FLOAT);

	gl_set_active_shader(s);
	switch (count)
	{
	case 1:
		glUniform1f(u->location, floats[0]);
		break;

	case 2:
		glUniform2f(u->location, floats[0], floats[1]);
		break;

	case 3:
		glUniform3f(u->location, floats[0], floats[1], floats[2]);
		break;

	case 4:
		glUniform4f(u->location, floats[0], floats[1], floats[2], floats[3]);
		break;

	default:
		CUTE_GL_ASSERT(0);
		break;
	}
	gl_deactivate_shader();
}

void gl_send_matrix(gl_shader_t* s, char* uniform_name, float* floats)
{
	gl_uniform_t* u = gl_find_uniform_internal(s, uniform_name);

	if (!u)
	{
		CUTE_GL_WARN("Unable to find uniform: %s\n", uniform_name);
		return;
	}

	CUTE_GL_ASSERT(u->size == 1);
	CUTE_GL_ASSERT(u->type == CUTE_GL_FLOAT);

	gl_set_active_shader(s);
	glUniformMatrix4fv(u->id, 1, 0, floats);
	gl_deactivate_shader();
}

void gl_send_texture(gl_shader_t* s, char* uniform_name, uint32_t index)
{
	gl_uniform_t* u = gl_find_uniform_internal(s, uniform_name);

	if (!u)
	{
		CUTE_GL_WARN("Unable to find uniform: %s\n", uniform_name);
		return;
	}

	CUTE_GL_ASSERT(u->type == CUTE_GL_SAMPLER);

	gl_set_active_shader(s);
	glUniform1i(u->location, index);
	gl_deactivate_shader();
}

static uint32_t gl_call_sort_pred_internal(gl_draw_call_t* a, gl_draw_call_t* b)
{
	return a->r->state.u.key < b->r->state.u.key;
}

static void gl_qsort_internal(gl_draw_call_t* items, uint32_t count)
{
	if (count <= 1) return;

	gl_draw_call_t pivot = items[count - 1];
	uint32_t low = 0;
	for (uint32_t i = 0; i < count - 1; ++i)
	{
		if (gl_call_sort_pred_internal(items + i, &pivot))
		{
			gl_draw_call_t tmp = items[i];
			items[i] = items[low];
			items[low] = tmp;
			low++;
		}
	}

	items[count - 1] = items[low];
	items[low] = pivot;
	gl_qsort_internal(items, low);
	gl_qsort_internal(items + low + 1, count - 1 - low);
}

void gl_push_draw_call(void* ctx, gl_draw_call_t call)
{
	gl_context_t* context = (gl_context_t*)ctx;
	CUTE_GL_ASSERT(context->count < context->max_draw_calls);
	context->calls[context->count++] = call;
}

uint32_t gl_get_enum(uint32_t type)
{
	switch (type)
	{
	case CUTE_GL_FLOAT:
		return GL_FLOAT;
		break;

	case CUTE_GL_INT:
		return GL_UNSIGNED_BYTE;
		break;

	default:
		CUTE_GL_ASSERT(0);
		return (uint32_t)~0;
	}
}

void gl_do_map_internal(gl_draw_call_t* call, gl_renderable_t* render)
{
	uint32_t count = call->vert_count;
	void* driver_memory = gl_map_internal(render, count);
	memcpy(driver_memory, call->verts, render->data.vertex_stride * count);
	gl_unmap_internal();
}

static void gl_render_internal(gl_draw_call_t* call)
{
	gl_renderable_t* render = call->r;
	uint32_t texture_count = call->texture_count;
	uint32_t* textures = call->textures;

	if (render->data.usage == GL_STATIC_DRAW)
	{
		if (render->need_new_sync)
		{
			render->need_new_sync = 0;
			gl_do_map_internal(call, render);
		}
	}
	else gl_do_map_internal(call, render);

	gl_vertex_data_t* data = &render->data;
	gl_vertex_attribute_t* attributes = data->attributes;
	uint32_t vertexStride = data->vertex_stride;
	uint32_t attributeCount = data->attribute_count;

	gl_set_active_shader(render->program);

	uint32_t bufferNumber = render->buffer_number;
	uint32_t buffer = render->buffers[bufferNumber];
	glBindBuffer(GL_ARRAY_BUFFER, buffer);

	for (uint32_t i = 0; i < attributeCount; ++i)
	{
		gl_vertex_attribute_t* attribute = attributes + i;

		uint32_t location = attribute->location;
		uint32_t size = attribute->size;
		uint32_t type = gl_get_enum(attribute->type);
		uint32_t offset = attribute->offset;

		glEnableVertexAttribArray(location);
		glVertexAttribPointer(location, size, type, GL_FALSE, vertexStride, (void*)((size_t)offset));
	}

	for (uint32_t i = 0; i < texture_count; ++i)
	{
		uint32_t gl_id = textures[i];

		glActiveTexture(GL_TEXTURE0 + i);
		glBindTexture(GL_TEXTURE_2D, gl_id);
	}

	uint32_t streamOffset = render->index0;
	uint32_t streamSize = render->index1 - streamOffset;
	glDrawArrays(data->primitive, streamOffset, streamSize);

	if (render->need_new_sync)
	{
		// @TODO: This shouldn't be called for static buffers, only needed for streaming.
		render->fences[bufferNumber] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		render->need_new_sync = 0;
	}

	for (uint32_t i = 0; i < attributeCount; ++i)
	{
		gl_vertex_attribute_t* attribute = attributes + i;

		uint32_t location = attribute->location;
		glDisableVertexAttribArray(location);
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);
}

void gl_present_internal(void* context, gl_framebuffer_t* fb, int w, int h)
{
	gl_context_t* ctx = (gl_context_t*)context;
	gl_qsort_internal(ctx->calls, ctx->count);

	if (fb)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fb->fb_id);
		glViewport(0, 0, fb->w, fb->h);
	}
	if (ctx->clear_bits) glClear(ctx->clear_bits);
	if (ctx->settings_bits) glEnable(ctx->settings_bits);

	// flush all draw calls to the GPU
	for (uint32_t i = 0; i < ctx->count; ++i)
	{
		gl_draw_call_t* call = ctx->calls + i;
		gl_render_internal(call);
	}

#if CUTE_GL_LINE_RENDERER
	if (ctx->line_vert_count)
	{
		if (ctx->line_depth_test) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		gl_draw_call_t call;
		call.vert_count = ctx->line_vert_count;
		call.verts = ctx->line_verts;
		call.r = &ctx->line_r;
		call.texture_count = 0;
		gl_render_internal(&call);
		ctx->line_vert_count = 0;
	}
#endif

	if (fb)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, w, h);
		glClear(GL_COLOR_BUFFER_BIT);
		glDisable(GL_DEPTH_TEST);

		gl_set_active_shader(fb->shader);
		glBindBuffer(GL_ARRAY_BUFFER, fb->quad_id);
		glBindTexture(GL_TEXTURE_2D, fb->tex_id);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, CUTE_GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, CUTE_GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (GLvoid*)(2 * sizeof(GLfloat)));
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		gl_deactivate_shader();
	}
}

void gl_flush(void* ctx, gl_func_t* swap, gl_framebuffer_t* fb, int w, int h)
{
	gl_present_internal(ctx, fb, w, h);
	gl_context_t* context = (gl_context_t*)ctx;
	context->count = 0;

	swap();
}

int gl_draw_call_tCount(void* ctx)
{
	gl_context_t* context = (gl_context_t*)ctx;
	return context->count;
}

#include <math.h>

void gl_perspective(float* m, float y_fov_radians, float aspect, float n, float f)
{
	float a = 1.0f / (float) tanf(y_fov_radians / 2.0f);

	m[0] = a / aspect;
	m[1] = 0;
	m[2] = 0;
	m[3] = 0;

	m[4] = 0;
	m[5] = a;
	m[6] = 0;
	m[7] = 0;

	m[8] = 0;
	m[9] = 0;
	m[10] = -((f + n) / (f - n));
	m[11] = -1.0f;

	m[12] = 0;
	m[13] = 0;
	m[14] = -((2.0f * f * n) / (f - n));
	m[15] = 0;
}

void gl_ortho_2d(float w, float h, float x, float y, float* m)
{
	float left = -w/2;
	float right = w/2;
	float top = h/2;
	float bottom = -h/2;
	float far_ = 1000.0f;
	float near_ = -1000.0f;

	memset(m, 0, sizeof(float) * 4 * 4);

	// ortho
	m[0] = 2.0f / (right - left);
	m[5] = 2.0f / (top - bottom);
	m[10] = -2.0f / (far_ - near_);
	m[15] = 1.0f;

	// translate
	m[12] = -x;
	m[13] = -y;
}

void gl_copy(float* dst, float* src)
{
	for (int i = 0; i < 16; ++i)
		dst[i] = src[i];
}

void gl_mul(float* a, float* b, float* out)
{
	float c[16];

	c[0 + 0 * 4] = a[0 + 0 * 4] * b[0 + 0 * 4] + a[0 + 1 * 4] * b[1 + 0 * 4] + a[0 + 2 * 4] * b[2 + 0 * 4] + a[0 + 3 * 4] * b[3 + 0 * 4];
	c[0 + 1 * 4] = a[0 + 0 * 4] * b[0 + 1 * 4] + a[0 + 1 * 4] * b[1 + 1 * 4] + a[0 + 2 * 4] * b[2 + 1 * 4] + a[0 + 3 * 4] * b[3 + 1 * 4];
	c[0 + 2 * 4] = a[0 + 0 * 4] * b[0 + 2 * 4] + a[0 + 1 * 4] * b[1 + 2 * 4] + a[0 + 2 * 4] * b[2 + 2 * 4] + a[0 + 3 * 4] * b[3 + 2 * 4];
	c[0 + 3 * 4] = a[0 + 0 * 4] * b[0 + 3 * 4] + a[0 + 1 * 4] * b[1 + 3 * 4] + a[0 + 2 * 4] * b[2 + 3 * 4] + a[0 + 3 * 4] * b[3 + 3 * 4];
	c[1 + 0 * 4] = a[1 + 0 * 4] * b[0 + 0 * 4] + a[1 + 1 * 4] * b[1 + 0 * 4] + a[1 + 2 * 4] * b[2 + 0 * 4] + a[1 + 3 * 4] * b[3 + 0 * 4];
	c[1 + 1 * 4] = a[1 + 0 * 4] * b[0 + 1 * 4] + a[1 + 1 * 4] * b[1 + 1 * 4] + a[1 + 2 * 4] * b[2 + 1 * 4] + a[1 + 3 * 4] * b[3 + 1 * 4];
	c[1 + 2 * 4] = a[1 + 0 * 4] * b[0 + 2 * 4] + a[1 + 1 * 4] * b[1 + 2 * 4] + a[1 + 2 * 4] * b[2 + 2 * 4] + a[1 + 3 * 4] * b[3 + 2 * 4];
	c[1 + 3 * 4] = a[1 + 0 * 4] * b[0 + 3 * 4] + a[1 + 1 * 4] * b[1 + 3 * 4] + a[1 + 2 * 4] * b[2 + 3 * 4] + a[1 + 3 * 4] * b[3 + 3 * 4];
	c[2 + 0 * 4] = a[2 + 0 * 4] * b[0 + 0 * 4] + a[2 + 1 * 4] * b[1 + 0 * 4] + a[2 + 2 * 4] * b[2 + 0 * 4] + a[2 + 3 * 4] * b[3 + 0 * 4];
	c[2 + 1 * 4] = a[2 + 0 * 4] * b[0 + 1 * 4] + a[2 + 1 * 4] * b[1 + 1 * 4] + a[2 + 2 * 4] * b[2 + 1 * 4] + a[2 + 3 * 4] * b[3 + 1 * 4];
	c[2 + 2 * 4] = a[2 + 0 * 4] * b[0 + 2 * 4] + a[2 + 1 * 4] * b[1 + 2 * 4] + a[2 + 2 * 4] * b[2 + 2 * 4] + a[2 + 3 * 4] * b[3 + 2 * 4];
	c[2 + 3 * 4] = a[2 + 0 * 4] * b[0 + 3 * 4] + a[2 + 1 * 4] * b[1 + 3 * 4] + a[2 + 2 * 4] * b[2 + 3 * 4] + a[2 + 3 * 4] * b[3 + 3 * 4];
	c[3 + 0 * 4] = a[3 + 0 * 4] * b[0 + 0 * 4] + a[3 + 1 * 4] * b[1 + 0 * 4] + a[3 + 2 * 4] * b[2 + 0 * 4] + a[3 + 3 * 4] * b[3 + 0 * 4];
	c[3 + 1 * 4] = a[3 + 0 * 4] * b[0 + 1 * 4] + a[3 + 1 * 4] * b[1 + 1 * 4] + a[3 + 2 * 4] * b[2 + 1 * 4] + a[3 + 3 * 4] * b[3 + 1 * 4];
	c[3 + 2 * 4] = a[3 + 0 * 4] * b[0 + 2 * 4] + a[3 + 1 * 4] * b[1 + 2 * 4] + a[3 + 2 * 4] * b[2 + 2 * 4] + a[3 + 3 * 4] * b[3 + 2 * 4];
	c[3 + 3 * 4] = a[3 + 0 * 4] * b[0 + 3 * 4] + a[3 + 1 * 4] * b[1 + 3 * 4] + a[3 + 2 * 4] * b[2 + 3 * 4] + a[3 + 3 * 4] * b[3 + 3 * 4];

	gl_copy(out, c);
}

void gl_mulv(float* a, float* b)
{
	float result[4];

	result[0] = a[0] * b[0] + a[4] * b[1] + a[8] * b[2] + a[12] * b[3];
	result[1] = a[1] * b[0] + a[5] * b[1] + a[9] * b[2] + a[13] * b[3];
	result[2] = a[2] * b[0] + a[6] * b[1] + a[10] * b[2] + a[14] * b[3];
	result[3] = a[3] * b[0] + a[7] * b[1] + a[11] * b[2] + a[15] * b[3];

	b[0] = result[0];
	b[1] = result[1];
	b[2] = result[2];
	b[3] = result[3];
}

void gl_identity(float* m)
{
	memset(m, 0, sizeof(float) * 16);
	m[0] = 1.0f;
	m[5] = 1.0f;
	m[10] = 1.0f;
	m[15] = 1.0f;
}

#if CUTE_GL_DEBUG_CHECKS
const char* gl_get_error_string(GLenum error_code)
{
	switch (error_code)
	{
	case GL_NO_ERROR: return "GL_NO_ERROR";
	case GL_INVALID_ENUM: return "GL_INVALID_ENUM";
	case GL_INVALID_VALUE: return "GL_INVALID_VALUE";
	case GL_INVALID_OPERATION: return "GL_INVALID_OPERATION";
	case GL_OUT_OF_MEMORY: return "GL_OUT_OF_MEMORY";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "GL_INVALID_FRAMEBUFFER_OPERATION";
	default: return "UNKNOWN_ERROR";
	}
}

const char* gl_get_error_description(GLenum error_code)
{
	switch (error_code)
	{
	case GL_NO_ERROR: return "No error detected.";
	case GL_INVALID_ENUM: return "Enum argument out of range.";
	case GL_INVALID_VALUE: return "Numeric argument out of range.";
	case GL_INVALID_OPERATION: return "Operation illegal in current state.";
	case GL_OUT_OF_MEMORY: return "Not enough memory left to execute command.";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "Framebuffer object is not complete.";
	default: return "No description available for UNKNOWN_ERROR.";
	}
}

void gl_print_errors_internal(char* file, uint32_t line)
{
	GLenum code = glGetError();

	if (code != GL_NO_ERROR)
	{
		char* last_slash = file;

		while (*file)
		{
			char c = *file;
			if (c == '\\' || c == '/')
				last_slash = file + 1;
			++file;
		}

		const char* str = gl_get_error_string(code);
		const char* des = gl_get_error_description(code);
		CUTE_GL_WARN("OpenGL Error %s (%u): %u, %s, %s: \n", last_slash, line, code, str, des);
	}
}
#endif

#endif // CUTE_GL_IMPLEMENTATION_ONCE
#endif // CUTE_GL_IMPLEMENTATION

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
