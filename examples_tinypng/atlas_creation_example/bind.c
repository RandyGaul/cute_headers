#define TINYDEFLATE_IMPL
#include "../tinydeflate.h"

int main( )
{
    const char* png_names[] = {
            "test01.png",
            "test02.png",
            "test03.png",
            "test04.png",
        };
    tdImage pngs[ 4 ];
    for ( int i = 0; i < 4; ++i )
    pngs[ i ] = tdLoadPNG( png_names[ i ] );
    tdMakeAtlas( "atlas.png", "atlas.txt", 256, 256, pngs, 4, png_names );
}