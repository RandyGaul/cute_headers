#define _CRT_SECURE_NO_WARNINGS

#define STB_VORBIS_HEADER_ONLY
#include "../stb_vorbis.c"

#define CUTE_SOUND_SDL_H <SDL2/SDL.h>
#define CUTE_SOUND_IMPLEMENTATION
#define CUTE_SOUND_PLATFORM_SDL
#define CUTE_SOUND_SDL_H <SDL2/SDL.h>
#include <cute_sound.h>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define CUTE_TIME_IMPLEMENTATION
#include "cute_time.h"

int main(int argc, char *args[])
{
	void* os_handle = NULL;
#ifndef CUTE_SOUND_PLATFORM_SDL
	os_handle = GetConsoleWindow();
#endif
	cs_init(os_handle, 44100, 1024, NULL);
	cs_audio_source_t* jump = cs_load_wav("../jump.wav", NULL);
	cs_spawn_mix_thread();
	cs_mix_thread_sleep_delay(1);

	printf("Jump five times...\n");
	cs_sleep(500);
	int count = 5;
	while (count--) {
		cs_sleep(500);
		cs_sound_params_t params = cs_sound_params_default();
		cs_play_sound(jump, params);
		printf("Jump!\n");
	}
	
	cs_sleep(500);
	printf("Jump five times with various pitches...\n");
	cs_sleep(1000);
	
	count = 5;
	while (count--) {
		cs_sleep(500);
		cs_sound_params_t params = cs_sound_params_default();
		params.pitch = ((float)count / 4.0f) + 0.5f;
		cs_play_sound(jump, params);
		printf("Jump!\n");
	}

	printf("Loading some music...\n");
	cs_audio_source_t* song1 = cs_load_ogg("song1.ogg", NULL);
	cs_audio_source_t* song2 = cs_load_ogg("song2.ogg", NULL);

	printf("Play some music.\n");
	cs_music_play(song1, 1.0f);

	float elapsed = 0;
	while (elapsed < 5) {
		float dt = ct_time();
		elapsed += dt;
		cs_update(dt);
		cs_sleep(1);
	}

	printf("Crossfade the music.\n");
	cs_music_crossfade(song2, 3.0f);

	elapsed = 0;
	while (elapsed < 5) {
		float dt = ct_time();
		elapsed += dt;
		cs_update(dt);
		cs_sleep(1);
	}

	printf("Switch the music.\n");
	cs_music_switch_to(song1, 1.0f, 1.0f);

	elapsed = 0;
	while (elapsed < 3) {
		float dt = ct_time();
		elapsed += dt;
		cs_update(dt);
		cs_sleep(1);
	}

	printf("Modulate music pitch.\n");

	elapsed = 0;
	while (elapsed < 5) {
		float dt = ct_time();
		elapsed += dt;
		cs_music_set_pitch(elapsed / 5.0f + 0.3f);
		cs_update(dt);
		cs_sleep(1);
	}

	printf("Stop the music.\n");
	cs_music_stop(1.0f);

	elapsed = 0;
	while (elapsed < 3) {
		float dt = ct_time();
		elapsed += dt;
		cs_update(dt);
		cs_sleep(1);
	}

	cs_free_audio_source(jump);
	cs_free_audio_source(song1);
	cs_free_audio_source(song2);

	cs_shutdown();

	return 0;
}

#undef STB_VORBIS_HEADER_ONLY
#include "../stb_vorbis.c"
