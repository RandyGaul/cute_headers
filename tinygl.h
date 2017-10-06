#if !defined( TINYGL_H )

/*
	tinygl - v1.01

	Summary:
	Wrapper for OpenGL for vertex attributes, shader creation, draw calls, and
	post-processing fx. The API was carefully designed to facilitate trying out
	one-off techqniques or experiments, of which are critical for certain kinds
	of development styles (i.e. fast iteration). Credit goes to thatgamecompany
	for the original designs + idea of this API style.

	To create implementation (the function definitions)
		#define TINYGL_IMPL
	in *one* C/CPP file (translation unit) that includes this file
*/

/*
	DOCUMENTATION (quick intro):
	1. create context
	2. define vertices
	3. load shader
	4. create renderable
	5. compose draw calls
	6. flush

	1. void* ctx = tgMakeCtx( max_draw_calls_per_flush, clear_bits, settings_bits )
	2. tgMakeVertexData( ... ) and tgAddAttribute( ... )
	3. tgLoadShader( &shader_instance, vertex_shader_string, pixel_shader_string )
	4. tgMakeRenderable( &renderable_instance, &vertex_data_instance )
	5. tgPushDrawCall( ctx, draw_call )
	6. tgFlush( ctx, swap_buffer_func, use_post_fx ? &frame_buffer : 0 )

	DETAILS:
	tinygl only renders triangles. Triangles can be fed to draw calls with static
	or dynamic geometry. Dynamic geometry is sent to the GPU upon each draw call,
	while static geometry is sent only upon the first draw call. Dynamic vertices
	are triple buffered using fence syncs for optimal performance and screen tear-
	ing avoidance.

	tinygl does come with a debug line renderer built in! Can be turned off at
	compile time if not used (see TG_LINE_RENDERER). This is perfect for creating
	a global line rendering utility for debugging purposes.

	Post processing fx are done with a frame buffer, and are done only once with
	one shader, after all draw calls have been processed.

	For full examples of use please visit either of these links:
		example to render various 2d shapes + post fx
			https://github.com/RandyGaul/tinyheaders/tree/master/examples_tinygl_and_tinyc2

		example of rendering 3d geometry + post fx, a full game from a game jam (windows only)
			https://github.com/RandyGaul/pook
*/

/*
	Revision history:
		1.0  (02/09/2017) initial release
		1.01 (03/23/2017) memory leak fix and swap to realloc for tgLine stuff
*/

/*
	Contributors:
		Andrew Hung       1.0  - initial frame buffer implementation, various bug fixes
		to-miz            1.01 - memory leak fix and swap to realloc for tgLine stuff
*/

/*
	Some Current Limitations
		* No index support. Adding indices would not be too hard and come down to a
			matter of adding in some more triple buffer/single buffer code for
			GL_STATIC_DRAW vs GL_DYNAMIC_DRAW.
		* GL 3.0+ support only
		* Full support for array uniforms is not quite tested and hammered out.
*/

#include <stdint.h>

// recommended to leave this on as long as possible (perhaps until release)
#define TG_DEBUG_CHECKS 1

// feel free to turn this off if unused.
// This is used to render lines on-top of all other rendering (besides post fx); rendered with no depth testing.
#define TG_LINE_RENDERER 1

enum
{
	TG_FLOAT,
	TG_INT,
	TG_BOOL,
	TG_SAMPLER,
	TG_UNKNOWN,
};

typedef struct
{
	const char* name;
	uint32_t hash;
	uint32_t size;
	uint32_t type;
	uint32_t offset;
	uint32_t location;
} tgVertexAttribute;

#define TG_ATTRIBUTE_MAX_COUNT 16
typedef struct
{
	uint32_t buffer_size;
	uint32_t vertex_stride;
	uint32_t primitive;
	uint32_t usage;

	uint32_t attribute_count;
	tgVertexAttribute attributes[ TG_ATTRIBUTE_MAX_COUNT ];
} tgVertexData;

// adjust this as necessary to create your own draw call ordering
// see: http://realtimecollisiondetection.net/blog/?p=86
typedef struct
{
	union
	{
		struct
		{
			uint64_t fullscreen    : 2;
			uint64_t hud           : 5;
			uint64_t depth         : 25;
			uint64_t translucency  : 32;
		};

		uint64_t key;
	};
} tgRenderState;

struct tgShader;
typedef struct tgShader tgShader;

typedef struct
{
	tgVertexData data;
	tgShader* program;
	tgRenderState state;
	uint32_t attribute_count;

	uint32_t index0;
	uint32_t index1;
	uint32_t buffer_number;
	uint32_t need_new_sync;
	uint32_t buffer_count;
	uint32_t buffers[ 3 ];
	GLsync fences[ 3 ];
} tgRenderable;

#define TG_UNIFORM_NAME_LENGTH 64
#define TG_UNIFORM_MAX_COUNT 16

typedef struct
{
	char name[ TG_UNIFORM_NAME_LENGTH ];
	uint32_t id;
	uint32_t hash;
	uint32_t size;
	uint32_t type;
	uint32_t location;
} tgUniform;

struct tgShader
{
	uint32_t program;
	uint32_t uniform_count;
	tgUniform uniforms[ TG_UNIFORM_MAX_COUNT ];
};

typedef struct
{
	uint32_t fb_id;
	uint32_t tex_id;
	uint32_t rb_id;
	uint32_t quad_id;
	tgShader* shader;
} tgFramebuffer;

typedef struct
{
	uint32_t vert_count;
	void* verts;
	tgRenderable* r;
	uint32_t texture_count;
	uint32_t textures[ 8 ];
} tgDrawCall;

struct tgContext;
typedef struct tgContext tgContext;

