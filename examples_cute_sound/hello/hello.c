// create header + implementation
#define CUTE_SOUND_IMPLEMENTATION
#include "cute_sound.h"

#include <stdio.h>

int main()
{
	HWND hwnd = GetConsoleWindow();
	cs_context_t* ctx = cs_make_context(hwnd, 44100, 15, 5, 0);
	cs_loaded_sound_t jump_audio = cs_load_wav("../jump.wav");
	cs_loaded_sound_t select_audio = cs_load_wav("select.wav");
	cs_playing_sound_t jump_instance = cs_make_playing_sound(&jump_audio);
	cs_playing_sound_t select_instance = cs_make_playing_sound(&select_audio);
	printf("jump.wav has a sample rate of %d Hz.\n", jump_audio.sample_rate);
	printf("select.wav has a sample rate of %d Hz.\n", select_audio.sample_rate);

	printf("Press the 1 or 2 keys!\n");
	printf("Press ESC to exit.\n");

	while (1)
	{
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

		if (GetAsyncKeyState(0x31))
			cs_insert_sound(ctx, &jump_instance);

		if (GetAsyncKeyState(0x32))
			cs_insert_sound(ctx, &select_instance);

		cs_mix(ctx );
	}

	cs_free_sound(&jump_audio);
}
