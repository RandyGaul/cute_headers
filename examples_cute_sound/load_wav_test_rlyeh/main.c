// create header + implementation
#define CUTE_SOUND_IMPLEMENTATION
#include <cute_sound.h>

#include <stdio.h>

int main()
{
	HWND hwnd = GetConsoleWindow();
	cs_context_t* ctx = cs_make_context(hwnd, 48000, 4092 * 2, 0, NULL);
	cs_loaded_sound_t voice_audio = cs_load_wav("demo.wav");
	cs_playing_sound_t voice_instance = cs_make_playing_sound(&voice_audio);
	printf("demo.wav has a sample rate of %d Hz.\n", voice_audio.sample_rate);

	printf("Press the 1 key or ESC to exit!\n");

	while (1)
	{
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

		if (GetAsyncKeyState(0x31))
			cs_insert_sound(ctx, &voice_instance);

		cs_mix(ctx );
	}

	cs_free_sound(&voice_audio);
}