tgContext* tgMakeCtx( uint32_t max_draw_calls, uint32_t clear_bits, uint32_t settings_bits );
void tgFreeCtx( void* ctx );

void tgLineMVP( void* context, float* mvp );
void tgLineColor( void* context, float r, float g, float b );
void tgLine( void* context, float ax, float ay, float az, float bx, float by, float bz );
void tgLineWidth( float width );
void tgLineDepthTest( void* context, int zero_for_off );

void tgMakeFramebuffer( tgFramebuffer* tgFbo, tgShader* shader, int w, int h );
void tgFreeFramebuffer( tgFramebuffer* tgFbo );

void tgMakeVertexData( tgVertexData* vd, uint32_t buffer_size, uint32_t primitive, uint32_t vertex_stride, uint32_t usage );
void tgAddAttribute( tgVertexData* vd, char* name, uint32_t size, uint32_t type, uint32_t offset );
void tgMakeRenderable( tgRenderable* r, tgVertexData* vd );

// Must be called after tgMakeRenderable
void tgSetShader( tgRenderable* r, tgShader* s );
void tgLoadShader( tgShader* s, const char* vertex, const char* pixel );
void tgFreeShader( tgShader* s );

void tgSetActiveShader( tgShader* s );
void tgDeactivateShader( );
void tgSendF32( tgShader* s, char* uniform_name, uint32_t size, float* floats, uint32_t count );
void tgSendMatrix( tgShader* s, char* uniform_name, float* floats );
void tgSendTexture( tgShader* s, char* uniform_name, uint32_t index );

void tgPushDrawCall( void* ctx, tgDrawCall call );

typedef void (*tgFunc)( );
void tgFlush( void* ctx, tgFunc swap, tgFramebuffer* fb );

void tgOrtho2D( float w, float h, float x, float y, float* m );
void tgPerspective( float* m, float y_fov_radians, float aspect, float n, float f );
void tgMul( float* a, float* b, float* out ); // perform a * b where a and b are 4x4 matrices, stores result in out
void tgIdentity( float* m );

#if TG_DEBUG_CHECKS

	#define TG_PRINT_GL_ERRORS( ) tgPrintGLErrors_internal( __FILE__, __LINE__ )
	void tgPrintGLErrors_internal( char* file, uint32_t line );

#endif

#define TINYGL_H
#endif

#ifdef TINYGL_IMPL

#define TG_OFFSET_OF( type, member ) ((uint32_t)((size_t)(&((type*)0)->member)))

#if TG_DEBUG_CHECKS

	#include <stdio.h>
	#include <assert.h>
	#define TG_ASSERT assert
	#define TG_WARN printf

#else

	#define TG_ASSERT( ... )
	#define TG_WARN( ... )

#endif

struct tgContext
{
	uint32_t clear_bits;
	uint32_t settings_bits;
	uint32_t max_draw_calls;
	uint32_t count;
	tgDrawCall* calls;

#if TG_LINE_RENDERER
	tgRenderable line_r;
	tgShader line_s;
	uint32_t line_vert_count;
	uint32_t line_vert_capacity;
	float* line_verts;
	float r, g, b;
	int line_depth_test;
#endif
};

#include <stdlib.h> // malloc, free, NULL
#include <string.h> // memset

tgContext* tgMakeCtx( uint32_t max_draw_calls, uint32_t clear_bits, uint32_t settings_bits )
{
	tgContext* ctx = (tgContext*)malloc( sizeof( tgContext ) );
	ctx->clear_bits = clear_bits;
	ctx->settings_bits = settings_bits;
	ctx->max_draw_calls = max_draw_calls;
	ctx->count = 0;
	ctx->calls = (tgDrawCall*)malloc( sizeof( tgDrawCall ) * max_draw_calls );
	if ( !ctx->calls )
	{
		free( ctx );
		return 0;
	}
	GLuint vao;
	glGenVertexArrays( 1, &vao );
	glBindVertexArray( vao );

#if TG_LINE_RENDERER
	#define TG_LINE_STRIDE (sizeof( float ) * 3 * 2)
	tgVertexData vd;
	tgMakeVertexData( &vd, 1024 * 1024, GL_LINES, TG_LINE_STRIDE, GL_DYNAMIC_DRAW );
	tgAddAttribute( &vd, "in_pos", 3, TG_FLOAT, 0 );
	tgAddAttribute( &vd, "in_col", 3, TG_FLOAT, TG_LINE_STRIDE / 2 );
	tgMakeRenderable( &ctx->line_r, &vd );
	const char* vs = "#version 410\nuniform mat4 u_mvp;in vec3 in_pos;in vec3 in_col;out vec3 v_col;void main( ){v_col = in_col;gl_Position = u_mvp * vec4( in_pos, 1 );}";
	char* ps = "#version 410\nin vec3 v_col;out vec4 out_col;void main( ){out_col = vec4( v_col, 1 );}";
	tgLoadShader( &ctx->line_s, vs, ps );
	tgSetShader( &ctx->line_r, &ctx->line_s );
	tgLineColor( ctx, 1.0f, 1.0f, 1.0f );
	ctx->line_vert_count = 0;
	ctx->line_vert_capacity = 1024 * 1024;
	ctx->line_verts = (float*)malloc( TG_LINE_STRIDE * ctx->line_vert_capacity );
	ctx->line_depth_test = 0;
#endif

	return ctx;
}

void tgFreeCtx( void* ctx )
{
	tgContext* context = (tgContext*)ctx;
	free( context->calls );
#if TG_LINE_RENDERER
	free( context->line_verts );
#endif
	free( context );
}

