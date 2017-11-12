#define TINYPNG_IMPLEMENTATION
#include "tinypng.h"

int main( )
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
	tpImage pngs[ 8 ];
	for ( int i = 0; i < 8; ++i )
		pngs[ i ] = tpLoadPNG( png_names[ i ] );

	int png_count = 8;
	tpAtlasImage* atlas_img_infos = (tpAtlasImage*)malloc( sizeof( tpAtlasImage ) * png_count );
	tpImage atlas_img = tpMakeAtlas( 64, 64, pngs, png_count, atlas_img_infos );
	if ( !atlas_img.pix )
	{
		printf( "tpMakeAtlas failed: %s", g_tpErrorReason );
		return -1;
	}

	tpDefaultSaveAtlas( "atlas.png", "atlas.txt", &atlas_img, atlas_img_infos, png_count, png_names );
	return 0;
}
