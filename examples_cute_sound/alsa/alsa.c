// create header + implementation
#define CUTE_SOUND_IMPLEMENTATION
#include "cute_sound.h"

int main(int argc, char *args[])
{
	cs_context_t* ctx = cs_make_context(NULL, 44100, 4096, 0, NULL);
	if (!ctx) {
		printf("%s", cs_error_reason);
		return -1;
	}
	cs_loaded_sound_t loaded = cs_load_wav("../jump.wav");
	cs_loaded_sound_t demo_wav = cs_load_wav("demo.wav");

	cs_playing_sound_t jump = cs_make_playing_sound(&loaded);
	cs_playing_sound_t demo_voice = cs_make_playing_sound(&demo_wav);
	cs_spawn_mix_thread(ctx);

	printf("Play a voice...\n");
	cs_insert_sound(ctx, &demo_voice);
	cs_sleep(2500);

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

	printf("And both at once!\n");
	cs_insert_sound(ctx, &demo_voice);

	count = 8;
	while (count--)
	{
		cs_sleep(500);
		cs_insert_sound(ctx, &jump);
		printf("Jump!\n");
	}

	cs_free_sound(&loaded);
	cs_shutdown_context(ctx);

	return 0;
}