#if TG_LINE_RENDERER
void tgLineMVP( void* context, float* mvp )
{
	tgContext* ctx = (tgContext*)context;
	tgSendMatrix( &ctx->line_s, "u_mvp", mvp );
}

void tgLineColor( void* context, float r, float g, float b )
{
	tgContext* ctx = (tgContext*)context;
	ctx->r = r; ctx->g = g; ctx->b = b;
}

void tgLine( void* context, float ax, float ay, float az, float bx, float by, float bz )
{
	tgContext* ctx = (tgContext*)context;
	if ( ctx->line_vert_count + 2 > ctx->line_vert_capacity )
	{
		ctx->line_vert_capacity *= 2;
		ctx->line_verts = (float*)realloc(ctx->line_verts, TG_LINE_STRIDE * ctx->line_vert_capacity);
	}
	float verts[] = { ax, ay, az, ctx->r, ctx->g, ctx->b, bx, by, bz, ctx->r, ctx->g, ctx->b };
	memcpy( ctx->line_verts + ctx->line_vert_count * (TG_LINE_STRIDE / sizeof( float )), verts, sizeof( verts ) );
	ctx->line_vert_count += 2;
}

void tgLineWidth( float width )
{
	glLineWidth( width );
	TG_PRINT_GL_ERRORS( ); // common errors here unfortunately
}

void tgLineDepthTest( void* context, int zero_for_off )
{
	tgContext* ctx = (tgContext*)context;
	ctx->line_depth_test = zero_for_off;
}
#else
#define TG_UNUSED( x ) (void)(x);
void tgLineMVP( void* context, float* mvp ) { TG_UNUSED( context ); TG_UNUSED( mvp ); }
void tgLineColor( void* context, float r, float g, float b ) { TG_UNUSED( context ); TG_UNUSED( r ); TG_UNUSED( g ); TG_UNUSED( b ); }
void tgLine( void* context, float ax, float ay, float az, float bx, float by, float bz )
{ TG_UNUSED( context ); TG_UNUSED( ax ); TG_UNUSED( ay ); TG_UNUSED( az ); TG_UNUSED( bx ); TG_UNUSED( by ); TG_UNUSED( bz ); }
void tgLineWidth( float width ) { TG_UNUSED( width ); }
void tgLineDepthTest( void* context, int zero_for_off ) { TG_UNUSED( context ); TG_UNUSED( zero_for_off ); }
#endif

void tgMakeFramebuffer( tgFramebuffer* fb, tgShader* shader, int w, int h )
{
	// Generate the frame buffer
	GLuint fb_id;
	glGenFramebuffers( 1, &fb_id );
	glBindFramebuffer( GL_FRAMEBUFFER, fb_id );

	// Generate a texture to use as the color buffer.
	GLuint tex_id;
	glGenTextures( 1, &tex_id );
	glBindTexture( GL_TEXTURE_2D, tex_id );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glBindTexture( GL_TEXTURE_2D, 0 );

	// Attach color buffer to frame buffer
	glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex_id, 0 );

	// Generate depth and stencil attachments for the fbo using a RenderBuffer.
	GLuint rb_id;
	glGenRenderbuffers( 1, &rb_id );
	glBindRenderbuffer( GL_RENDERBUFFER, rb_id );
	glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h );
	glBindRenderbuffer( GL_RENDERBUFFER, 0 );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rb_id );

	if ( glCheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE )
		TG_WARN( "failed to generate framebuffer\n" );

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	// Prepare quad
	GLuint quad_id;
	glGenBuffers( 1, &quad_id );
	glBindBuffer( GL_ARRAY_BUFFER, quad_id );
	static GLfloat quad[] = {
		-1.0f,  1.0f,  0.0f, 1.0f,
		-1.0f, -1.0f,  0.0f, 0.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,

		-1.0f,  1.0f,  0.0f, 1.0f,
		 1.0f, -1.0f,  1.0f, 0.0f,
		 1.0f,  1.0f,  1.0f, 1.0f
	};
	glBufferData( GL_ARRAY_BUFFER, sizeof( quad ), quad, GL_STATIC_DRAW );
	glBindBuffer( GL_ARRAY_BUFFER, 0 );

	fb->fb_id = fb_id;
	fb->tex_id = tex_id;
	fb->rb_id = rb_id;
	fb->quad_id = quad_id;
	fb->shader = shader;
}

void tgFreeFramebuffer( tgFramebuffer* fb )
{
	glDeleteTextures( 1, &fb->tex_id );
	glDeleteRenderbuffers( 1, &fb->rb_id );
	glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	glDeleteFramebuffers( 1, &fb->fb_id );
	glDeleteBuffers( 1, &fb->quad_id );
	memset( fb, 0, sizeof( tgFramebuffer ) );
}

static uint32_t tg_djb2( unsigned char* str )
{
	uint32_t hash = 5381;
	int c;

	while ( (c = *str++) )
		hash = ((hash << 5) + hash) + c;

	return hash;
}

void tgMakeVertexData( tgVertexData* vd, uint32_t buffer_size, uint32_t primitive, uint32_t vertex_stride, uint32_t usage )
{
	vd->buffer_size = buffer_size;
	vd->vertex_stride = vertex_stride;
	vd->primitive = primitive;
	vd->usage = usage;
	vd->attribute_count = 0;
}

