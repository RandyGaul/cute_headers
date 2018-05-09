#define TINYANI_IMPLEMENTATION
#include <tinyani.h>

#include <stdio.h>
#include <string.h>

void print_ani(tinyani_map_t* map, tinyani_t* ani)
{
	printf("current frame: %s\n", tinyani_map_cstr(map, tinyani_current_image(ani)));
	printf("frame time: %f\n", ani->seconds);
	printf("play count: %d\n", ani->play_count);
	printf("frames:\n");
	for (int i = 0; i < ani->frame_count; ++i) printf("\t\"%s\" %f\n", tinyani_map_cstr(map, ani->frames[i].image_id), ani->frames[i].seconds);
	printf("\t\"end\"\n\n");
}

int main()
{
#define STR(x) #x

	const char* smoke_mem = STR(
		"smoke0.png" 0.15
		"smoke1.png" 0.15
		"smoke2.png" 0.15
		"smoke3.png" 0.15
		"end"
	);

	const char* mushroom_mem = STR(
		"mushroom0.png" 0.25
		"mushroom1.png" 0.25
		"mushroom2.png" 0.25
		"mushroom3.png" 0.25
		"end"
	);

	const char* dog_mem = STR(
		"dog0.png" 0.3
		"dog1.png" 0.15
		"end"
	);

	tinyani_map_t* ani_map = tinyani_map_create(0);
	tinyani_t smoke;
	tinyani_t mushroom;
	tinyani_t dog;

	if (tinyani_load_from_mem(ani_map, &smoke, smoke_mem, strlen(smoke_mem), 0) != TINYANI_SUCCESS) return -1;
	if (tinyani_load_from_mem(ani_map, &mushroom, mushroom_mem, strlen(mushroom_mem), 0) != TINYANI_SUCCESS) return -1;
	if (tinyani_load_from_mem(ani_map, &dog, dog_mem, strlen(dog_mem), 0) != TINYANI_SUCCESS) return -1;

	float dt = 0.01f;
	float time = 0;

	while (time < 5)
	{
		tinyani_update(&smoke, dt);
		tinyani_update(&mushroom, dt);
		tinyani_update(&dog, dt);
		time += dt;
	}

	print_ani(ani_map, &smoke);
	print_ani(ani_map, &mushroom);
	print_ani(ani_map, &dog);

	tinyani_map_destroy(ani_map);

	return 0;
}
