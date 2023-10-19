#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CUTE_TILED_IMPLEMENTATION
#include "cute_tiled.h"

int main() {
    cute_tiled_map_t* map = cute_tiled_load_map_from_file("test_cute_tiled.json", NULL);
    assert(map);
    assert(map->width == 20);
    assert(map->height == 15);
    assert(strcmp(map->class_.ptr, "desert") == 0);

    cute_tiled_layer_t* layer = map->layers;
    while (layer) {
        if (strcmp(layer->name.ptr, "Structures") == 0) {
            break;
        }
        layer = layer->next;
    }
    
    assert(layer);
    assert(strcmp(layer->class_.ptr, "structures") == 0);

    cute_tiled_free_map(map);
    printf("test_cute_tiled: Tests passed\n");

    return 0;
}