static uint32_t tgGetGLType( uint32_t type )
{
	switch ( type )
	{
	case GL_INT:
	case GL_INT_VEC2:
	case GL_INT_VEC3:
	case GL_INT_VEC4:
		return TG_INT;

	case GL_FLOAT:
	case GL_FLOAT_VEC2:
	case GL_FLOAT_VEC3:
	case GL_FLOAT_VEC4:
	case GL_FLOAT_MAT2:
	case GL_FLOAT_MAT3:
	case GL_FLOAT_MAT4:
		return TG_FLOAT;

	case GL_BOOL:
	case GL_BOOL_VEC2:
	case GL_BOOL_VEC3:
	case GL_BOOL_VEC4:
		return TG_BOOL;

	case GL_SAMPLER_1D:
	case GL_SAMPLER_2D:
	case GL_SAMPLER_3D:
		return TG_SAMPLER;

	default:
		return TG_UNKNOWN;
	}
}

void tgAddAttribute( tgVertexData* vd, char* name, uint32_t size, uint32_t type, uint32_t offset )
{
	tgVertexAttribute va;
	va.name = name;
	va.hash = tg_djb2( (unsigned char*)name );
	va.size = size;
	va.type = type;
	va.offset = offset;

	TG_ASSERT( vd->attribute_count < TG_ATTRIBUTE_MAX_COUNT );
	vd->attributes[ vd->attribute_count++ ] = va;
}

void tgMakeRenderable( tgRenderable* r, tgVertexData* vd )
{
	r->data = *vd;
	r->index0 = 0;
	r->index1 = 0;
	r->buffer_number = 0;
	r->need_new_sync = 0;
	r->program = 0;
	r->state.key = 0;;

	if ( vd->usage == GL_STATIC_DRAW )
	{
		r->buffer_count = 1;
		r->need_new_sync = 1;
	}
	else r->buffer_count = 3;
}

// WARNING: Messes with GL global state via glUnmapBuffer( GL_ARRAY_BUFFER ) and
// glBindBuffer( GL_ARRAY_BUFFER, ... ), so call tgMap, fill in data, then call tgUnmap.
void* tgMap( tgRenderable* r, uint32_t count )
{
	// Cannot map a buffer when the buffer is too small
	// Make your buffer is bigger or draw less data
	TG_ASSERT( count <= r->data.buffer_size );

	uint32_t newIndex = r->index1 + count;

	if ( newIndex > r->data.buffer_size )
	{
		// should never overflow a static buffer
		TG_ASSERT( r->data.usage != GL_STATIC_DRAW );

		++r->buffer_number;
		r->buffer_number %= r->buffer_count;
		GLsync fence = r->fences[ r->buffer_number ];

		// Ensure buffer is not in use by GPU
		// If we stall here we are GPU bound
		GLenum result = glClientWaitSync( fence, 0, (GLuint64)1000000000 );
		TG_ASSERT( result != GL_TIMEOUT_EXPIRED );
		TG_ASSERT( result != GL_WAIT_FAILED );
		glDeleteSync( fence );

		r->index0 = 0;
		r->index1 = count;
		r->need_new_sync = 1;
	}

	else
	{
		r->index0 = r->index1;
		r->index1 = newIndex;
	}

	glBindBuffer( GL_ARRAY_BUFFER, r->buffers[ r->buffer_number ] );
	uint32_t stream_size = (r->index1 - r->index0) * r->data.vertex_stride;
	void* memory = glMapBufferRange( GL_ARRAY_BUFFER, r->index0 * r->data.vertex_stride, stream_size, GL_MAP_WRITE_BIT | GL_MAP_UNSYNCHRONIZED_BIT );

#if TG_DEBUG_CHECKS
	if ( !memory )
	{
		TG_WARN( "\n%u\n", glGetError( ) );
		TG_ASSERT( memory );
	}
#endif

	return memory;
}

void tgUnmap( )
{
	glUnmapBuffer( GL_ARRAY_BUFFER );
}

void tgSetShader( tgRenderable* r, tgShader* program )
{
	// Cannot set the shader of a Renderable more than once
	TG_ASSERT( !r->program );

	r->program = program;
	glGetProgramiv( program->program, GL_ACTIVE_ATTRIBUTES, (GLint*)&r->attribute_count );

#if TG_DEBUG_CHECKS
	if ( r->attribute_count != r->data.attribute_count )
	{
		TG_WARN( "Mismatch between VertexData attribute count (%d), and shader attribute count (%d).\n",
				r->attribute_count,
				r->data.attribute_count );
	}
#endif

	uint32_t size;
	uint32_t type;
	char buffer[ 256 ];
	uint32_t hash;

	// Query and set all attribute locations as defined by the shader linking
	for ( uint32_t i = 0; i < r->attribute_count; ++i )
	{
		glGetActiveAttrib( program->program, i, 256, 0, (GLint*)&size, (GLenum*)&type, buffer );
		hash = tg_djb2( (unsigned char*)buffer );
		type = tgGetGLType( type );

#if TG_DEBUG_CHECKS
		tgVertexAttribute* a = 0;

		// Make sure data.AddAttribute( name, ... ) has matching named attribute
		// Also make sure the GL::Type matches
		// This helps to catch common mismatch errors between glsl and C++
		for ( uint32_t j = 0; j < r->data.attribute_count; ++j )
		{
			tgVertexAttribute* b = r->data.attributes + j;

			if ( b->hash == hash )
			{
				a = b;
				break;
			}
		}
#endif

		// Make sure the user did not have a mismatch between VertexData
		// attributes and the attributes defined in the vertex shader
		TG_ASSERT( a );
		TG_ASSERT( a->type == type );

		a->location = glGetAttribLocation( program->program, buffer );
	}

	// Generate VBOs and initialize fences
	uint32_t usage = r->data.usage;

	for ( uint32_t i = 0; i < r->buffer_count; ++i )
	{
		GLuint* buffer = (GLuint*)r->buffers + i;

		glGenBuffers( 1, buffer );
		glBindBuffer( GL_ARRAY_BUFFER, *buffer );
		glBufferData( GL_ARRAY_BUFFER, r->data.buffer_size * r->data.vertex_stride, NULL, usage );
		r->fences[ i ] = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
	}

	glBindBuffer( GL_ARRAY_BUFFER, 0 );
}

