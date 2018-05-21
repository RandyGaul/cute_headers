#include <Windows.h>

#define _CRT_SECURE_NO_WARNINGS
#include <SDL2/SDL.h>

#define CUTE_SOUND_IMPLEMENTATION
#define CUTE_SOUND_FORCE_SDL
#include <cute_sound.h>

int main(int argc, char *args[])
{
	cs_context_t* ctx = cs_make_context(0, 44100, 15, 5, 0);
	cs_loaded_sound_t loaded = cs_load_wav("../jump.wav");

	cs_playing_sound_t jump = cs_make_playing_sound(&loaded);
	cs_spawn_mix_thread(ctx);

	printf("Jump ten times...\n");
	cs_sleep(500);

	int count = 10;
	while (count--)
	{
		cs_sleep(500);
		cs_insert_sound(ctx, &jump);
		printf("Jump!\n");
	}

	cs_sleep(500);
	cs_free_sound(&loaded);

	return 0;
}
