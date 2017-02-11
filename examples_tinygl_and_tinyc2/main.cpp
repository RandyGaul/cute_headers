#include "glad/glad.h"
#include "glfw/glfw_config.h"
#include "glfw/glfw3.h"

#define TINYGL_IMPL
#include "../tinygl.h"

#define TT_IMPLEMENTATION
#include "../tinytime.h"

#define TINYC2_IMPL
#include "../tinyc2.h"

GLFWwindow* window;
float projection[ 16 ];
tgShader simple;
int use_post_fx = 0;
tgFramebuffer fb;
tgShader post_fx;
int spaced_pressed;
void* ctx;
float screen_w;
float screen_h;
float mouse_x;
float mouse_y;

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
		spaced_pressed = 1;

	if ( key == GLFW_KEY_SPACE && action == GLFW_RELEASE )
		spaced_pressed = 0;

	if ( key == GLFW_KEY_P && action == GLFW_PRESS )
		use_post_fx = !use_post_fx;
}

void MouseCB( GLFWwindow* window, double x, double y )
{
	mouse_x = (float)x - screen_w / 2;
	mouse_y = -((float)y - screen_h / 2);
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
	screen_w = (float)w;
	screen_h = (float)h;
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

// enable depth test here if you care, also clear
void GLSettings( )
{
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

void DrawPoly( c2v* verts, int count )
{
	for ( int i = 0; i < count; ++i )
	{
		int iA = i;
		int iB = (i + 1) % count;
		c2v a = verts[ iA ];
		c2v b = verts[ iB ];
		tgLine( ctx, a.x, a.y, 0, b.x, b.y, 0 );
	}
}

void DrawAABB( c2v a, c2v b )
{
	c2v c = c2V( a.x, b.y );
	c2v d = c2V( b.x, a.y );
	tgLine( ctx, a.x, a.y, 0, c.x, c.y, 0 );
	tgLine( ctx, c.x, c.y, 0, b.x, b.y, 0 );
	tgLine( ctx, b.x, b.y, 0, d.x, d.y, 0 );
	tgLine( ctx, d.x, d.y, 0, a.x, a.y, 0 );
}

void DrawHalfCircle( c2v a, c2v b )
{
	c2v u = c2Sub( b, a );
	float r = c2Len( u );
	u = c2Skew( u );
	c2v v = c2CW90( u );
	c2v s = c2Add( v, a );
	c2m m;
	m.x = c2Norm( u );
	m.y = c2Norm( v );

	int kSegs = 20;
	float theta = 0;
	float inc = 3.14159265f / (float)kSegs;
	c2v p0;
	c2SinCos( theta, &p0.y, &p0.x );
	p0 = c2Mulvs( p0, r );
	p0 = c2Add( c2Mulmv( m, p0 ), a );
	for ( int i = 0; i < kSegs; ++i )
	{
		theta += inc;
		c2v p1;
		c2SinCos( theta, &p1.y, &p1.x );
		p1 = c2Mulvs( p1, r );
		p1 = c2Add( c2Mulmv( m, p1 ), a );
		tgLine( ctx, p0.x, p0.y, 0, p1.x, p1.y, 0 );
		p0 = p1;
	}
}

void DrawCapsule( c2v a, c2v b, float r )
{
	c2v n = c2Norm( c2Sub( b, a ) );
	DrawHalfCircle( a, c2Add( a, c2Mulvs( n, -r ) ) );
	DrawHalfCircle( b, c2Add( b, c2Mulvs( n, r ) ) );
	c2v p0 = c2Add( a, c2Mulvs( c2Skew( n ), r ) );
	c2v p1 = c2Add( b, c2Mulvs( c2CW90( n ), -r ) );
	tgLine( ctx, p0.x, p0.y, 0, p1.x, p1.y, 0 );
	p0 = c2Add( a, c2Mulvs( c2Skew( n ), -r ) );
	p1 = c2Add( b, c2Mulvs( c2CW90( n ), r ) );
	tgLine( ctx, p0.x, p0.y, 0, p1.x, p1.y, 0 );
}

void DrawCircle( c2v p, float r )
{
	int kSegs = 40;
	float theta = 0;
	float inc = 3.14159265f * 2.0f / (float)kSegs;
	float px, py;
	c2SinCos( theta, &py, &px );
	px *= r; py *= r;
	px += p.x; py += p.y;
	for ( int i = 0; i <= kSegs; ++i )
	{
		theta += inc;
		float x, y;
		c2SinCos( theta, &y, &x );
		x *= r; y *= r;
		x += p.x; y += p.y;
		tgLine( ctx, x, y, 0, px, py, 0 );
		px = x; py = y;
	}
}

// should see slow rotation CCW, then CW
// space toggles between two different rotation implements
// after toggling implementations space toggles rotation direction
void TestRotation( )
{
	static int first = 1;
	static Vertex v[ 3 ];
	if ( first )
	{
		first = 0;
		Color c = { 1.0f, 0.0f, 0.0f };
		v[ 0 ].col = c;
		v[ 1 ].col = c;
		v[ 2 ].col = c;
		v[ 0 ].pos = c2V( 0, 100 );
		v[ 1 ].pos = c2V( 0, 0 );
		v[ 2 ].pos = c2V( 100, 0 );
	}

	static int which0;
	static int which1;
	if ( spaced_pressed ) which0 = !which0;
	if ( spaced_pressed && which0 ) which1 = !which1;

	if ( which0 )
	{
		c2m m;
		m.x = c2Norm( c2V( 1, 0.01f ) );
		m.y = c2Skew( m.x );
		for ( int i = 0; i < 3; ++i )
			v[ i ].pos = which1 ? c2Mulmv( m, v[ i ].pos ) : c2MulmvT( m, v[ i ].pos );
	}

	else
	{
		c2r r = c2Rot( 0.01f );
		for ( int i = 0; i < 3; ++i )
			v[ i ].pos = which1 ? c2Mulrv( r, v[ i ].pos ) : c2MulrvT( r, v[ i ].pos );
	}

	for ( int i = 0; i < 3; ++i )
		verts.push_back( v[ i ] );
}

void TestDrawPrim( )
{
	TestRotation( );

	tgLineColor( ctx, 0.2f, 0.6f, 0.8f );
	tgLine( ctx, 0, 0, 0, 100, 100, 0 );
	tgLineColor( ctx, 0.8f, 0.6f, 0.2f );
	tgLine( ctx, 100, 100, 0, -100, 200, 0 );

	DrawCircle( c2V( 0, 0 ), 100.0f );

	tgLineColor( ctx, 0, 1.0f, 0 );
	DrawHalfCircle( c2V( 0, 0 ), c2V( 50, -50 ) );

	tgLineColor( ctx, 0, 0, 1.0f );
	DrawCapsule( c2V( 0, 200 ), c2V( 75, 150 ), 20.0f );

	tgLineColor( ctx, 1.0f, 0, 0 );
	DrawAABB( c2V( -20, -20 ), c2V( 20, 20 ) );

	tgLineColor( ctx, 0.5f, 0.9f, 0.1f );
	c2v poly[] = {
		{ 0, 0 },
		{ 20.0f, 10.0f },
		{ 5.0f, 15.0f },
		{ -3.0f, 7.0f },
	};
	DrawPoly( poly, 4 );
}

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
	// the clear bits are used in glClear, the settings bits are used in glEnable
	// use the | operator to mask together settings/bits, example settings_bits: GL_DEPTH_TEST | GL_STENCIL_TEST
	// either can be 0
	int max_draw_calls_per_flush = 32;
	int clear_bits = GL_COLOR_BUFFER_BIT;
	int settings_bits = 0;
	ctx = tgMakeCtx( max_draw_calls_per_flush, clear_bits, settings_bits );

	// define the attributes of vertices, which are inputs to the vertex shader
	// only accepts GL_TRIANGLES in 4th parameter
	// 5th parameter can be GL_STATIC_DRAW or GL_DYNAMIC_DRAW, which controls triple buffer or single buffering
	tgVertexData vd;
	tgMakeVertexData( &vd, 1024 * 1024, GL_TRIANGLES, sizeof( Vertex ), GL_DYNAMIC_DRAW );
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
	tgLineMVP( ctx, projection );

	// main loop
	glClearColor( 0, 0, 0, 1 );
	float t = 0;
	while ( !glfwWindowShouldClose( window ) )
	{
		if ( spaced_pressed == 1 ) spaced_pressed = 0;
		glfwPollEvents( );

		float dt = ttTime( );
		t += dt;
		t = fmod( t, 2.0f * 3.14159265f );
		tgSendF32( &post_fx, "u_time", 1, &t, 1 );

		TestDrawPrim( );
		DrawCircle( c2V( mouse_x, mouse_y ), 10.0f );

		// push a draw call to tinygl
		// all members of a tgDrawCall *must* be initialized
		if ( verts.size( ) )
		{
			tgDrawCall call;
			call.r = &r;
			call.texture_count = 0;
			call.verts = &verts[ 0 ];
			call.vert_count = verts.size( );
			tgPushDrawCall( ctx, call );
		}

		// flush all draw calls to screen
		// optionally the fb can be NULL or 0 to signify no post-processing fx
		tgFlush( ctx, SwapBuffers, use_post_fx ? &fb : 0 );
		TG_PRINT_GL_ERRORS( );
		verts.clear( );
	}

	tgFreeCtx( ctx );
	glfwDestroyWindow( window );
	glfwTerminate( );

	return 0;
}