GLuint tgCompileShader( const char* Shader, uint32_t type )
{
	GLuint handle = glCreateShader( type );
	glShaderSource( handle, 1, (const GLchar**)&Shader, NULL );
	glCompileShader( handle );

	uint32_t compiled;
	glGetShaderiv( handle, GL_COMPILE_STATUS, (GLint*)&compiled );

#if TG_DEBUG_CHECKS
	if ( !compiled )
	{
		TG_WARN( "Shader of type %d failed compilation.\n", type );
		char out[ 2000 ];
		GLsizei outLen;
		glGetShaderInfoLog( handle, 2000, &outLen, out );
		TG_WARN( "%s\n", out );
		TG_ASSERT( 0 );
	}
#endif

	return handle;
}

void tgLoadShader( tgShader* s, const char* vertex, const char* pixel )
{
	// Compile vertex and pixel Shader
	uint32_t program = glCreateProgram( );
	uint32_t vs = tgCompileShader( vertex, GL_VERTEX_SHADER );
	uint32_t ps = tgCompileShader( pixel, GL_FRAGMENT_SHADER );
	glAttachShader( program, vs );
	glAttachShader( program, ps );

	// Link the Shader to form a program
	glLinkProgram( program );

	uint32_t linked;
	glGetProgramiv( program, GL_LINK_STATUS, (GLint*)&linked );

#if TG_DEBUG_CHECKS
	if ( !linked )
	{
		TG_WARN( "Shaders failed to link.\n" );
		char out[ 2000 ];
		GLsizei outLen;
		glGetProgramInfoLog( program, 2000, &outLen, out );
		TG_WARN( "%s\n", out );
		TG_ASSERT( 0 );
	}
#endif

	glDetachShader( program, vs );
	glDetachShader( program, ps );
	glDeleteShader( vs );
	glDeleteShader( ps );

	// Insert Shader into the Shaders array for future lookups
	s->program = program;

	// Query Uniform information and fill out the Shader Uniforms
	GLint uniform_count;
	uint32_t nameSize = sizeof( char ) * TG_UNIFORM_NAME_LENGTH;
	glGetProgramiv( program, GL_ACTIVE_UNIFORMS, &uniform_count );
	TG_ASSERT( uniform_count < TG_UNIFORM_MAX_COUNT );
	s->uniform_count = uniform_count;

	for ( uint32_t i = 0; i < (uint32_t)uniform_count; ++i )
	{
		uint32_t nameLength;
		tgUniform u;

		glGetActiveUniform( program, (GLint)i, nameSize, (GLsizei*)&nameLength, (GLsizei*)&u.size, (GLenum*)&u.type, u.name );

		// Uniform named in a Shader is too long for the UNIFORM_NAME_LENGTH constant
		TG_ASSERT( nameLength <= TG_UNIFORM_NAME_LENGTH );

		u.location = glGetUniformLocation( program, u.name );
		u.type = tgGetGLType( u.type );
		u.hash = tg_djb2( (unsigned char*)u.name );
		u.id = i;

		// @TODO: Perhaps need to handle appended [0] to Uniform names?

		s->uniforms[ i ] = u;
	}

#if TG_DEBUG_CHECKS
	// prevent hash collisions
	for ( uint32_t i = 0; i < (uint32_t)uniform_count; ++i )
		for ( uint32_t j = i + 1; j < (uint32_t)uniform_count; ++j )
			TG_ASSERT( s->uniforms[ i ].hash != s->uniforms[ j ].hash );
#endif
}

void tgFreeShader( tgShader* s )
{
	glDeleteProgram( s->program );
	memset( s, 0, sizeof( tgShader ) );
}

tgUniform* tgFindUniform( tgShader* s, char* name )
{
	uint32_t uniform_count = s->uniform_count;
	tgUniform* uniforms = s->uniforms;
	uint32_t hash = tg_djb2( (unsigned char*)name );

	for ( uint32_t i = 0; i < uniform_count; ++i )
	{
		tgUniform* u = uniforms + i;

		if ( u->hash == hash )
		{
			return u;
		}
	}

	return 0;
}

void tgSetActiveShader( tgShader* s )
{
	glUseProgram( s->program );
}

void tgDeactivateShader( )
{
	glUseProgram( 0 );
}

void tgSendF32( tgShader* s, char* uniform_name, uint32_t size, float* floats, uint32_t count )
{
	tgUniform* u = tgFindUniform( s, uniform_name );

	if ( !u )
	{
		TG_WARN( "Unable to find uniform: %s\n", uniform_name );
		return;
	}

	TG_ASSERT( size == u->size );
	TG_ASSERT( u->type == TG_FLOAT );

	tgSetActiveShader( s );
	switch ( count )
	{
	case 1:
		glUniform1f( u->location, floats[ 0 ] );
		break;

	case 2:
		glUniform2f( u->location, floats[ 0 ], floats[ 1 ] );
		break;

	case 3:
		glUniform3f( u->location, floats[ 0 ], floats[ 1 ], floats[ 2 ] );
		break;

	case 4:
		glUniform4f( u->location, floats[ 0 ], floats[ 1 ], floats[ 2 ], floats[ 3 ] );
		break;

	default:
		TG_ASSERT( 0 );
		break;
	}
	tgDeactivateShader( );
}

