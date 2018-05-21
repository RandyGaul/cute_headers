// create header + implementation
#define TINYSOUND_IMPLEMENTATION
#include <tinysound.h>

#include <stdio.h>

int main()
{
	HWND hwnd = GetConsoleWindow();
	tsContext* ctx = tsMakeContext(hwnd, 44100, 15, 5, 0);
	tsLoadedSound voice_audio = tsLoadWAV("demo.wav");
	tsPlayingSound voice_instance = tsMakePlayingSound(&voice_audio);
	printf("demo.wav has a sample rate of %d Hz.\n", voice_audio.sample_rate);

	printf("Press the key or ESC to exit!\n");

	while (1)
	{
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

		if (GetAsyncKeyState(0x31))
			tsInsertSound(ctx, &voice_instance);


		tsMix(ctx );
	}

	tsFreeSound(&voice_audio);
}
