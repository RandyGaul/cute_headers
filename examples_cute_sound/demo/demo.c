// only create header symbols
#define STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"

// create header + implementation
#define CUTE_SOUND_IMPLEMENTATION
#include "cute_sound.h"

cs_loaded_sound_t vorbis_loaded;
cs_playing_sound_t vorbis_sound;

void Vorbis(cs_context_t* ctx)
{
	vorbis_loaded = cs_load_ogg("thingy.ogg");
	vorbis_sound = cs_make_playing_sound(&vorbis_loaded);
	cs_set_volume(&vorbis_sound, 0.3f, 0.3f);
	cs_insert_sound(ctx, &vorbis_sound);
}

void LowLevelAPI(cs_context_t* ctx)
{
	cs_loaded_sound_t airlock = cs_load_wav("airlock.wav");
	cs_loaded_sound_t jump = cs_load_wav("../jump.wav");
	cs_playing_sound_t s0 = cs_make_playing_sound(&airlock);
	cs_playing_sound_t s1 = cs_make_playing_sound(&jump);
	cs_insert_sound(ctx, &s0);

	// Play the vorbis song thingy.ogg, or loop airlock
	if (0) cs_loop_sound(&s0, 1);
	else Vorbis(ctx);

	while (1)
	{
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

		if (GetAsyncKeyState(VK_SPACE))
			cs_insert_sound(ctx, &s1);

		cs_mix(ctx);
	}

	cs_free_sound(&vorbis_loaded);
}

float Time()
{
	static int first = 1;
	static LARGE_INTEGER prev;

	LARGE_INTEGER count, freq;
	ULONGLONG diff;
	QueryPerformanceCounter(&count);
	QueryPerformanceFrequency(&freq);

	if (first)
	{
		first = 0;
		prev = count;
	}

	diff = count.QuadPart - prev.QuadPart;
	prev = count;
	return (float)(diff / (double)freq.QuadPart);
}

cs_loaded_sound_t airlock;
cs_loaded_sound_t rupee1;
cs_loaded_sound_t rupee2;

void HighLevelAPI(cs_context_t* ctx, int use_thread)
{
	airlock = cs_load_wav("airlock.wav");
	rupee1 = cs_load_wav("LTTP_Rupee1.wav");
	rupee2 = cs_load_wav("LTTP_Rupee2.wav");
	cs_play_sound_def_t def0 = cs_make_def(&airlock);
	cs_play_sound_def_t def1 = cs_make_def(&rupee1);
	cs_play_sound_def_t def2 = cs_make_def(&rupee2);
	cs_play_sound(ctx, def0);

	while (1)
	{
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

		// if space is pressed play rupee noise and start tracking time
		static int debounce = 1;
		static float time = 0.0f;
		if (GetAsyncKeyState(VK_SPACE) && debounce)
		{
			cs_play_sound(ctx, def1);
			debounce = 0;
		}

		// if we pressed space track time, once we wait long enough go
		// ahead nad play the second rupee noise as long as we are
		// holding space
		if (!debounce)
		{
			time += Time();
			if (time > 0.2f)
			{
				cs_play_sound(ctx, def2);
				time = 0.0f;
			}
		}

		// clear time accumulations when not pressing space
		else Time();

		// if we pressed space but aren't any longer clear all state
		if (!GetAsyncKeyState(VK_SPACE))
		{
			time = 0.0f;
			debounce = 1;
		}

		if (!use_thread)
			cs_mix(ctx);
	}
}

int main()
{
	int frequency = 44000; // a good standard frequency for playing commonly saved OGG + wav files
	int buffered_samples = 8192; // number of samples internal buffers can hold at once
	int use_playing_pool = 1; // non-zero uses high-level API, 0 uses low-level API
	int num_elements_in_playing_pool = use_playing_pool ? 5 : 0; // pooled memory array size for playing sounds
	void* no_allocator_used = NULL; // No custom allocator is used, just pass in NULL here.

	// initializes direct sound and allocate necessary memory
	cs_context_t* ctx = cs_make_context(GetConsoleWindow(), frequency, buffered_samples, num_elements_in_playing_pool, no_allocator_used);

	printf("Press, or press and hold, space to play a sound. Press ESC to exit.\n");

	if (!use_playing_pool)
	{
		// play airlock sound upon startup and loop it (or play the ogg file,
		// change the if statement inside to try each out),
		// press space to play jump sound
		LowLevelAPI(ctx);

		// shuts down direct sound, frees all used context memory
		// does not free any loaded sounds!
		cs_shutdown_context(ctx);
	}

	else
	{
		// cs_mix can be placed onto its own separate thread
		// this can be handy since it performs a bunch of SIMD instructions on the CPU
		int use_thread = 1;

		if (use_thread)
		{
			// be sure to read the docs for these functions in the header!
			cs_spawn_mix_thread(ctx);
			cs_thread_sleep_delay(ctx, 10);
		}

		// Same as LowLevelApi, but uses the internal tsPlayingSound memory
		// pool. This can make it easier for users to generate tsPlayingSound
		// instances without doing manual memory management. Pressing space
		// once quickly will play a rupee noise (5 rupee pickup from ALTTP).
		// If space is held more rupees will keep coming :)
		HighLevelAPI(ctx, use_thread);

		// shuts down direct sound, frees all used context memory
		// does not free any loaded sounds!
		cs_shutdown_context(ctx);

		// Sounds should be freed after cs_shutdown_context in case cs_spawn_mix_thread was used, as the
		// ctx thread may still be reading from loaded sounds prior to calling cs_shutdown_context.
		cs_free_sound(&airlock);
		cs_free_sound(&rupee1);
		cs_free_sound(&rupee2);
	}
}

// create implementation
#undef STB_VORBIS_HEADER_ONLY
#include "stb_vorbis.c"