void tgSendMatrix( tgShader* s, char* uniform_name, float* floats )
{
	tgUniform* u = tgFindUniform( s, uniform_name );

	if ( !u )
	{
		TG_WARN( "Unable to find uniform: %s\n", uniform_name );
		return;
	}

	TG_ASSERT( u->size == 1 );
	TG_ASSERT( u->type == TG_FLOAT );

	tgSetActiveShader( s );
	glUniformMatrix4fv( u->id, 1, 0, floats );
	tgDeactivateShader( );
}

void tgSendTexture( tgShader* s, char* uniform_name, uint32_t index )
{
	tgUniform* u = tgFindUniform( s, uniform_name );

	if ( !u )
	{
		TG_WARN( "Unable to find uniform: %s\n", uniform_name );
		return;
	}

	TG_ASSERT( u->type == TG_SAMPLER );

	tgSetActiveShader( s );
	glUniform1i( u->location, index );
	tgDeactivateShader( );
}

static uint32_t tgCallSortPred( tgDrawCall* a, tgDrawCall* b )
{
	return a->r->state.key < b->r->state.key;
}

static void tgQSort( tgDrawCall* items, uint32_t count )
{
	if ( count <= 1 ) return;

	tgDrawCall pivot = items[ count - 1 ];
	uint32_t low = 0;
	for ( uint32_t i = 0; i < count - 1; ++i )
	{
		if ( tgCallSortPred( items + i, &pivot ) )
		{
			tgDrawCall tmp = items[ i ];
			items[ i ] = items[ low ];
			items[ low ] = tmp;
			low++;
		}
	}

	items[ count - 1 ] = items[ low ];
	items[ low ] = pivot;
	tgQSort( items, low );
	tgQSort( items + low + 1, count - 1 - low );
}

void tgPushDrawCall( void* ctx, tgDrawCall call )
{
	tgContext* context = (tgContext*)ctx;
	TG_ASSERT( context->count < context->max_draw_calls );
	context->calls[ context->count++ ] = call;
}

uint32_t tgGetGLEnum( uint32_t type )
{
	switch ( type )
	{
	case TG_FLOAT:
		return GL_FLOAT;
		break;

	case TG_INT:
		return GL_UNSIGNED_BYTE;
		break;

	default:
		TG_ASSERT( 0 );
		return ~0;
	}
}

void tgDoMap( tgDrawCall* call, tgRenderable* render )
{
	uint32_t count = call->vert_count;
	void* driver_memory = tgMap( render, count );
	memcpy( driver_memory, call->verts, render->data.vertex_stride * count );
	tgUnmap( );
}

static void tgRender( tgContext* ctx, tgDrawCall* call )
{
	tgRenderable* render = call->r;
	uint32_t texture_count = call->texture_count;
	uint32_t* textures = call->textures;

	if ( render->data.usage == GL_STATIC_DRAW )
	{
		if ( render->need_new_sync )
		{
			render->need_new_sync = 0;
			tgDoMap( call, render );
		}
	}
	else tgDoMap( call, render );

	tgVertexData* data = &render->data;
	tgVertexAttribute* attributes = data->attributes;
	uint32_t vertexStride = data->vertex_stride;
	uint32_t attributeCount = data->attribute_count;

	tgSetActiveShader( render->program );

	uint32_t bufferNumber = render->buffer_number;
	uint32_t buffer = render->buffers[ bufferNumber ];
	glBindBuffer( GL_ARRAY_BUFFER, buffer );

	for ( uint32_t i = 0; i < attributeCount; ++i )
	{
		tgVertexAttribute* attribute = attributes + i;

		uint32_t location = attribute->location;
		uint32_t size = attribute->size;
		uint32_t type = tgGetGLEnum( attribute->type );
		uint32_t offset = attribute->offset;

		glEnableVertexAttribArray( location );
		glVertexAttribPointer( location, size, type, GL_FALSE, vertexStride, (void*)((size_t)offset) );
	}

	for ( uint32_t i = 0; i < texture_count; ++i )
	{
		uint32_t gl_id = textures[ i ];

		glActiveTexture( GL_TEXTURE0 + i );
		glBindTexture( GL_TEXTURE_2D, gl_id );
	}

	uint32_t streamOffset = render->index0;
	uint32_t streamSize = render->index1 - streamOffset;
	glDrawArrays( data->primitive, streamOffset, streamSize );

	if ( render->need_new_sync )
	{
		// @TODO: This shouldn't be called for static buffers, only needed for streaming.
		render->fences[ bufferNumber ] = glFenceSync( GL_SYNC_GPU_COMMANDS_COMPLETE, 0 );
		render->need_new_sync = 0;
	}

	for ( uint32_t i = 0; i < attributeCount; ++i )
	{
		tgVertexAttribute* attribute = attributes + i;

		uint32_t location = attribute->location;
		glDisableVertexAttribArray( location );
	}

	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	glUseProgram( 0 );
}

