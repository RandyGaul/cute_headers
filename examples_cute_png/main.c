#define CUTE_PNG_IMPLEMENTATION
#include <cute_png.h>

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
	cp_image_t pngs[ 8 ];
	for ( int i = 0; i < 8; ++i )
		pngs[ i ] = cp_load_png( png_names[ i ] );

	int png_count = 8;
	cp_atlas_image_t* atlas_img_infos = (cp_atlas_image_t*)malloc( sizeof( cp_atlas_image_t ) * png_count );
	cp_image_t atlas_img = cp_make_atlas( 64, 64, pngs, png_count, atlas_img_infos );
	if ( !atlas_img.pix )
	{
		printf( "tpMakeAtlas failed: %s", cp_error_reason );
		return -1;
	}

	cp_default_save_atlas( "atlas.png", "atlas.txt", &atlas_img, atlas_img_infos, png_count, png_names );
	return 0;
}
