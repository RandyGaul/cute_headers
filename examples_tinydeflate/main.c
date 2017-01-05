#define TINYDEFLATE_IMPL
#include "tinydeflate.h"

void TestAtlas( )
{
	const char* png_names[] = {
		"imgs/1x1.png",
		"imgs/4x4.png",
		"imgs/debug_tile.png",
		"imgs/default.png",
		"imgs/house_blue.png",
		"imgs/house_red.png",
		"imgs/house_yellow.png",
		"imgs/squinkle.png",
	};
	tdImage pngs[ 8 ];
	for ( int i = 0; i < 8; ++i )
		pngs[ i ] = tdLoadPNG( png_names[ i ] );
	tdMakeAtlas( "atlas.png", "atlas.txt", 64, 64, pngs, 8, png_names );
}

int main( )
{
	//TestAtlas( );
	//tdImage img = tdLoadPNG( "atlas.png" );
	int out_size;
	const char* str = "    hello kekekekeke hello hi hi 123 321 123 321...";
	int len = (int)(strlen( str ) + 1);
	void* memory = malloc( len );
	memcpy( memory, str, len );
	void* out = tdDeflateMem( memory, len, &out_size, 0 );
	char* deflated = (char*)malloc( len );
	tdInflate( out, out_size, deflated, len );
	return 0;
}
