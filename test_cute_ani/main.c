#define CUTE_ANI_IMPLEMENTATION
#include <cute_ani.h>

#include <stdio.h>
#include <string.h>

void print_ani(cute_ani_map_t* map, cute_ani_t* ani)
{
	printf("current frame: %s\n", cute_ani_map_cstr(map, cute_ani_current_image(ani)));
	printf("frame time: %f\n", ani->seconds);
	printf("play count: %d\n", ani->play_count);
	printf("frames:\n");
	for (int i = 0; i < ani->frame_count; ++i) printf("\t\"%s\" %f\n", cute_ani_map_cstr(map, ani->frames[i].image_id), ani->frames[i].seconds);
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

	cute_ani_map_t* ani_map = cute_ani_map_create(0);
	cute_ani_t smoke;
	cute_ani_t mushroom;
	cute_ani_t dog;

	if (cute_ani_load_from_mem(ani_map, &smoke, smoke_mem, strlen(smoke_mem), 0) != CUTE_ANI_SUCCESS) return -1;
	if (cute_ani_load_from_mem(ani_map, &mushroom, mushroom_mem, strlen(mushroom_mem), 0) != CUTE_ANI_SUCCESS) return -1;
	if (cute_ani_load_from_mem(ani_map, &dog, dog_mem, strlen(dog_mem), 0) != CUTE_ANI_SUCCESS) return -1;

	float dt = 0.01f;
	float time = 0;

	while (time < 5)
	{
		cute_ani_update(&smoke, dt);
		cute_ani_update(&mushroom, dt);
		cute_ani_update(&dog, dt);
		time += dt;
	}

	print_ani(ani_map, &smoke);
	print_ani(ani_map, &mushroom);
	print_ani(ani_map, &dog);

	cute_ani_map_destroy(ani_map);

	return 0;
}