void tgPresent( void* context, tgFramebuffer* fb )
{
	tgContext* ctx = (tgContext*)context;
	tgQSort( ctx->calls, ctx->count );

	if ( fb ) glBindFramebuffer( GL_FRAMEBUFFER, fb->fb_id );
	if ( ctx->clear_bits ) glClear( ctx->clear_bits );
	if ( ctx->settings_bits ) glEnable( ctx->settings_bits );

	// flush all draw calls to the GPU
	for ( uint32_t i = 0; i < ctx->count; ++i )
	{
		tgDrawCall* call = ctx->calls + i;
		tgRender( ctx, call );
	}

#if TG_LINE_RENDERER
	if ( ctx->line_vert_count )
	{
		if ( ctx->line_depth_test ) glEnable( GL_DEPTH_TEST );
		else glDisable( GL_DEPTH_TEST );
		tgDrawCall call;
		call.vert_count = ctx->line_vert_count;
		call.verts = ctx->line_verts;
		call.r = &ctx->line_r;
		call.texture_count = 0;
		tgRender( ctx, &call );
		ctx->line_vert_count = 0;
	}
#endif

	if ( fb )
	{
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glClear( GL_COLOR_BUFFER_BIT );
		glDisable( GL_DEPTH_TEST );

		tgSetActiveShader( fb->shader );
		glBindBuffer( GL_ARRAY_BUFFER, fb->quad_id );
		glBindTexture( GL_TEXTURE_2D, fb->tex_id );
		glEnableVertexAttribArray( 0 );
		glVertexAttribPointer( 0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof( GLfloat ), (GLvoid*)0);
		glEnableVertexAttribArray( 1 );
		glVertexAttribPointer( 1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof( GLfloat ), (GLvoid*)(2 * sizeof( GLfloat )) );
		glDrawArrays( GL_TRIANGLES, 0, 6 );
		glBindBuffer( GL_ARRAY_BUFFER, 0 );
		tgDeactivateShader( );
	}
}

void tgFlush( void* ctx, tgFunc swap, tgFramebuffer* fb )
{
	tgPresent( ctx, fb );
	tgContext* context = (tgContext*)ctx;
	context->count = 0;

	swap( );
}

#include <math.h>

void tgPerspective( float* m, float y_fov_radians, float aspect, float n, float f )
{
	float a = 1.0f / (float) tanf( y_fov_radians / 2.0f );

	m[ 0 ] = a / aspect;
	m[ 1 ] = 0;
	m[ 2 ] = 0;
	m[ 3 ] = 0;

	m[ 4 ] = 0;
	m[ 5 ] = a;
	m[ 6 ] = 0;
	m[ 7 ] = 0;

	m[ 8 ] = 0;
	m[ 9 ] = 0;
	m[ 10 ] = -((f + n) / (f - n));
	m[ 11 ] = -1.0f;

	m[ 12 ] = 0;
	m[ 13 ] = 0;
	m[ 14 ] = -((2.0f * f * n) / (f - n));
	m[ 15 ] = 0;
}

void tgOrtho2D( float w, float h, float x, float y, float* m )
{
	float left = -w/2;
	float right = w/2;
	float top = h/2;
	float bottom = -h/2;
	float far_ = 1000.0f;
	float near_ = -1000.0f;

	memset( m, 0, sizeof( float ) * 4 * 4 );

	// ortho
	m[ 0 ] = 2.0f / (right - left);
	m[ 5 ] = 2.0f / (top - bottom);
	m[ 10 ] = -2.0f / (far_ - near_);
	m[ 15 ] = 1.0f;

	// translate
	m[ 12 ] = -x;
	m[ 13 ] = -y;
}

