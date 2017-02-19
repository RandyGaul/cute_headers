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
	////tdImage img = tdLoadPNG( "atlas.png" );

	int feldpar_size;
	const char* str = tdReadFileToMemory( "feldspar.txt", &feldpar_size );
	++feldpar_size;

	int out_size;
	void* out = tdDeflateMem( str, feldpar_size, &out_size, 0 );
	char* deflated = (char*)calloc( 1, feldpar_size * 2 );
	int inflatedSize = tdInflate( out, out_size, deflated, feldpar_size * 2 );

	//TestAtlas( );
	//tdImage img = tdLoadPNG( "atlas.png" );
	//int out_size;
	//const char* str = "abcdefghijklmnopqrstuvwxyz1234567890 asdf 2o f29 as-00as--xzl nkmasnofnn	-- 92 i dont really like you mr pickle man :D :D :D xd haha no 234567890 asdf 2o f29 as-00as--xzl nkmasnofnn	-- 92ofnn	-- 92 i dont really like you mr pickle man :D :D :D xd haha no 234567890 asdf 2o f29 as-00as--xzl nkmasnofnn	-- 92";
	//int len = (int)(strlen( str ) + 1);
	//void* memory = malloc( len );
	//memcpy( memory, str, len );
	//void* out = tdDeflateMem( memory, len, &out_size, 0 );
	//char* deflated = (char*)malloc( len );
	//tdInflate( out, out_size, deflated, len );

	int a = strcmp( str, deflated );
	FILE* fp = fopen( "feldspar_deflated.txt", "wb" );
	fwrite( deflated, strlen( deflated ), 1, fp );
	fclose( fp );
	return 0;
}
