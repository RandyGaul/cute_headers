// create header + implementation
#define TINYSOUND_IMPLEMENTATION
#include "tinysound.h"

#include <stdio.h>

int main()
{
	HWND hwnd = GetConsoleWindow();
	tsContext* ctx = tsMakeContext(hwnd, 44100, 15, 5, 0);
	tsLoadedSound jump_audio = tsLoadWAV("../jump.wav");
	tsLoadedSound select_audio = tsLoadWAV("select.wav");
	tsPlayingSound jump_instance = tsMakePlayingSound(&jump_audio);
	tsPlayingSound select_instance = tsMakePlayingSound(&select_audio);
	printf("jump.wav has a sample rate of %d Hz.\n", jump_audio.sample_rate);
	printf("select.wav has a sample rate of %d Hz.\n", select_audio.sample_rate);

	printf("Press the 1 or 2 keys!\n");
	printf("Press ESC to exit.\n");

	while (1)
	{
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

		if (GetAsyncKeyState(0x31))
			tsInsertSound(ctx, &jump_instance);

		if (GetAsyncKeyState(0x32))
			tsInsertSound(ctx, &select_instance);


		tsMix(ctx );
	}

	tsFreeSound(&jump_audio);
}
