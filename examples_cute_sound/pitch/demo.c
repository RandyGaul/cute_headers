#define CUTE_SOUND_IMPLEMENTATION
#include <cute_sound.h>

// this file was originally written by Aaron Balint, see:
// https://forums.tigsource.com/index.php?topic=58706.0
int main()
{
#if TS_PLATFORM == TS_WINDOWS
	cs_context_t* ctx = cs_make_context(GetConsoleWindow(), 44100, 15, 5, 5);
#else
	cs_context_t* ctx = cs_make_context(0, 44100, 15, 5, 5);
#endif
	cs_thread_sleep_delay(ctx, 5);
	cs_loaded_sound_t loaded_sound = cs_load_wav("la.wav");
	int delay = 1000 * loaded_sound.sample_count / 44100;

	cs_spawn_mix_thread(ctx);
	cs_play_sound_def_t def = cs_make_def(&loaded_sound);

	def.pitch = 0.5;
	cs_play_sound(ctx, def);
	cs_sleep(delay);

	def.pitch = 1.0;
	cs_play_sound(ctx, def);
	cs_sleep(delay);

	def.pitch = 1.5;
	def.looped = 1;
	cs_playing_sound_t* sound1 = cs_play_sound(ctx, def);
	cs_sleep(delay >> 1);

	def.pitch = 1.0;
	cs_playing_sound_t* sound2 = cs_play_sound(ctx, def);
	cs_sleep(delay >> 1);

	def.pitch = 0.5;
	cs_playing_sound_t* sound3 = cs_play_sound(ctx, def);
	cs_sleep((int)(1.5f * (float)delay));

	for (int i=0; i<100; i++)
	{
		cs_set_pitch(sound1, 1.5f - i / 100.0f);
		cs_set_pitch(sound2, 1.0f - 0.5f * i / 100.0f);
		cs_sleep(delay/100);
	}

	cs_loop_sound(sound1, 0);
	cs_loop_sound(sound2, 0);
	cs_loop_sound(sound3, 0);
	cs_sleep(delay);

	cs_shutdown_context(ctx);
}
