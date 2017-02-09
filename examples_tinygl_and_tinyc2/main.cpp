#include "glad/glad.h"
#include "glfw/glfw_config.h"
#include "glfw/glfw3.h"
#include "../tinyc2.h"

#include <cstdio>

#define TINYGL_IMPL
#include "../tinygl.h"

#define TT_IMPLEMENTATION
#include "../tinytime.h"

GLFWwindow* window;
float projection[ 16 ];
tgShader simple;
int use_post_fx = 0;
tgFramebuffer fb;
tgShader post_fx;

void* ReadFileToMemory( const char* path, int* size )
{
	void* data = 0;
	FILE* fp = fopen( path, "rb" );
	int sizeNum = 0;

	if ( fp )
	{
		fseek( fp, 0, SEEK_END );
		sizeNum = ftell( fp );
		fseek( fp, 0, SEEK_SET );
		data = malloc( sizeNum + 1 );
		fread( data, sizeNum, 1, fp );
		((char*)data)[ sizeNum ] = 0;
		fclose( fp );
	}

	if ( size ) *size = sizeNum;
	return data;
}

void ErrorCB( int error, const char* description )
{
	fprintf( stderr, "Error: %s\n", description );
}

void KeyCB( GLFWwindow* window, int key, int scancode, int action, int mods )
{
	if ( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS )
		glfwSetWindowShouldClose( window, GLFW_TRUE );

	if ( key == GLFW_KEY_SPACE && action == GLFW_PRESS )
		use_post_fx = !use_post_fx;
}

void MouseCB( GLFWwindow* window, double x, double y )
{
}

void ResizeFramebuffer( int w, int h )
{
	static int first = 1;
	if ( first ) 
	{
		first = 0;
		char* vs = (char*)ReadFileToMemory( "postprocess.vs", 0 );
		char* ps = (char*)ReadFileToMemory( "postprocess.ps", 0 );
		tgLoadShader( &post_fx, vs, ps );
		free( vs );
		free( ps );
	}
	else tgFreeFramebuffer( &fb );

	tgMakeFramebuffer( &fb, &post_fx, w, h );
}

void Reshape( GLFWwindow* window, int width, int height )
{
	tgOrtho2D( (float)width, (float)height, 0, 0, projection );
	glViewport( 0, 0, width, height );
	ResizeFramebuffer( width, height );
}

void SwapBuffers( )
{
	glfwSwapBuffers( window );
}

#include <vector>

struct Color
{
	float r;
	float g;
	float b;
};

struct Vertex
{
	c2v pos;
	Color col;
};

std::vector<Vertex> verts;

int main( )
{
	// glfw and glad setup
	glfwSetErrorCallback( ErrorCB );

	if ( !glfwInit( ) )
		return 1;

	glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3 );
	glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 2 );
	glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
	glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE );

	int width = 640;
	int height = 480;
	window = glfwCreateWindow( width, height, "tinyc2 and tinygl", NULL, NULL );

	if ( !window )
	{
		glfwTerminate( );
		return 1;
	}

	glfwSetCursorPosCallback( window, MouseCB );
	glfwSetKeyCallback( window, KeyCB );
	glfwSetFramebufferSizeCallback( window, Reshape );

	glfwMakeContextCurrent( window );
	gladLoadGLLoader( (GLADloadproc)glfwGetProcAddress );
	glfwSwapInterval( 1 );

	glfwGetFramebufferSize( window, &width, &height );
	Reshape( window, width, height );

	// tinygl setup
	void* ctx = tgMakeCtx( 32 );

	// define the attributes of vertices, which are inputs to the vertex shader
	tgVertexData vd;
	tgMakeVertexData( &vd, 3, sizeof( Vertex ), GL_TRIANGLES, GL_STATIC_DRAW );
	tgAddAttribute( &vd, "in_pos", 2, TG_FLOAT, TG_OFFSET_OF( Vertex, pos ) );
	tgAddAttribute( &vd, "in_col", 3, TG_FLOAT, TG_OFFSET_OF( Vertex, col ) );

	// a renderable holds a shader (tgShader) + vertex definition (tgVertexData)
	// renderables are used to construct draw calls (see main loop below)
	tgRenderable r;
	tgMakeRenderable( &r, &vd );
	char* vs = (char*)ReadFileToMemory( "simple.vs", 0 );
	char* ps = (char*)ReadFileToMemory( "simple.ps", 0 );
	TG_ASSERT( vs );
	TG_ASSERT( ps );
	tgLoadShader( &simple, vs, ps );
	free( vs );
	free( ps );
	tgSetShader( &r, &simple );
	tgSendMatrix( &simple, "u_mvp", projection );

	Vertex v[ 3 ];
	Color c = { 1.0f, 0.0f, 0.0f };
	v[ 0 ].col = c;
	v[ 1 ].col = c;
	v[ 2 ].col = c;

	v[ 0 ].pos = c2V( 0, 100 );
	v[ 1 ].pos = c2V( -100, 0 );
	v[ 2 ].pos = c2V( 100, -100 );

	// main loop
	glClearColor( 0, 0, 0, 1 );
	float t = 0;
	while ( !glfwWindowShouldClose( window ) )
	{
		glfwPollEvents( );

		float dt = ttTime( );
		t += dt;
		t = fmod( t, 2.0f * 3.14159265f );

		tgSendF32( &post_fx, "u_time", 1, &t, 1 );

		// push a draw call to tinygl
		// all members of a tgDrawCall *must* be initialized
		// optionally the fbo can be NULL or 0 to signify no post-processing fx
		tgDrawCall call;
		call.r = &r;
		call.texture_count = 0;
		call.verts = &v;
		call.vert_count = 3;
		call.fbo = use_post_fx ? &fb : 0;
		tgPushDrawCall( ctx, call );

		// flush all draw calls to screen
		tgFlush( ctx, SwapBuffers );
		TG_PRINT_GL_ERRORS( );
	}

	tgFreeCtx( ctx );
	glfwDestroyWindow( window );
	glfwTerminate( );

	return 0;
}