void tgMul( float* a, float* b, float* out )
{
	float c[ 16 ];

	c[ 0 + 0 * 4 ] = a[ 0 + 0 * 4 ] * b[ 0 + 0 * 4 ] + a[ 0 + 1 * 4 ] * b[ 1 + 0 * 4 ] + a[ 0 + 2 * 4 ] * b[ 2 + 0 * 4 ] + a[ 0 + 3 * 4 ] * b[ 3 + 0 * 4 ];
	c[ 0 + 1 * 4 ] = a[ 0 + 0 * 4 ] * b[ 0 + 1 * 4 ] + a[ 0 + 1 * 4 ] * b[ 1 + 1 * 4 ] + a[ 0 + 2 * 4 ] * b[ 2 + 1 * 4 ] + a[ 0 + 3 * 4 ] * b[ 3 + 1 * 4 ];
	c[ 0 + 2 * 4 ] = a[ 0 + 0 * 4 ] * b[ 0 + 2 * 4 ] + a[ 0 + 1 * 4 ] * b[ 1 + 2 * 4 ] + a[ 0 + 2 * 4 ] * b[ 2 + 2 * 4 ] + a[ 0 + 3 * 4 ] * b[ 3 + 2 * 4 ];
	c[ 0 + 3 * 4 ] = a[ 0 + 0 * 4 ] * b[ 0 + 3 * 4 ] + a[ 0 + 1 * 4 ] * b[ 1 + 3 * 4 ] + a[ 0 + 2 * 4 ] * b[ 2 + 3 * 4 ] + a[ 0 + 3 * 4 ] * b[ 3 + 3 * 4 ];
	c[ 1 + 0 * 4 ] = a[ 1 + 0 * 4 ] * b[ 0 + 0 * 4 ] + a[ 1 + 1 * 4 ] * b[ 1 + 0 * 4 ] + a[ 1 + 2 * 4 ] * b[ 2 + 0 * 4 ] + a[ 1 + 3 * 4 ] * b[ 3 + 0 * 4 ];
	c[ 1 + 1 * 4 ] = a[ 1 + 0 * 4 ] * b[ 0 + 1 * 4 ] + a[ 1 + 1 * 4 ] * b[ 1 + 1 * 4 ] + a[ 1 + 2 * 4 ] * b[ 2 + 1 * 4 ] + a[ 1 + 3 * 4 ] * b[ 3 + 1 * 4 ];
	c[ 1 + 2 * 4 ] = a[ 1 + 0 * 4 ] * b[ 0 + 2 * 4 ] + a[ 1 + 1 * 4 ] * b[ 1 + 2 * 4 ] + a[ 1 + 2 * 4 ] * b[ 2 + 2 * 4 ] + a[ 1 + 3 * 4 ] * b[ 3 + 2 * 4 ];
	c[ 1 + 3 * 4 ] = a[ 1 + 0 * 4 ] * b[ 0 + 3 * 4 ] + a[ 1 + 1 * 4 ] * b[ 1 + 3 * 4 ] + a[ 1 + 2 * 4 ] * b[ 2 + 3 * 4 ] + a[ 1 + 3 * 4 ] * b[ 3 + 3 * 4 ];
	c[ 2 + 0 * 4 ] = a[ 2 + 0 * 4 ] * b[ 0 + 0 * 4 ] + a[ 2 + 1 * 4 ] * b[ 1 + 0 * 4 ] + a[ 2 + 2 * 4 ] * b[ 2 + 0 * 4 ] + a[ 2 + 3 * 4 ] * b[ 3 + 0 * 4 ];
	c[ 2 + 1 * 4 ] = a[ 2 + 0 * 4 ] * b[ 0 + 1 * 4 ] + a[ 2 + 1 * 4 ] * b[ 1 + 1 * 4 ] + a[ 2 + 2 * 4 ] * b[ 2 + 1 * 4 ] + a[ 2 + 3 * 4 ] * b[ 3 + 1 * 4 ];
	c[ 2 + 2 * 4 ] = a[ 2 + 0 * 4 ] * b[ 0 + 2 * 4 ] + a[ 2 + 1 * 4 ] * b[ 1 + 2 * 4 ] + a[ 2 + 2 * 4 ] * b[ 2 + 2 * 4 ] + a[ 2 + 3 * 4 ] * b[ 3 + 2 * 4 ];
	c[ 2 + 3 * 4 ] = a[ 2 + 0 * 4 ] * b[ 0 + 3 * 4 ] + a[ 2 + 1 * 4 ] * b[ 1 + 3 * 4 ] + a[ 2 + 2 * 4 ] * b[ 2 + 3 * 4 ] + a[ 2 + 3 * 4 ] * b[ 3 + 3 * 4 ];
	c[ 3 + 0 * 4 ] = a[ 3 + 0 * 4 ] * b[ 0 + 0 * 4 ] + a[ 3 + 1 * 4 ] * b[ 1 + 0 * 4 ] + a[ 3 + 2 * 4 ] * b[ 2 + 0 * 4 ] + a[ 3 + 3 * 4 ] * b[ 3 + 0 * 4 ];
	c[ 3 + 1 * 4 ] = a[ 3 + 0 * 4 ] * b[ 0 + 1 * 4 ] + a[ 3 + 1 * 4 ] * b[ 1 + 1 * 4 ] + a[ 3 + 2 * 4 ] * b[ 2 + 1 * 4 ] + a[ 3 + 3 * 4 ] * b[ 3 + 1 * 4 ];
	c[ 3 + 2 * 4 ] = a[ 3 + 0 * 4 ] * b[ 0 + 2 * 4 ] + a[ 3 + 1 * 4 ] * b[ 1 + 2 * 4 ] + a[ 3 + 2 * 4 ] * b[ 2 + 2 * 4 ] + a[ 3 + 3 * 4 ] * b[ 3 + 2 * 4 ];
	c[ 3 + 3 * 4 ] = a[ 3 + 0 * 4 ] * b[ 0 + 3 * 4 ] + a[ 3 + 1 * 4 ] * b[ 1 + 3 * 4 ] + a[ 3 + 2 * 4 ] * b[ 2 + 3 * 4 ] + a[ 3 + 3 * 4 ] * b[ 3 + 3 * 4 ];

	for ( int i = 0; i < 16; ++i )
		out[ i ] = c[ i ];
}

void tgMulv( float* a, float* b )
{
	float result[ 4 ];

	result[ 0 ] = a[ 0 ] * b[ 0 ] + a[4] * b[ 1 ] + a[ 8 ] * b[ 2 ] + a[ 12 ] * b[ 3 ];
	result[ 1 ] = a[ 1 ] * b[ 0 ] + a[5] * b[ 1 ] + a[ 9 ] * b[ 2 ] + a[ 13 ] * b[ 3 ];
	result[ 2 ] = a[ 2 ] * b[ 0 ] + a[6] * b[ 1 ] + a[ 10 ] * b[ 2 ] + a[ 14 ] * b[ 3 ];
	result[ 3 ] = a[ 3 ] * b[ 0 ] + a[7] * b[ 1 ] + a[ 11 ] * b[ 2 ] + a[ 15 ] * b[ 3 ];

	b[ 0 ] = result[ 0 ];
	b[ 1 ] = result[ 1 ];
	b[ 2 ] = result[ 2 ];
	b[ 3 ] = result[ 3 ];
}

void tgIdentity( float* m )
{
	memset( m, 0, sizeof( float ) * 16 );
	m[ 0 ] = 1.0f;
	m[ 5 ] = 1.0f;
	m[ 10 ] = 1.0f;
	m[ 15 ] = 1.0f;
}

#if TG_DEBUG_CHECKS
#if defined(__APPLE__)
	#include <OpenGL/gl.h>
	#include <OpenGL/glu.h>
#else
	#include <GL/gl.h>
	#include <GL/glu.h>
#endif
#pragma comment( lib, "glu32.lib" )
void tgPrintGLErrors_internal( char* file, uint32_t line )
{
	GLenum code = glGetError( );

	if ( code != GL_NO_ERROR )
	{
		char* last_slash = file;

		while ( *file )
		{
			char c = *file;
			if ( c == '\\' || c == '/' )
				last_slash = file + 1;
			++file;
		}

		const char* str = (const char*)gluErrorString( code );
		TG_WARN( "OpenGL Error %s ( %u ): %u, %s\n", last_slash, line, code, str );
	}
}
#endif

#endif // TINYGL_IMPL
