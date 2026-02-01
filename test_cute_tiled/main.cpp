#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void* dummy_alloc(size_t size, void* ctx) {
    (void)ctx;
    return malloc(size);
}

void dummy_free(void* ptr, void* ctx) {
    (void)ctx;
    free(ptr);
}

#define CUTE_TILED_IMPLEMENTATION
#define CUTE_TILED_ALLOC(size, ctx) dummy_alloc(size, ctx)
#define CUTE_TILED_FREE(ptr, ctx) dummy_free(ptr, ctx)
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