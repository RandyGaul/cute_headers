#define CUTE_SOUND_IMPLEMENTATION
#include <cute_sound.h>

#define CUTE_SOUND_PITCH_PLUGIN_IMPLEMENTATION
#include <cute_sound_pitch_plugin.h>

// This file was originally written by Aaron Balint, see:
// https://forums.tigsource.com/index.php?topic=58706.0
int main()
{
	void* hwnd = NULL;
#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS
	hwnd = GetConsoleWindow();
#endif
	cs_context_t* ctx = cs_make_context(hwnd, 44100, 4096 * 16, 5, NULL);
	cs_thread_sleep_delay(ctx, 5);
	cs_loaded_sound_t loaded_sound = cs_load_wav("la.wav");
	int delay = 1000 * loaded_sound.sample_count / 44100;

	cs_plugin_interface_t plugin = csp_get_pitch_plugin();
	cs_plugin_id_t pitch_plugin_id = cs_add_plugin(ctx, &plugin);

	cs_spawn_mix_thread(ctx);
	cs_play_sound_def_t def = cs_make_def(&loaded_sound);
	cs_playing_sound_t* sound;

	sound = cs_play_sound(ctx, def);
	csp_set_pitch(sound, 0.5f, pitch_plugin_id);
	cs_sleep(delay);

	sound = cs_play_sound(ctx, def);
	csp_set_pitch(sound, 1.0f, pitch_plugin_id);
	cs_sleep(delay);

	def.looped = 1;
	cs_playing_sound_t* sound1 = cs_play_sound(ctx, def);
	csp_set_pitch(sound1, 1.5, pitch_plugin_id);
	cs_sleep(delay >> 1);

	cs_playing_sound_t* sound2 = cs_play_sound(ctx, def);
	csp_set_pitch(sound2, 1.0f, pitch_plugin_id);
	cs_sleep(delay >> 1);

	cs_playing_sound_t* sound3 = cs_play_sound(ctx, def);
	csp_set_pitch(sound3, 0.5f, pitch_plugin_id);
	cs_sleep((int)(1.5f * (float)delay));

	for (int i = 0; i < 100; ++i)
	{
		csp_set_pitch(sound1, 1.5f - i / 100.0f, pitch_plugin_id);
		csp_set_pitch(sound2, 1.0f - 0.5f * i / 100.0f, pitch_plugin_id);
		cs_sleep(delay/100);
	}

	cs_loop_sound(sound1, 0);
	cs_loop_sound(sound2, 0);
	cs_loop_sound(sound3, 0);
	cs_sleep(delay);

	cs_shutdown_context(ctx);
}
