/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_sound.h - v2.08


	To create implementation (the function definitions)
		#define CUTE_SOUND_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file


	SUMMARY

		cute_sound is a C API for loading, playing, looping, panning and fading mono
		and stereo sounds, without any external dependencies other than things that ship
		with standard OSs, or SDL2 for more uncommon OSs.

		While platform detection is done automatically, you are able to explicitly
		specify which to use by defining one of the following:

			#define CUTE_SOUND_PLATFORM_WINDOWS
			#define CUTE_SOUND_PLATFORM_APPLE
			#define CUTE_SOUND_PLATFORM_SDL

		For Windows cute_sound uses DirectSound. Due to the use of SSE intrinsics, MinGW
		builds must be made with the compiler option: -march=native, or optionally SSE
		can be disabled with CUTE_SOUND_SCALAR_MODE. More on this mode written below.

		For Apple machines cute_sound uses CoreAudio.

		For Linux builds cute_sound uses SDL2.

		An SDL2 implementation of cute_sound is available on platforms SDL2 is available,
		which is pretty much everywhere. To use the SDL2 implementation of cute_sound
		define CUTE_SOUND_FORCE_SDL before placing the CUTE_SOUND_IMPLEMENTATION into a
		translation unit in order to force the use of SDL. Here is an example:

			#define CUTE_SOUND_FORCE_SDL
			#define CUTE_SOUND_IMPLEMENTATION
			#include <cute_sound.h>

		If you want to use cute_sound with SDL_RWops, you must enable it by putting this
		before you #include cute_sound.h:

			#define CUTE_SOUND_SDL_RWOPS


	REVISION HISTORY

		1.0  (06/04/2016) Initial release.
		1.01 (06/06/2016) Load WAV from memory.
		                * Separate portable and OS-specific code in cs_mix.
		                * Fixed bug causing audio glitches when sounds ended.
		                * Added stb_vorbis loaders + demo example.
		1.02 (06/08/2016) error checking + strings in vorbis loaders
		                * SSE2 implementation of mixer.
		                * Fix typos on docs/comments.
		                * Corrected volume bug introduced in 1.01.
		1.03 (07/05/2016) size calculation helper (to know size of sound in
		                  bytes on the heap) cs_sound_size
		1.04 (12/06/2016) Merged in Aaron Balint's contributions.
		                * SFFT and pitch functions from Stephan M. Bernsee
		                * cs_mix can run on its own thread with cs_spawn_mix_thread
		                * Updated documentation, typo fixes.
		                * Fixed typo in cs_malloc16 that caused heap corruption.
		1.05 (12/08/2016) cs_stop_all_sounds, suggested by Aaron Balint
		1.06 (02/17/2017) Port to CoreAudio for Apple machines.
		1.07 (06/18/2017) SIMD the pitch shift code; swapped out old Bernsee
		                  code for a new re-write, updated docs as necessary,
		                  support for compiling as .c and .cpp on Windows,
		                  port for SDL (for Linux, or any other platform).
		                * Special thanks to DeXP (Dmitry Hrabrov) for 90% of
		                  the work on the SDL port!
		1.08 (09/06/2017) SDL_RWops support by RobLoach
		1.09 (05/20/2018) Load wav funcs can skip all irrelevant chunks
		                  Ref counting for playing sounds
		1.10 (08/24/2019) Introduced plugin interface, reimplemented pitch shifting
		                  as a plugin, added optional `ctx` to alloc functions
		1.11 (04/23/2020) scalar SIMD mode and various compiler warning/error fixes
		1.12 (10/20/2021) removed old and broken assert if driver requested non-
		                  power of two sample size for mixing updates
		2.00 (05/21/2022) redesigned the entire API for v2.00, music support, broke
		                  the pitch/plugin interface (to be fixed), CoreAudio is not
		                  yet tested, but the SDL2 implementation is well tested,
		                * ALSA support is dropped entirely
		2.01 (11/02/2022) Compilation fixes for clang/llvm, added #include <stddef.h>
		                  to have size_t defined. Correctly finalize the thread when
		                  cs_shutdown is called for all platforms that spawned it
		                  (this fixes segmentation fault on cs_shutdown).
		2.02 (11/10/2022) Removed plugin API -- It was clunky and just not worth the
		                  maintenence effort. If you're reading this Matt, I haven't
		                  forgotten all the cool work you did! And will use it in the
		                  future for sure :) -- Unfortunately this removes pitch.
		                * Fixed a bug where dsound mixing could run too fast.
		2.03 (11/12/2022) Added internal queue for freeing audio sources to avoid the
		                  need for refcount polling.
		2.04 (02/04/2024) Added `cs_cull_duplicates` helper for removing extra plays
		                  to the same sound on the exact same update tick.
		2.05 (03/27/2023) Added cs_get_global_context and friends, and extra accessors
		                  for panning and similar.
		2.06 (06/23/2024) Looping sounds play seamlessly.
		2.07 (06/23/2024) Added pitch shifting support, removed delay support.
		2.08 (08/07/2024) Added sample_index to sound params, removed unnecessary asserts
		                  for stopping music, added callbacks sounds/music ending


	CONTRIBUTORS

		Aaron Balint      1.04 - real time pitch
		                  1.04 - separate thread for cs_mix
		                  1.04 - bugfix, removed extra cs_free16 call for second channel
		DeXP              1.07 - initial work on SDL port
		RobLoach          1.08 - SDL_RWops support
		Matt Rosen        1.10 - Initial experiments with cute_dsp to figure out plugin
		                         interface needs and use-cases
		fluffrabbit       1.11 - scalar SIMD mode and various compiler warning/error fixes
		Daniel Guzman     2.01 - compilation fixes for clang/llvm on MAC. 
		Brie              2.06 - Looping sound rollover
		ogam              x.xx - Lots of bugfixes over time, including support negative pitch
		renex             x.xx - Fixes to popping issues and a crash in the mixer.


	DOCUMENTATION (very quick intro)

		Steps to play audio:

			1. create context (call cs_init)
			2. load sounds from disk into memory (call cs_load_wav, or cs_load_ogg with stb_vorbis.c)
			3. play sounds (cs_play_sound), or music (cs_music_play)

	DISABLE SSE SIMD ACCELERATION

		If for whatever reason you don't want to use SSE intrinsics and instead would prefer
		plain C (for example if your platform does not support SSE) then define
		CUTE_SOUND_SCALAR_MODE before including cute_sound.h while also defining the
		symbol definitions. Here's an example:

			#define CUTE_SOUND_IMPLEMENTATION
			#define CUTE_SOUND_SCALAR_MODE
			#include <cute_sound.h>
	
	CUSTOMIZATION

		A few different macros can be overriden to provide custom functionality. Simply define any of these
		macros before including this file to override them.

			CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES
			CUTE_SOUND_ASSERT
			CUTE_SOUND_ALLOC
			CUTE_SOUND_FREE
			CUTE_SOUND_MEMCPY
			CUTE_SOUND_MEMSET
			CUTE_SOUND_MEMCMP
			CUTE_SOUND_SEEK_SET
			CUTE_SOUND_SEEK_END
			CUTE_SOUND_FILE
			CUTE_SOUND_FOPEN
			CUTE_SOUND_FSEEK
			CUTE_SOUND_FREAD
			CUTE_SOUND_FTELL
			CUTE_SOUND_FCLOSE


	KNOWN LIMITATIONS

		* PCM mono/stereo format is the only formats the LoadWAV function supports. I don't
		  guarantee it will work for all kinds of wav files, but it certainly does for the common
		  kind (and can be changed fairly easily if someone wanted to extend it).
		* Only supports 16 bits per sample.
		* Mixer does not do any fancy clipping. The algorithm is to convert all 16 bit samples
		  to float, mix all samples, and write back to audio API as 16 bit integers. In
		  practice this works very well and clipping is not often a big problem.


	FAQ

		Q : I would like to use my own memory management, how can I achieve this?
		A : This header makes a couple uses of malloc/free, and cs_malloc16/cs_free16. Simply find these bits
		    and replace them with your own memory allocation routines. They can be wrapped up into a macro,
		    or call your own functions directly -- it's up to you. Generally these functions allocate fairly
		    large chunks of memory, and not very often (if at all).

		Q : Does this library support audio streaming? Something like System::createStream in FMOD.
		A : No. Typically music files aren't that large (in the megabytes). Compare this to a typical
		    DXT texture of 1024x1024, at 0.5MB of memory. Now say an average music file for a game is three
		    minutes long. Loading this file into memory and storing it as raw 16bit samples with two channels,
		    would be:

		        num_samples = 3 * 60 * 44100 * 2
		        num_bits = num_samples * 16
		        num_bytes = num_bits / 8
		        num_megabytes = num_bytes / (1024 * 1024)
		        or 30.3mb

		    So say the user has 2gb of spare RAM. That means we could fit 67 different three minute length
		    music files in there simultaneously. That is a ridiculous amount of spare memory. 30mb is nothing
		    nowadays. Just load your music file into memory all at once and then play it.

		Q : But I really need streaming of audio files from disk to save memory! Also loading my audio
		    files (like .OGG) takes a long time (multiple seconds).
		A : It is recommended to either A) load up your music files before they are needed, perhaps during
		    a loading screen, or B) stream in the entire audio into memory on another thread. cs_read_mem_ogg is
		    a great candidate function to throw onto a job pool. Streaming is more a remnant of older machines
		    (like in the 90's or early 2000's) where decoding speed and RAM were real nasty bottlenecks. For
		    more modern machines, these aren't really concerns, even with mobile devices. If even after reading
		    this Q/A section you still want to stream your audio, you can try mini_al as an alternative:
		    https://github.com/dr-soft/mini_al
*/

#if !defined(CUTE_SOUND_H)

#if defined(_WIN32)
	#if !defined _CRT_SECURE_NO_WARNINGS
		#define _CRT_SECURE_NO_WARNINGS
	#endif
	#if !defined _CRT_NONSTDC_NO_DEPRECATE
		#define _CRT_NONSTDC_NO_DEPRECATE
	#endif
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// -------------------------------------------------------------------------------------------------
// Error handling.

typedef enum cs_error_t
{
	CUTE_SOUND_ERROR_NONE,
	CUTE_SOUND_ERROR_IMPLEMENTATION_ERROR_PLEASE_REPORT_THIS_ON_GITHUB, // https://github.com/RandyGaul/cute_headers/issues
	CUTE_SOUND_ERROR_FILE_NOT_FOUND,
	CUTE_SOUND_ERROR_INVALID_SOUND,
	CUTE_SOUND_ERROR_HWND_IS_NULL,
	CUTE_SOUND_ERROR_DIRECTSOUND_CREATE_FAILED,
	CUTE_SOUND_ERROR_CREATESOUNDBUFFER_FAILED,
	CUTE_SOUND_ERROR_SETFORMAT_FAILED,
	CUTE_SOUND_ERROR_AUDIOCOMPONENTFINDNEXT_FAILED,
	CUTE_SOUND_ERROR_AUDIOCOMPONENTINSTANCENEW_FAILED,
	CUTE_SOUND_ERROR_FAILED_TO_SET_STREAM_FORMAT,
	CUTE_SOUND_ERROR_FAILED_TO_SET_RENDER_CALLBACK,
	CUTE_SOUND_ERROR_AUDIOUNITINITIALIZE_FAILED,
	CUTE_SOUND_ERROR_AUDIOUNITSTART_FAILED,
	CUTE_SOUND_ERROR_CANT_OPEN_AUDIO_DEVICE,
	CUTE_SOUND_ERROR_CANT_INIT_SDL_AUDIO,
	CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE,
	CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND,
	CUTE_SOUND_ERROR_WAV_DATA_CHUNK_NOT_FOUND,
	CUTE_SOUND_ERROR_ONLY_PCM_WAV_FILES_ARE_SUPPORTED,
	CUTE_SOUND_ERROR_WAV_ONLY_MONO_OR_STEREO_IS_SUPPORTED,
	CUTE_SOUND_ERROR_WAV_ONLY_16_BITS_PER_SAMPLE_SUPPORTED,
	CUTE_SOUND_ERROR_CANNOT_SWITCH_MUSIC_WHILE_PAUSED,
	CUTE_SOUND_ERROR_CANNOT_CROSSFADE_WHILE_MUSIC_IS_PAUSED,
	CUTE_SOUND_ERROR_CANNOT_FADEOUT_WHILE_MUSIC_IS_PAUSED,
	CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT,
	CUTE_SOUND_ERROR_STB_VORBIS_DECODE_FAILED,
	CUTE_SOUND_ERROR_OGG_UNSUPPORTED_CHANNEL_COUNT,
} cs_error_t;

const char* cs_error_as_string(cs_error_t error);

// -------------------------------------------------------------------------------------------------
// Cute sound context functions.

/**
 * Pass in NULL for `os_handle`, except for the DirectSound backend this should be hwnd.
 * play_frequency_in_Hz depends on your audio file, 44100 seems to be fine.
 * buffered_samples is clamped to be at least 1024.
 */
cs_error_t cs_init(void* os_handle, unsigned play_frequency_in_Hz, int buffered_samples, void* user_allocator_context /* = NULL */);
void cs_shutdown();

/**
 * Call this function once per game-tick.
 */
void cs_update(float dt);
void cs_set_global_volume(float volume_0_to_1);
void cs_set_global_pan(float pan_0_to_1);
void cs_set_global_pause(bool true_for_paused);

/**
 * Spawns a mixing thread dedicated to mixing audio in the background.
 * If you don't call this function mixing will happen on the main-thread when you call `cs_update`.
 */
void cs_spawn_mix_thread();

/**
 * In cases where the mixing thread takes up extra CPU attention doing nothing, you can force
 * it to sleep manually. You can tune this as necessary, but it's probably not necessary for you.
 */
void cs_mix_thread_sleep_delay(int milliseconds);

/**
 * Sometimes useful for dynamic library shenanigans.
 */
void* cs_get_context_ptr();
void cs_set_context_ptr(void* ctx);

// -------------------------------------------------------------------------------------------------
// Loaded sounds.

typedef struct cs_audio_source_t cs_audio_source_t;

cs_audio_source_t* cs_load_wav(const char* path, cs_error_t* err /* = NULL */);
cs_audio_source_t* cs_read_mem_wav(const void* memory, size_t size, cs_error_t* err /* = NULL */);
void cs_free_audio_source(cs_audio_source_t* audio);

// If stb_vorbis was included *before* cute_sound go ahead and create
// some functions for dealing with OGG files.
#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H

	cs_audio_source_t* cs_load_ogg(const char* path, cs_error_t* err /* = NULL */);
	cs_audio_source_t* cs_read_mem_ogg(const void* memory, size_t size, cs_error_t* err /* = NULL */);

#endif

// SDL_RWops specific functions
#if defined(SDL_rwops_h_) && defined(CUTE_SOUND_SDL_RWOPS)

	// Provides the ability to use cs_load_wav with an SDL_RWops object.
	cs_audio_source_t* cs_load_wav_rw(SDL_RWops* context, cs_error_t* err /* = NULL */);

	#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H

		// Provides the ability to use cs_load_ogg with an SDL_RWops object.
		cs_audio_source_t* cs_load_ogg_rw(SDL_RWops* rw, cs_error_t* err /* = NULL */);

	#endif

#endif // SDL_rwops_h_

// -------------------------------------------------------------------------------------------------
// Audio source accessors.

int cs_get_sample_rate(const cs_audio_source_t* audio);
int cs_get_sample_count(const cs_audio_source_t* audio);
int cs_get_channel_count(const cs_audio_source_t* audio);

// -------------------------------------------------------------------------------------------------
// Music sounds.

void cs_music_play(cs_audio_source_t* audio, float fade_in_time /* = 0 */);
void cs_music_stop(float fade_out_time /* = 0 */);
void cs_music_pause();
void cs_music_resume();
void cs_music_set_volume(float volume_0_to_1);
void cs_music_set_pitch(float pitch /* = 1.0f */);
void cs_music_set_loop(bool true_to_loop);
void cs_music_switch_to(cs_audio_source_t* audio, float fade_out_time /* = 0 */, float fade_in_time /* = 0 */);
void cs_music_crossfade(cs_audio_source_t* audio, float cross_fade_time /* = 0 */);
int cs_music_get_sample_index();
cs_error_t cs_music_set_sample_index(int sample_index);

// -------------------------------------------------------------------------------------------------
// Playing sounds.

typedef struct cs_playing_sound_t { uint64_t id; } cs_playing_sound_t;
#define CUTE_PLAYING_SOUND_INVALID (cs_playing_sound_t){ 0 }

typedef struct cs_sound_params_t
{
	bool paused  /* = false */;
	bool looped  /* = false */;
	float volume /* = 1.0f */;
	float pan    /* = 0.5f */; // Can be from 0 to 1.
	float pitch  /* = 1.0f */;
	int sample_index /* = 0 */;
} cs_sound_params_t;

cs_sound_params_t cs_sound_params_default();

cs_playing_sound_t cs_play_sound(cs_audio_source_t* audio, cs_sound_params_t params);

/**
 * Setup a callback for whenever a sound finishes playing. This will get called from the
 * mixer thread, which means you'll need to deal with a multithreaded callback if you've
 * spawned a separate mixing thread.
 */
void cs_on_sound_finished_callback(void (*on_finish)(cs_playing_sound_t, void*), void* udata);

/**
 * Setup a callback for whenever the current song finishes playing. This will get called from the
 * mixer thread, which means you'll need to deal with a multithreaded callback if you've
 * spawned a separate mixing thread.
 */
void cs_on_music_finished_callback(void (*on_finish)(void*), void* udata);

bool cs_sound_is_active(cs_playing_sound_t sound);
bool cs_sound_get_is_paused(cs_playing_sound_t sound);
bool cs_sound_get_is_looped(cs_playing_sound_t sound);
float cs_sound_get_volume(cs_playing_sound_t sound);
float cs_sound_get_pitch(cs_playing_sound_t sound);
float cs_sound_get_pan(cs_playing_sound_t sound);
int cs_sound_get_sample_index(cs_playing_sound_t sound);
void cs_sound_set_is_paused(cs_playing_sound_t sound, bool true_for_paused);
void cs_sound_set_is_looped(cs_playing_sound_t sound, bool true_for_looped);
void cs_sound_set_volume(cs_playing_sound_t sound, float volume_0_to_1);
void cs_sound_set_pan(cs_playing_sound_t sound, float pan_0_to_1);
void cs_sound_set_pitch(cs_playing_sound_t sound, float pitch);
cs_error_t cs_sound_set_sample_index(cs_playing_sound_t sound, int sample_index);
void cs_sound_stop(cs_playing_sound_t sound);

void cs_set_playing_sounds_volume(float volume_0_to_1);
void cs_stop_all_playing_sounds();

/**
 * Off by default. When enabled only one instance of audio can be created per audio update-tick. This
 * does *not* take into account the starting sample index, so disable this feature if you want to spawn
 * audio with dynamic sample indices, such as when syncing programmatically generated scores/sequences.
 * This also does not take into account pitch differences.
 */
void cs_cull_duplicates(bool true_to_enable);

// -------------------------------------------------------------------------------------------------
// Global context.

void* cs_get_global_context();
void cs_set_global_context(void* context);

void* cs_get_global_user_allocator_context();
void cs_set_global_user_allocator_context(void* user_allocator_context);

#define CUTE_SOUND_H
#endif

#ifdef CUTE_SOUND_IMPLEMENTATION
#ifndef CUTE_SOUND_IMPLEMENTATION_ONCE
#define CUTE_SOUND_IMPLEMENTATION_ONCE

#ifndef CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES
#	define CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES 1024
#endif

#if !defined(CUTE_SOUND_ASSERT)
#	include <assert.h>
#	define CUTE_SOUND_ASSERT assert
#endif

#if !defined(CUTE_SOUND_ALLOC)
	#include <stdlib.h>
	#define CUTE_SOUND_ALLOC(size, ctx) malloc(size)
#endif

#if !defined(CUTE_SOUND_FREE)
	#include <stdlib.h>
	#define CUTE_SOUND_FREE(mem, ctx) free(mem)
#endif

#ifndef CUTE_SOUND_MEMCPY
#	include <string.h>
#	define CUTE_SOUND_MEMCPY memcpy
#endif

#ifndef CUTE_SOUND_MEMSET
#	include <string.h>
#	define CUTE_SOUND_MEMSET memset
#endif

#ifndef CUTE_SOUND_MEMCMP
#	include <string.h>
#	define CUTE_SOUND_MEMCMP memcmp
#endif

#ifndef CUTE_SOUND_SEEK_SET
#	include <stdio.h>
#	define CUTE_SOUND_SEEK_SET SEEK_SET
#endif

#ifndef CUTE_SOUND_SEEK_END
#	include <stdio.h>
#	define CUTE_SOUND_SEEK_END SEEK_END
#endif

#ifndef CUTE_SOUND_FILE
#	include <stdio.h>
#	define CUTE_SOUND_FILE FILE
#endif

#ifndef CUTE_SOUND_FOPEN
#	include <stdio.h>
#	define CUTE_SOUND_FOPEN fopen
#endif

#ifndef CUTE_SOUND_FSEEK
#	include <stdio.h>
#	define CUTE_SOUND_FSEEK fseek
#endif

#ifndef CUTE_SOUND_FREAD
#	include <stdio.h>
#	define CUTE_SOUND_FREAD fread
#endif

#ifndef CUTE_SOUND_FTELL
#	include <stdio.h>
#	define CUTE_SOUND_FTELL ftell
#endif

#ifndef CUTE_SOUND_FCLOSE
#	include <stdio.h>
#	define CUTE_SOUND_FCLOSE fclose
#endif

// Platform detection.
#define CUTE_SOUND_WINDOWS 1
#define CUTE_SOUND_APPLE   2
#define CUTE_SOUND_SDL     3

// Use CUTE_SOUND_FORCE_SDL as a way to force CUTE_SOUND_PLATFORM_SDL.
#ifdef CUTE_SOUND_FORCE_SDL
	#define CUTE_SOUND_PLATFORM_SDL
#endif

#ifndef CUTE_SOUND_PLATFORM
	// Check the specific platform defines.
	#ifdef CUTE_SOUND_PLATFORM_WINDOWS
		#define CUTE_SOUND_PLATFORM CUTE_SOUND_WINDOWS
	#elif defined(CUTE_SOUND_PLATFORM_APPLE)
		#define CUTE_SOUND_PLATFORM CUTE_SOUND_APPLE
	#elif defined(CUTE_SOUND_PLATFORM_SDL)
		#define CUTE_SOUND_PLATFORM CUTE_SOUND_SDL
	#else
		// Detect the platform automatically.
		#if defined(_WIN32)
			#if !defined _CRT_SECURE_NO_WARNINGS
				#define _CRT_SECURE_NO_WARNINGS
			#endif
			#if !defined _CRT_NONSTDC_NO_DEPRECATE
				#define _CRT_NONSTDC_NO_DEPRECATE
			#endif
			#define CUTE_SOUND_PLATFORM CUTE_SOUND_WINDOWS
		#elif defined(__APPLE__)
			#define CUTE_SOUND_PLATFORM CUTE_SOUND_APPLE
		#else
			// Just use SDL on other esoteric platforms.
			#define CUTE_SOUND_PLATFORM CUTE_SOUND_SDL
		#endif
	#endif
#endif

// Platform specific file inclusions.
#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS


	#ifndef _WINDOWS_
		#ifndef WIN32_LEAN_AND_MEAN
			#define WIN32_LEAN_AND_MEAN
		#endif
		#include <windows.h>
	#endif

	#ifndef _WAVEFORMATEX_
		#include <mmreg.h>
		#include <mmsystem.h>
	#endif

	#include <dsound.h>
	#undef PlaySound

	#ifdef _MSC_VER
		#pragma comment(lib, "dsound.lib")
	#endif

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE

	#include <CoreAudio/CoreAudio.h>
	#include <AudioUnit/AudioUnit.h>
	#include <pthread.h>
	#include <mach/mach_time.h>

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL
	
	#ifndef SDL_h_
		// Define CUTE_SOUND_SDL_H to allow changing the SDL.h path.
		#ifndef CUTE_SOUND_SDL_H
			#define CUTE_SOUND_SDL_H <SDL.h>
		#endif
		#include CUTE_SOUND_SDL_H
	#endif
	#ifndef _WIN32
		#include <alloca.h>
	#endif

#else

	#error Unsupported platform - please choose one of CUTE_SOUND_WINDOWS, CUTE_SOUND_APPLE, CUTE_SOUND_SDL.

#endif

#ifdef CUTE_SOUND_SCALAR_MODE

	#include <limits.h>

	#define CUTE_SOUND_SATURATE16(X) (int16_t)((X) > SHRT_MAX ? SHRT_MAX : ((X) < SHRT_MIN ? SHRT_MIN : (X)))

	typedef struct cs__m128
	{
		float a, b, c, d;
	} cs__m128;

	typedef struct cs__m128i
	{
		int32_t a, b, c, d;
	} cs__m128i;

	cs__m128 cs_mm_set_ps(float e3, float e2, float e1, float e0)
	{
		cs__m128 a;
		a.a = e0;
		a.b = e1;
		a.c = e2;
		a.d = e3;
		return a;
	}

	cs__m128 cs_mm_set1_ps(float e)
	{
		cs__m128 a;
		a.a = e;
		a.b = e;
		a.c = e;
		a.d = e;
		return a;
	}

	cs__m128 cs_mm_load_ps(float const* mem_addr)
	{
		cs__m128 a;
		a.a = mem_addr[0];
		a.b = mem_addr[1];
		a.c = mem_addr[2];
		a.d = mem_addr[3];
		return a;
	}

	cs__m128 cs_mm_add_ps(cs__m128 a, cs__m128 b)
	{
		cs__m128 c;
		c.a = a.a + b.a;
		c.b = a.b + b.b;
		c.c = a.c + b.c;
		c.d = a.d + b.d;
		return c;
	}

	cs__m128 cs_mm_sub_ps(cs__m128 a, cs__m128 b)
	{
		cs__m128 c;
		c.a = a.a - b.a;
		c.b = a.b - b.b;
		c.c = a.c - b.c;
		c.d = a.d - b.d;
		return c;
	}

	cs__m128 cs_mm_mul_ps(cs__m128 a, cs__m128 b)
	{
		cs__m128 c;
		c.a = a.a * b.a;
		c.b = a.b * b.b;
		c.c = a.c * b.c;
		c.d = a.d * b.d;
		return c;
	}

	cs__m128i cs_mm_cvtps_epi32(cs__m128 a)
	{
		cs__m128i b;
		b.a = (int32_t)a.a;
		b.b = (int32_t)a.b;
		b.c = (int32_t)a.c;
		b.d = (int32_t)a.d;
		return b;
	}

	cs__m128i cs_mm_unpacklo_epi32(cs__m128i a, cs__m128i b)
	{
		cs__m128i c;
		c.a = a.a;
		c.b = b.a;
		c.c = a.b;
		c.d = b.b;
		return c;
	}

	cs__m128i cs_mm_unpackhi_epi32(cs__m128i a, cs__m128i b)
	{
		cs__m128i c;
		c.a = a.c;
		c.b = b.c;
		c.c = a.d;
		c.d = b.d;
		return c;
	}

	cs__m128i cs_mm_packs_epi32(cs__m128i a, cs__m128i b)
	{
		union {
			int16_t c[8];
			cs__m128i m;
		} dst;
		dst.c[0] = CUTE_SOUND_SATURATE16(a.a);
		dst.c[1] = CUTE_SOUND_SATURATE16(a.b);
		dst.c[2] = CUTE_SOUND_SATURATE16(a.c);
		dst.c[3] = CUTE_SOUND_SATURATE16(a.d);
		dst.c[4] = CUTE_SOUND_SATURATE16(b.a);
		dst.c[5] = CUTE_SOUND_SATURATE16(b.b);
		dst.c[6] = CUTE_SOUND_SATURATE16(b.c);
		dst.c[7] = CUTE_SOUND_SATURATE16(b.d);
		return dst.m;
	}

	cs__m128i cs_mm_cvttps_epi32(cs__m128 a)
	{
		cs__m128i b;
		b.a = (int32_t)a.a;
		b.b = (int32_t)a.b;
		b.c = (int32_t)a.c;
		b.d = (int32_t)a.d;
		return b;
	}

	cs__m128 cs_mm_cvtepi32_ps(cs__m128i a)
	{
		cs__m128 b;
		b.a = (float)a.a;
		b.b = (float)a.b;
		b.c = (float)a.c;
		b.d = (float)a.d;
		return b;
	}

	int32_t cs_mm_extract_epi32(cs__m128i a, const int imm8)
	{
		switch (imm8) {
			case 0: return a.a;
			case 1: return a.b;
			case 2: return a.c;
			case 3: return a.d;
			default: return 0;
		}
	}

#else // CUTE_SOUND_SCALAR_MODE

	#include <xmmintrin.h>
	#include <emmintrin.h>

	#define cs__m128 __m128
	#define cs__m128i __m128i

	#define cs_mm_set_ps _mm_set_ps
	#define cs_mm_set1_ps _mm_set1_ps
	#define cs_mm_add_ps _mm_add_ps
	#define cs_mm_sub_ps _mm_sub_ps
	#define cs_mm_mul_ps _mm_mul_ps
	#define cs_mm_cvtps_epi32 _mm_cvtps_epi32
	#define cs_mm_unpacklo_epi32 _mm_unpacklo_epi32
	#define cs_mm_unpackhi_epi32 _mm_unpackhi_epi32
	#define cs_mm_packs_epi32 _mm_packs_epi32
	#define cs_mm_cvttps_epi32 _mm_cvttps_epi32
	#define cs_mm_cvtepi32_ps _mm_cvtepi32_ps
	#define cs_mm_extract_epi32 _mm_extract_epi32

#endif // CUTE_SOUND_SCALAR_MODE

#define CUTE_SOUND_ALIGN(X, Y) ((((size_t)X) + ((Y) - 1)) & ~((Y) - 1))
#define CUTE_SOUND_TRUNC(X, Y) ((size_t)(X) & ~((Y) - 1))

// -------------------------------------------------------------------------------------------------
// hashtable.h implementation by Mattias Gustavsson
// See: http://www.mattiasgustavsson.com/ and https://github.com/mattiasgustavsson/libs/blob/master/hashtable.h
// begin hashtable.h

#ifndef CS_HASHTABLEMEMSET
	#define CS_HASHTABLEMEMSET(ptr, val, n) CUTE_SOUND_MEMSET(ptr, val, n)
#endif

#ifndef CS_HASHTABLEMEMCPY
	#define CS_HASHTABLEMEMCPY(dst, src, n) CUTE_SOUND_MEMCPY(dst, src, n)
#endif

#ifndef CS_HASHTABLEMALLOC
	#define CS_HASHTABLEMALLOC(ctx, size) CUTE_SOUND_ALLOC(size, ctx)
#endif

#ifndef CS_HASHTABLEFREE
	#define CS_HASHTABLEFREE(ctx, ptr) CUTE_SOUND_FREE(ptr, ctx)
#endif

/*
------------------------------------------------------------------------------
          Licensing information can be found at the end of the file.
------------------------------------------------------------------------------
hashtable.h - v1.1 - Cache efficient hash table implementation for C/C++.
Do this:
    #define CS_HASHTABLEIMPLEMENTATION
before you include this file in *one* C/C++ file to create the implementation.
*/

#ifndef cs_hashtableh
#define cs_hashtableh

#ifndef CS_HASHTABLEU64
    #define CS_HASHTABLEU64 unsigned long long
#endif

typedef struct cs_hashtablet cs_hashtablet;

static void cs_hashtableinit( cs_hashtablet* table, int item_size, int initial_capacity, void* memctx );
static void cs_hashtableterm( cs_hashtablet* table );

static void* cs_hashtableinsert( cs_hashtablet* table, CS_HASHTABLEU64 key, void const* item );
static void cs_hashtableremove( cs_hashtablet* table, CS_HASHTABLEU64 key );
static void cs_hashtableclear( cs_hashtablet* table );

static void* cs_hashtablefind( cs_hashtablet const* table, CS_HASHTABLEU64 key );

static int cs_hashtablecount( cs_hashtablet const* table );
static void* cs_hashtableitems( cs_hashtablet const* table );
static CS_HASHTABLEU64 const* cs_hashtablekeys( cs_hashtablet const* table );

static void cs_hashtableswap( cs_hashtablet* table, int index_a, int index_b );


#endif /* cs_hashtableh */

/*
----------------------
    IMPLEMENTATION
----------------------
*/

#ifndef cs_hashtablet_h
#define cs_hashtablet_h

#ifndef CS_HASHTABLEU32
    #define CS_HASHTABLEU32 unsigned int
#endif

struct cs_hashtableinternal_slot_t
    {
    CS_HASHTABLEU32 key_hash;
    int item_index;
    int base_count;
    };

struct cs_hashtablet
    {
    void* memctx;
    int count;
    int item_size;

    struct cs_hashtableinternal_slot_t* slots;
    int slot_capacity;

    CS_HASHTABLEU64* items_key;
    int* items_slot;
    void* items_data;
    int item_capacity;

    void* swap_temp;
    };

#endif /* cs_hashtablet_h */

// end hashtable.h

#define CS_HASHTABLEIMPLEMENTATION

#ifdef CS_HASHTABLEIMPLEMENTATION
#ifndef CS_HASHTABLEIMPLEMENTATION_ONCE
#define CS_HASHTABLEIMPLEMENTATION_ONCE

// hashtable.h implementation by Mattias Gustavsson
// See: http://www.mattiasgustavsson.com/ and https://github.com/mattiasgustavsson/libs/blob/master/hashtable.h
// begin hashtable.h (continuing from first time)

#ifndef CS_HASHTABLESIZE_T
    #include <stddef.h>
    #define CS_HASHTABLESIZE_T size_t
#endif

#ifndef CS_HASHTABLEASSERT
    #include <assert.h>
    #define CS_HASHTABLEASSERT( x ) assert( x )
#endif

#ifndef CS_HASHTABLEMEMSET
    #include <string.h>
    #define CS_HASHTABLEMEMSET( ptr, val, cnt ) ( memset( ptr, val, cnt ) )
#endif 

#ifndef CS_HASHTABLEMEMCPY
    #include <string.h>
    #define CS_HASHTABLEMEMCPY( dst, src, cnt ) ( memcpy( dst, src, cnt ) )
#endif 

#ifndef CS_HASHTABLEMALLOC
    #include <stdlib.h>
    #define CS_HASHTABLEMALLOC( ctx, size ) ( malloc( size ) )
    #define CS_HASHTABLEFREE( ctx, ptr ) ( free( ptr ) )
#endif


static CS_HASHTABLEU32 cs_hashtableinternal_pow2ceil( CS_HASHTABLEU32 v )
    {
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    ++v;
    v += ( v == 0 );
    return v;
    }


static void cs_hashtableinit( cs_hashtablet* table, int item_size, int initial_capacity, void* memctx )
    {
    initial_capacity = (int)cs_hashtableinternal_pow2ceil( initial_capacity >=0 ? (CS_HASHTABLEU32) initial_capacity : 32U );
    table->memctx = memctx;
    table->count = 0;
    table->item_size = item_size;
    table->slot_capacity = (int) cs_hashtableinternal_pow2ceil( (CS_HASHTABLEU32) ( initial_capacity + initial_capacity / 2 ) );
    int slots_size = (int)( table->slot_capacity * sizeof( *table->slots ) );
    table->slots = (struct cs_hashtableinternal_slot_t*) CS_HASHTABLEMALLOC( table->memctx, (CS_HASHTABLESIZE_T) slots_size );
    CS_HASHTABLEASSERT( table->slots );
    CS_HASHTABLEMEMSET( table->slots, 0, (CS_HASHTABLESIZE_T) slots_size );
    table->item_capacity = (int) cs_hashtableinternal_pow2ceil( (CS_HASHTABLEU32) initial_capacity );
    table->items_key = (CS_HASHTABLEU64*) CS_HASHTABLEMALLOC( table->memctx,
        table->item_capacity * ( sizeof( *table->items_key ) + sizeof( *table->items_slot ) + table->item_size ) + table->item_size );
    CS_HASHTABLEASSERT( table->items_key );
    table->items_slot = (int*)( table->items_key + table->item_capacity );
    table->items_data = (void*)( table->items_slot + table->item_capacity );
    table->swap_temp = (void*)( ( (uintptr_t) table->items_data ) + table->item_size * table->item_capacity ); 
    }


static void cs_hashtableterm( cs_hashtablet* table )
    {
    CS_HASHTABLEFREE( table->memctx, table->items_key );
    CS_HASHTABLEFREE( table->memctx, table->slots );
    }


// from https://gist.github.com/badboy/6267743
static CS_HASHTABLEU32 cs_hashtableinternal_calculate_hash( CS_HASHTABLEU64 key )
    {
    key = ( ~key ) + ( key << 18 );
    key = key ^ ( key >> 31 );
    key = key * 21;
    key = key ^ ( key >> 11 );
    key = key + ( key << 6 );
    key = key ^ ( key >> 22 );  
    CS_HASHTABLEASSERT( key );
    return (CS_HASHTABLEU32) key;
    }


static int cs_hashtableinternal_find_slot( cs_hashtablet const* table, CS_HASHTABLEU64 key )
    {
    int const slot_mask = table->slot_capacity - 1;
    CS_HASHTABLEU32 const hash = cs_hashtableinternal_calculate_hash( key );

    int const base_slot = (int)( hash & (CS_HASHTABLEU32)slot_mask );
    int base_count = table->slots[ base_slot ].base_count;
    int slot = base_slot;

    while( base_count > 0 )
        {
        CS_HASHTABLEU32 slot_hash = table->slots[ slot ].key_hash;
        if( slot_hash )
            {
            int slot_base = (int)( slot_hash & (CS_HASHTABLEU32)slot_mask );
            if( slot_base == base_slot ) 
                {
                CS_HASHTABLEASSERT( base_count > 0 );
                --base_count;
                if( slot_hash == hash && table->items_key[ table->slots[ slot ].item_index ] == key )
                    return slot;
                }
            }
        slot = ( slot + 1 ) & slot_mask;
        }   

    return -1;
    }


static void cs_hashtableinternal_expand_slots( cs_hashtablet* table )
    {
    int const old_capacity = table->slot_capacity;
    struct cs_hashtableinternal_slot_t* old_slots = table->slots;

    table->slot_capacity *= 2;
    int const slot_mask = table->slot_capacity - 1;

    int const size = (int)( table->slot_capacity * sizeof( *table->slots ) );
    table->slots = (struct cs_hashtableinternal_slot_t*) CS_HASHTABLEMALLOC( table->memctx, (CS_HASHTABLESIZE_T) size );
    CS_HASHTABLEASSERT( table->slots );
    CS_HASHTABLEMEMSET( table->slots, 0, (CS_HASHTABLESIZE_T) size );

    for( int i = 0; i < old_capacity; ++i )
        {
        CS_HASHTABLEU32 const hash = old_slots[ i ].key_hash;
        if( hash )
            {
            int const base_slot = (int)( hash & (CS_HASHTABLEU32)slot_mask );
            int slot = base_slot;
            while( table->slots[ slot ].key_hash )
                slot = ( slot + 1 ) & slot_mask;
            table->slots[ slot ].key_hash = hash;
            int item_index = old_slots[ i ].item_index;
            table->slots[ slot ].item_index = item_index;
            table->items_slot[ item_index ] = slot; 
            ++table->slots[ base_slot ].base_count;
            }               
        }

    CS_HASHTABLEFREE( table->memctx, old_slots );
    }


static void cs_hashtableinternal_expand_items( cs_hashtablet* table )
    {
    table->item_capacity *= 2;
     CS_HASHTABLEU64* const new_items_key = (CS_HASHTABLEU64*) CS_HASHTABLEMALLOC( table->memctx, 
         table->item_capacity * ( sizeof( *table->items_key ) + sizeof( *table->items_slot ) + table->item_size ) + table->item_size);
    CS_HASHTABLEASSERT( new_items_key );

    int* const new_items_slot = (int*)( new_items_key + table->item_capacity );
    void* const new_items_data = (void*)( new_items_slot + table->item_capacity );
    void* const new_swap_temp = (void*)( ( (uintptr_t) new_items_data ) + table->item_size * table->item_capacity ); 

    CS_HASHTABLEMEMCPY( new_items_key, table->items_key, table->count * sizeof( *table->items_key ) );
    CS_HASHTABLEMEMCPY( new_items_slot, table->items_slot, table->count * sizeof( *table->items_key ) );
    CS_HASHTABLEMEMCPY( new_items_data, table->items_data, (CS_HASHTABLESIZE_T) table->count * table->item_size );
    
    CS_HASHTABLEFREE( table->memctx, table->items_key );

    table->items_key = new_items_key;
    table->items_slot = new_items_slot;
    table->items_data = new_items_data;
    table->swap_temp = new_swap_temp;
    }


static void* cs_hashtableinsert( cs_hashtablet* table, CS_HASHTABLEU64 key, void const* item )
    {
    CS_HASHTABLEASSERT( cs_hashtableinternal_find_slot( table, key ) < 0 );

    if( table->count >= ( table->slot_capacity - table->slot_capacity / 3 ) )
        cs_hashtableinternal_expand_slots( table );
        
    int const slot_mask = table->slot_capacity - 1;
    CS_HASHTABLEU32 const hash = cs_hashtableinternal_calculate_hash( key );

    int const base_slot = (int)( hash & (CS_HASHTABLEU32)slot_mask );
    int base_count = table->slots[ base_slot ].base_count;
    int slot = base_slot;
    int first_free = slot;
    while( base_count )
        {
        CS_HASHTABLEU32 const slot_hash = table->slots[ slot ].key_hash;
        if( slot_hash == 0 && table->slots[ first_free ].key_hash != 0 ) first_free = slot;
        int slot_base = (int)( slot_hash & (CS_HASHTABLEU32)slot_mask );
        if( slot_base == base_slot ) 
            --base_count;
        slot = ( slot + 1 ) & slot_mask;
        }       

    slot = first_free;
    while( table->slots[ slot ].key_hash )
        slot = ( slot + 1 ) & slot_mask;

    if( table->count >= table->item_capacity )
        cs_hashtableinternal_expand_items( table );

    CS_HASHTABLEASSERT( !table->slots[ slot ].key_hash && ( hash & (CS_HASHTABLEU32) slot_mask ) == (CS_HASHTABLEU32) base_slot );
    CS_HASHTABLEASSERT( hash );
    table->slots[ slot ].key_hash = hash;
    table->slots[ slot ].item_index = table->count;
    ++table->slots[ base_slot ].base_count;


    void* dest_item = (void*)( ( (uintptr_t) table->items_data ) + table->count * table->item_size );
    CS_HASHTABLEMEMCPY( dest_item, item, (CS_HASHTABLESIZE_T) table->item_size );
    table->items_key[ table->count ] = key;
    table->items_slot[ table->count ] = slot;
    ++table->count;
    return dest_item;
    } 


static void cs_hashtableremove( cs_hashtablet* table, CS_HASHTABLEU64 key )
    {
    int const slot = cs_hashtableinternal_find_slot( table, key );
    CS_HASHTABLEASSERT( slot >= 0 );

    int const slot_mask = table->slot_capacity - 1;
    CS_HASHTABLEU32 const hash = table->slots[ slot ].key_hash;
    int const base_slot = (int)( hash & (CS_HASHTABLEU32) slot_mask );
    CS_HASHTABLEASSERT( hash );
    --table->slots[ base_slot ].base_count;
    table->slots[ slot ].key_hash = 0;

    int index = table->slots[ slot ].item_index;
    int last_index = table->count - 1;
    if( index != last_index )
        {
        table->items_key[ index ] = table->items_key[ last_index ];
        table->items_slot[ index ] = table->items_slot[ last_index ];
        void* dst_item = (void*)( ( (uintptr_t) table->items_data ) + index * table->item_size );
        void* src_item = (void*)( ( (uintptr_t) table->items_data ) + last_index * table->item_size );
        CS_HASHTABLEMEMCPY( dst_item, src_item, (CS_HASHTABLESIZE_T) table->item_size );
        table->slots[ table->items_slot[ last_index ] ].item_index = index;
        }
    --table->count;
    } 


static void cs_hashtableclear( cs_hashtablet* table )
    {
    table->count = 0;
    CS_HASHTABLEMEMSET( table->slots, 0, table->slot_capacity * sizeof( *table->slots ) );
    }


static void* cs_hashtablefind( cs_hashtablet const* table, CS_HASHTABLEU64 key )
    {
    int const slot = cs_hashtableinternal_find_slot( table, key );
    if( slot < 0 ) return 0;

    int const index = table->slots[ slot ].item_index;
    void* const item = (void*)( ( (uintptr_t) table->items_data ) + index * table->item_size );
    return item;
    }


static int cs_hashtablecount( cs_hashtablet const* table )
    {
    return table->count;
    }


static void* cs_hashtableitems( cs_hashtablet const* table )
    {
    return table->items_data;
    }


static CS_HASHTABLEU64 const* cs_hashtablekeys( cs_hashtablet const* table )
    {
    return table->items_key;
    }


static void cs_hashtableswap( cs_hashtablet* table, int index_a, int index_b )
    {
    if( index_a < 0 || index_a >= table->count || index_b < 0 || index_b >= table->count ) return;

    int slot_a = table->items_slot[ index_a ];
    int slot_b = table->items_slot[ index_b ];

    table->items_slot[ index_a ] = slot_b;
    table->items_slot[ index_b ] = slot_a;

    CS_HASHTABLEU64 temp_key = table->items_key[ index_a ];
    table->items_key[ index_a ] = table->items_key[ index_b ];
    table->items_key[ index_b ] = temp_key;

    void* item_a = (void*)( ( (uintptr_t) table->items_data ) + index_a * table->item_size );
    void* item_b = (void*)( ( (uintptr_t) table->items_data ) + index_b * table->item_size );
    CS_HASHTABLEMEMCPY( table->swap_temp, item_a, table->item_size );
    CS_HASHTABLEMEMCPY( item_a, item_b, table->item_size );
    CS_HASHTABLEMEMCPY( item_b, table->swap_temp, table->item_size );

    table->slots[ slot_a ].item_index = index_b;
    table->slots[ slot_b ].item_index = index_a;
    }


#endif /* CS_HASHTABLEIMPLEMENTATION */
#endif // CS_HASHTABLEIMPLEMENTATION_ONCE

/*
contributors:
    Randy Gaul (cs_hashtableclear, cs_hashtableswap )
revision history:
    1.1     added cs_hashtableclear, cs_hashtableswap
    1.0     first released version  
*/

/*
------------------------------------------------------------------------------
This software is available under 2 licenses - you may choose the one you like.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2015 Mattias Gustavsson
Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this 
software, either in source code form or as a compiled binary, for any purpose, 
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this 
software dedicate any and all copyright interest in the software to the public 
domain. We make this dedication for the benefit of the public at large and to 
the detriment of our heirs and successors. We intend this dedication to be an 
overt act of relinquishment in perpetuity of all present and future rights to 
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/

// end of hashtable.h

// -------------------------------------------------------------------------------------------------
// Doubly list.

typedef struct cs_list_node_t
{
	struct cs_list_node_t* next /* = this */;
	struct cs_list_node_t* prev /* = this */;
} cs_list_node_t;

typedef struct cs_list_t
{
	cs_list_node_t nodes;
} cs_list_t;

#define CUTE_SOUND_OFFSET_OF(T, member) ((size_t)((uintptr_t)(&(((T*)0)->member))))
#define CUTE_SOUND_LIST_NODE(T, member, ptr) ((cs_list_node_t*)((uintptr_t)ptr + CUTE_SOUND_OFFSET_OF(T, member)))
#define CUTE_SOUND_LIST_HOST(T, member, ptr) ((T*)((uintptr_t)ptr - CUTE_SOUND_OFFSET_OF(T, member)))

void cs_list_init_node(cs_list_node_t* node)
{
	node->next = node;
	node->prev = node;
}

void cs_list_init(cs_list_t* list)
{
	cs_list_init_node(&list->nodes);
}

void cs_list_push_front(cs_list_t* list, cs_list_node_t* node)
{
	node->next = list->nodes.next;
	node->prev = &list->nodes;
	list->nodes.next->prev = node;
	list->nodes.next = node;
}

void cs_list_push_back(cs_list_t* list, cs_list_node_t* node)
{
	node->prev = list->nodes.prev;
	node->next = &list->nodes;
	list->nodes.prev->next = node;
	list->nodes.prev = node;
}

void cs_list_remove(cs_list_node_t* node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
	cs_list_init_node(node);
}

cs_list_node_t* cs_list_pop_front(cs_list_t* list)
{
	cs_list_node_t* node = list->nodes.next;
	cs_list_remove(node);
	return node;
}

cs_list_node_t* cs_list_pop_back(cs_list_t* list)
{
	cs_list_node_t* node = list->nodes.prev;
	cs_list_remove(node);
	return node;
}

int cs_list_empty(cs_list_t* list)
{
	return list->nodes.next == list->nodes.prev && list->nodes.next == &list->nodes;
}

cs_list_node_t* cs_list_begin(cs_list_t* list)
{
	return list->nodes.next;
}

cs_list_node_t* cs_list_end(cs_list_t* list)
{
	return &list->nodes;
}

cs_list_node_t* cs_list_front(cs_list_t* list)
{
	return list->nodes.next;
}

cs_list_node_t* cs_list_back(cs_list_t* list)
{
	return list->nodes.prev;
}

// -------------------------------------------------------------------------------------------------

const char* cs_error_as_string(cs_error_t error) {
	switch (error) {
	case CUTE_SOUND_ERROR_NONE: return "CUTE_SOUND_ERROR_NONE";
	case CUTE_SOUND_ERROR_IMPLEMENTATION_ERROR_PLEASE_REPORT_THIS_ON_GITHUB: return "CUTE_SOUND_ERROR_IMPLEMENTATION_ERROR_PLEASE_REPORT_THIS_ON_GITHUB";
	case CUTE_SOUND_ERROR_FILE_NOT_FOUND: return "CUTE_SOUND_ERROR_FILE_NOT_FOUND";
	case CUTE_SOUND_ERROR_INVALID_SOUND: return "CUTE_SOUND_ERROR_INVALID_SOUND";
	case CUTE_SOUND_ERROR_HWND_IS_NULL: return "CUTE_SOUND_ERROR_HWND_IS_NULL";
	case CUTE_SOUND_ERROR_DIRECTSOUND_CREATE_FAILED: return "CUTE_SOUND_ERROR_DIRECTSOUND_CREATE_FAILED";
	case CUTE_SOUND_ERROR_CREATESOUNDBUFFER_FAILED: return "CUTE_SOUND_ERROR_CREATESOUNDBUFFER_FAILED";
	case CUTE_SOUND_ERROR_SETFORMAT_FAILED: return "CUTE_SOUND_ERROR_SETFORMAT_FAILED";
	case CUTE_SOUND_ERROR_AUDIOCOMPONENTFINDNEXT_FAILED: return "CUTE_SOUND_ERROR_AUDIOCOMPONENTFINDNEXT_FAILED";
	case CUTE_SOUND_ERROR_AUDIOCOMPONENTINSTANCENEW_FAILED: return "CUTE_SOUND_ERROR_AUDIOCOMPONENTINSTANCENEW_FAILED";
	case CUTE_SOUND_ERROR_FAILED_TO_SET_STREAM_FORMAT: return "CUTE_SOUND_ERROR_FAILED_TO_SET_STREAM_FORMAT";
	case CUTE_SOUND_ERROR_FAILED_TO_SET_RENDER_CALLBACK: return "CUTE_SOUND_ERROR_FAILED_TO_SET_RENDER_CALLBACK";
	case CUTE_SOUND_ERROR_AUDIOUNITINITIALIZE_FAILED: return "CUTE_SOUND_ERROR_AUDIOUNITINITIALIZE_FAILED";
	case CUTE_SOUND_ERROR_AUDIOUNITSTART_FAILED: return "CUTE_SOUND_ERROR_AUDIOUNITSTART_FAILED";
	case CUTE_SOUND_ERROR_CANT_OPEN_AUDIO_DEVICE: return "CUTE_SOUND_ERROR_CANT_OPEN_AUDIO_DEVICE";
	case CUTE_SOUND_ERROR_CANT_INIT_SDL_AUDIO: return "CUTE_SOUND_ERROR_CANT_INIT_SDL_AUDIO";
	case CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE: return "CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE";
	case CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND: return "CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND";
	case CUTE_SOUND_ERROR_WAV_DATA_CHUNK_NOT_FOUND: return "CUTE_SOUND_ERROR_WAV_DATA_CHUNK_NOT_FOUND";
	case CUTE_SOUND_ERROR_ONLY_PCM_WAV_FILES_ARE_SUPPORTED: return "CUTE_SOUND_ERROR_ONLY_PCM_WAV_FILES_ARE_SUPPORTED";
	case CUTE_SOUND_ERROR_WAV_ONLY_MONO_OR_STEREO_IS_SUPPORTED: return "CUTE_SOUND_ERROR_WAV_ONLY_MONO_OR_STEREO_IS_SUPPORTED";
	case CUTE_SOUND_ERROR_WAV_ONLY_16_BITS_PER_SAMPLE_SUPPORTED: return "CUTE_SOUND_ERROR_WAV_ONLY_16_BITS_PER_SAMPLE_SUPPORTED";
	case CUTE_SOUND_ERROR_CANNOT_SWITCH_MUSIC_WHILE_PAUSED: return "CUTE_SOUND_ERROR_CANNOT_SWITCH_MUSIC_WHILE_PAUSED";
	case CUTE_SOUND_ERROR_CANNOT_CROSSFADE_WHILE_MUSIC_IS_PAUSED: return "CUTE_SOUND_ERROR_CANNOT_CROSSFADE_WHILE_MUSIC_IS_PAUSED";
	case CUTE_SOUND_ERROR_CANNOT_FADEOUT_WHILE_MUSIC_IS_PAUSED: return "CUTE_SOUND_ERROR_CANNOT_FADEOUT_WHILE_MUSIC_IS_PAUSED";
	case CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT: return "CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT";
	case CUTE_SOUND_ERROR_STB_VORBIS_DECODE_FAILED: return "CUTE_SOUND_ERROR_STB_VORBIS_DECODE_FAILED";
	case CUTE_SOUND_ERROR_OGG_UNSUPPORTED_CHANNEL_COUNT: return "CUTE_SOUND_ERROR_OGG_UNSUPPORTED_CHANNEL_COUN";
	default: return "UNKNOWN";
	}
}

// Cute sound context functions.

void cs_mix();

typedef struct cs_audio_source_t
{
	int sample_rate;
	int sample_count;
	int channel_count;

	// Number of instances currently referencing this audio. Must be zero
	// in order to safely delete the audio. References are automatically
	// updated whenever playing instances are inserted into the context.
	int playing_count;

	// The actual raw audio samples in memory.
	void* channels[2];
} cs_audio_source_t;

typedef struct cs_sound_inst_t
{
	uint64_t id;
	bool is_music;
	bool active;
	bool paused;
	bool looped;
	float volume;
	float pan0;
	float pan1;
	float pitch;
	int sample_index;
	cs_audio_source_t* audio;
	cs_list_node_t node;
} cs_sound_inst_t;

typedef enum cs_music_state_t
{
	CUTE_SOUND_MUSIC_STATE_NONE,
	CUTE_SOUND_MUSIC_STATE_PLAYING,
	CUTE_SOUND_MUSIC_STATE_FADE_OUT,
	CUTE_SOUND_MUSIC_STATE_FADE_IN,
	CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0,
	CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1,
	CUTE_SOUND_MUSIC_STATE_CROSSFADE,
	CUTE_SOUND_MUSIC_STATE_PAUSED
} cs_music_state_t;

#define CUTE_SOUND_PAGE_INSTANCE_COUNT 1024

typedef struct cs_inst_page_t
{
	struct cs_inst_page_t* next;
	cs_sound_inst_t instances[CUTE_SOUND_PAGE_INSTANCE_COUNT];
} cs_inst_page_t;

typedef struct cs_context_t
{
	float global_pan /* = 0.5f */;
	float global_volume /* = 1.0f */;
	bool global_pause /* = false */;
	float music_volume /* = 1.0f */;
	float music_pitch /* = 1.0f */;
	float sound_volume /* = 1.0f */;
	bool cull_duplicates /* = false */;
	void** duplicates /* = NULL */;
	int duplicate_count /* = 0 */;
	int duplicate_capacity /* = 0 */;
	void (*on_finish)(cs_playing_sound_t, void*); /* = NULL */;
	void* on_finish_udata /* = NULL */;
	void (*on_music_finish)(void*); /* = NULL */;
	void* on_music_finish_udata /* = NULL */;

	bool music_paused /* = false */;
	bool music_looped /* = true */;
	float t /* = 0 */;
	float fade /* = 0 */;
	float fade_switch_1 /* = 0 */;
	cs_music_state_t music_state /* = MUSIC_STATE_NONE */;
	cs_music_state_t music_state_to_resume_from_paused /* = MUSIC_STATE_NONE */;
	cs_sound_inst_t* music_playing /* = NULL */;
	cs_sound_inst_t* music_next /* = NULL */;

	int audio_sources_to_free_capacity /* = 0 */;
	int audio_sources_to_free_size /* = 0 */;
	cs_audio_source_t** audio_sources_to_free /* = NULL */;
	uint64_t instance_id_gen /* = 1 */;
	cs_hashtablet instance_map; // <uint64_t, cs_audio_source_t*>
	cs_inst_page_t* pages /* = NULL */;
	cs_list_t playing_sounds;
	cs_list_t free_sounds;

	unsigned latency_samples;
	int Hz;
	int bps;
	int wide_count;
	cs__m128* floatA;
	cs__m128* floatB;
	cs__m128i* samples;
	bool separate_thread;
	bool running;
	int sleep_milliseconds;

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

	DWORD last_cursor;
	unsigned running_index;
	int buffer_size;
	LPDIRECTSOUND dsound;
	LPDIRECTSOUNDBUFFER primary;
	LPDIRECTSOUNDBUFFER secondary;

	// data for cs_mix thread, enable these with cs_spawn_mix_thread
	CRITICAL_SECTION critical_section;

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE

	unsigned index0; // read
	unsigned index1; // write
	unsigned samples_in_circular_buffer;
	int sample_count;

	// platform specific stuff
	AudioComponentInstance inst;

	// data for cs_mix thread, enable these with cs_spawn_mix_thread
	pthread_t thread;
	pthread_mutex_t mutex;

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

	unsigned index0; // read
	unsigned index1; // write
	unsigned samples_in_circular_buffer;
	int sample_count;
	SDL_AudioDeviceID dev;

	// data for cs_mix thread, enable these with cs_spawn_mix_thread
	SDL_Thread* thread;
	SDL_mutex* mutex;

#endif
} cs_context_t;

void* s_mem_ctx;
cs_context_t* s_ctx = NULL;

void cs_sleep(int milliseconds)
{
#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS
	Sleep(milliseconds);
#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE
	struct timespec ts = { 0, milliseconds * 1000000 };
	nanosleep(&ts, NULL);
#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL
	SDL_Delay(milliseconds);
#endif
}

static void* cs_malloc16(size_t size)
{
	void* p = CUTE_SOUND_ALLOC(size + 16, s_mem_ctx);
	if (!p) return 0;
	unsigned char offset = (size_t)p & 15;
	p = (void*)CUTE_SOUND_ALIGN(p + 1, 16);
	*((char*)p - 1) = 16 - offset;
	CUTE_SOUND_ASSERT(!((size_t)p & 15));
	return p;
}

static void cs_free16(void* p)
{
	if (!p) return;
	CUTE_SOUND_FREE((char*)p - (((size_t)*((char*)p - 1)) & 0xFF), s_mem_ctx);
}

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL || CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE

	static int cs_samples_written()
	{
		return s_ctx->samples_in_circular_buffer;
	}

	static int cs_samples_unwritten()
	{
		return s_ctx->sample_count - s_ctx->samples_in_circular_buffer;
	}

	static int cs_samples_to_mix()
	{
		int lat = s_ctx->latency_samples;
		int written = cs_samples_written();
		int dif = lat - written;
		CUTE_SOUND_ASSERT(dif >= 0);
		if (dif)
		{
			int unwritten = cs_samples_unwritten();
			return dif < unwritten ? dif : unwritten;
		}
		return 0;
	}

	#define CUTE_SOUND_SAMPLES_TO_BYTES(interleaved_sample_count) ((interleaved_sample_count) * s_ctx->bps)
	#define CUTE_SOUND_BYTES_TO_SAMPLES(byte_count) ((byte_count) / s_ctx->bps)

	static void cs_push_bytes(void* data, int size)
	{
		int index1 = s_ctx->index1;
		int samples_to_write = CUTE_SOUND_BYTES_TO_SAMPLES(size);
		int sample_count = s_ctx->sample_count;

		int unwritten = cs_samples_unwritten();
		if (unwritten < samples_to_write) samples_to_write = unwritten;
		int samples_to_end = sample_count - index1;

		if (samples_to_write > samples_to_end) {
			CUTE_SOUND_MEMCPY((char*)s_ctx->samples + CUTE_SOUND_SAMPLES_TO_BYTES(index1), data, CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end));
			CUTE_SOUND_MEMCPY(s_ctx->samples, (char*)data + CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end), size - CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end));
			s_ctx->index1 = (samples_to_write - samples_to_end) % sample_count;
		} else {
			CUTE_SOUND_MEMCPY((char*)s_ctx->samples + CUTE_SOUND_SAMPLES_TO_BYTES(index1), data, size);
			s_ctx->index1 = (s_ctx->index1 + samples_to_write) % sample_count;
		}

		s_ctx->samples_in_circular_buffer += samples_to_write;
	}

	static int cs_pull_bytes(void* dst, int size)
	{
		int index0 = s_ctx->index0;
		int allowed_size = CUTE_SOUND_SAMPLES_TO_BYTES(cs_samples_written());
		int sample_count = s_ctx->sample_count;
		int zeros = 0;

		if (allowed_size < size) {
			zeros = size - allowed_size;
			size = allowed_size;
		}

		int samples_to_read = CUTE_SOUND_BYTES_TO_SAMPLES(size);
		int samples_to_end = sample_count - index0;

		if (samples_to_read > samples_to_end) {
			CUTE_SOUND_MEMCPY(dst, ((char*)s_ctx->samples) + CUTE_SOUND_SAMPLES_TO_BYTES(index0), CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end));
			CUTE_SOUND_MEMCPY(((char*)dst) + CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end), s_ctx->samples, size - CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end));
			s_ctx->index0 = (samples_to_read - samples_to_end) % sample_count;
		} else {
			CUTE_SOUND_MEMCPY(dst, ((char*)s_ctx->samples) + CUTE_SOUND_SAMPLES_TO_BYTES(index0), size);
			s_ctx->index0 = (s_ctx->index0 + samples_to_read) % sample_count;
		}

		s_ctx->samples_in_circular_buffer -= samples_to_read;

		return zeros;
	}

#endif

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

	static DWORD WINAPI cs_ctx_thread(LPVOID lpParameter)
	{
		(void)lpParameter;
		while (s_ctx->running) {
			cs_mix();
			if (s_ctx->sleep_milliseconds) cs_sleep(s_ctx->sleep_milliseconds);
			else YieldProcessor();
		}

		s_ctx->separate_thread = false;
		return 0;
	}

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE

	static void* cs_ctx_thread(void* udata)
	{
		while (s_ctx->running) {
			cs_mix();
			if (s_ctx->sleep_milliseconds) cs_sleep(s_ctx->sleep_milliseconds);
			else pthread_yield_np();
		}

		s_ctx->separate_thread = 0;
		pthread_exit(0);
		return 0;
	}

	static OSStatus cs_memcpy_to_coreaudio(void* udata, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
	{
		int bps = s_ctx->bps;
		int samples_requested_to_consume = inNumberFrames;
		AudioBuffer* buffer = ioData->mBuffers;

		CUTE_SOUND_ASSERT(ioData->mNumberBuffers == 1);
		CUTE_SOUND_ASSERT(buffer->mNumberChannels == 2);
		int byte_size = buffer->mDataByteSize;
		CUTE_SOUND_ASSERT(byte_size == samples_requested_to_consume * bps);

		int zero_bytes = cs_pull_bytes(buffer->mData, byte_size);
		CUTE_SOUND_MEMSET(((char*)buffer->mData) + (byte_size - zero_bytes), 0, zero_bytes);

		return noErr;
	}

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

	int cs_ctx_thread(void* udata)
	{
		while (s_ctx->running) {
			cs_mix();
			if (s_ctx->sleep_milliseconds) cs_sleep(s_ctx->sleep_milliseconds);
			else cs_sleep(1);
		}

		s_ctx->separate_thread = false;
		return 0;
	}

	static void cs_sdl_audio_callback(void* udata, Uint8* stream, int len)
	{
		int zero_bytes = cs_pull_bytes(stream, len);
		CUTE_SOUND_MEMSET(stream + (len - zero_bytes), 0, zero_bytes);
	}

#endif

static void s_add_page()
{
	cs_inst_page_t* page = (cs_inst_page_t*)CUTE_SOUND_ALLOC(sizeof(cs_inst_page_t), user_allocator_context);
	for (int i = 0; i < CUTE_SOUND_PAGE_INSTANCE_COUNT; ++i) {
		cs_list_init_node(&page->instances[i].node);
		cs_list_push_back(&s_ctx->free_sounds, &page->instances[i].node);
	}
	page->next = s_ctx->pages;
	s_ctx->pages = page;
}

cs_error_t cs_init(void* os_handle, unsigned play_frequency_in_Hz, int buffered_samples, void* user_allocator_context /* = NULL */)
{
	buffered_samples = buffered_samples < CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES ? CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES : buffered_samples;
	int sample_count = buffered_samples;
	int wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4);
	int bps = sizeof(uint16_t) * 2;

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

	int buffer_size = buffered_samples * bps;
	LPDIRECTSOUND dsound = NULL;
	LPDIRECTSOUNDBUFFER primary_buffer = NULL;
	LPDIRECTSOUNDBUFFER secondary_buffer = NULL;

	if (!os_handle) return CUTE_SOUND_ERROR_HWND_IS_NULL;
	{
		WAVEFORMATEX format = { 0, 0, 0, 0, 0, 0, 0 };
		DSBUFFERDESC bufdesc = { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } };
		HRESULT res = DirectSoundCreate(0, &dsound, 0);
		if (res != DS_OK) return CUTE_SOUND_ERROR_DIRECTSOUND_CREATE_FAILED;
		IDirectSound_SetCooperativeLevel(dsound, (HWND)os_handle, DSSCL_PRIORITY);
		bufdesc.dwSize = sizeof(bufdesc);
		bufdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

		res = IDirectSound_CreateSoundBuffer(dsound, &bufdesc, &primary_buffer, 0);
		if (res != DS_OK) CUTE_SOUND_ERROR_CREATESOUNDBUFFER_FAILED;

		format.wFormatTag = WAVE_FORMAT_PCM;
		format.nChannels = 2;
		format.nSamplesPerSec = play_frequency_in_Hz;
		format.wBitsPerSample = 16;
		format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
		format.cbSize = 0;
		res = IDirectSoundBuffer_SetFormat(primary_buffer, &format);
		if (res != DS_OK) CUTE_SOUND_ERROR_SETFORMAT_FAILED;

		bufdesc.dwSize = sizeof(bufdesc);
		bufdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
		bufdesc.dwBufferBytes = buffer_size;
		bufdesc.lpwfxFormat = &format;
		res = IDirectSound_CreateSoundBuffer(dsound, &bufdesc, &secondary_buffer, 0);
		if (res != DS_OK) CUTE_SOUND_ERROR_SETFORMAT_FAILED;

		// Silence the initial audio buffer.
		void* region1;
		DWORD size1;
		void* region2;
		DWORD size2;
		res = IDirectSoundBuffer_Lock(secondary_buffer, 0, bufdesc.dwBufferBytes, &region1, &size1, &region2, &size2, DSBLOCK_ENTIREBUFFER);
		if (res == DS_OK) {
			CUTE_SOUND_MEMSET(region1, 0, size1);
			IDirectSoundBuffer_Unlock(secondary_buffer, region1, size1, region2, size2);
		}
	}

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE

	AudioComponentDescription comp_desc = { 0 };
	comp_desc.componentType = kAudioUnitType_Output;
	comp_desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	comp_desc.componentFlags = 0;
	comp_desc.componentFlagsMask = 0;
	comp_desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent comp = AudioComponentFindNext(NULL, &comp_desc);
	if (!comp) return CUTE_SOUND_ERROR_AUDIOCOMPONENTFINDNEXT_FAILED;

	AudioStreamBasicDescription stream_desc = { 0 };
	stream_desc.mSampleRate = (double)play_frequency_in_Hz;
	stream_desc.mFormatID = kAudioFormatLinearPCM;
	stream_desc.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
	stream_desc.mFramesPerPacket = 1;
	stream_desc.mChannelsPerFrame = 2;
	stream_desc.mBitsPerChannel = sizeof(uint16_t) * 8;
	stream_desc.mBytesPerPacket = bps;
	stream_desc.mBytesPerFrame = bps;
	stream_desc.mReserved = 0;

	AudioComponentInstance inst;
	OSStatus ret;
	AURenderCallbackStruct input;

	ret = AudioComponentInstanceNew(comp, &inst);
	if (ret != noErr) return CUTE_SOUND_ERROR_AUDIOCOMPONENTINSTANCENEW_FAILED;

	ret = AudioUnitSetProperty(inst, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &stream_desc, sizeof(stream_desc));
	if (ret != noErr) return CUTE_SOUND_ERROR_FAILED_TO_SET_STREAM_FORMAT;

	ret = AudioUnitInitialize(inst);
	if (ret != noErr) return CUTE_SOUND_ERROR_AUDIOUNITINITIALIZE_FAILED;

	ret = AudioOutputUnitStart(inst);
	if (ret != noErr) return CUTE_SOUND_ERROR_AUDIOUNITSTART_FAILED;

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

	SDL_AudioSpec wanted, have;
	int ret = SDL_InitSubSystem(SDL_INIT_AUDIO);
	if (ret < 0) return CUTE_SOUND_ERROR_CANT_INIT_SDL_AUDIO;

#endif

	s_ctx = (cs_context_t*)CUTE_SOUND_ALLOC(sizeof(cs_context_t), user_allocator_context);
	s_ctx->global_pan = 0.5f;
	s_ctx->global_volume = 1.0f;
	s_ctx->global_pause = false;
	s_ctx->music_volume = 1.0f;
	s_ctx->music_pitch = 1.0f;
	s_ctx->sound_volume = 1.0f;
	s_ctx->music_looped = true;
	s_ctx->music_paused = false;
	s_ctx->on_finish = NULL;
	s_ctx->on_finish_udata = NULL;
	s_ctx->on_music_finish = NULL;
	s_ctx->on_music_finish_udata = NULL;
	s_ctx->t = 0;
	s_ctx->fade = 0;
	s_ctx->fade_switch_1 = 0;
	s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_NONE;
	s_ctx->music_state_to_resume_from_paused = CUTE_SOUND_MUSIC_STATE_NONE;
	s_ctx->music_playing = NULL;
	s_ctx->music_next = NULL;
	s_ctx->audio_sources_to_free_capacity = 32;
	s_ctx->audio_sources_to_free_size = 0;
	s_ctx->audio_sources_to_free = (cs_audio_source_t**)CUTE_SOUND_ALLOC(sizeof(cs_audio_source_t*) * s_ctx->audio_sources_to_free_capacity, s_mem_ctx);
	s_ctx->instance_id_gen = 1;
	cs_hashtableinit(&s_ctx->instance_map, sizeof(cs_audio_source_t*), 1024, user_allocator_context);
	s_ctx->pages = NULL;
	cs_list_init(&s_ctx->playing_sounds);
	cs_list_init(&s_ctx->free_sounds);
	s_add_page();
	s_mem_ctx = user_allocator_context;
	s_ctx->latency_samples = buffered_samples;
	s_ctx->Hz = play_frequency_in_Hz;
	s_ctx->bps = bps;
	s_ctx->wide_count = wide_count;
	s_ctx->floatA = (cs__m128*)cs_malloc16(sizeof(cs__m128) * wide_count);
	s_ctx->floatB = (cs__m128*)cs_malloc16(sizeof(cs__m128) * wide_count);
	s_ctx->samples = (cs__m128i*)cs_malloc16(sizeof(cs__m128i) * wide_count);
	s_ctx->running = true;
	s_ctx->separate_thread = false;
	s_ctx->sleep_milliseconds = 0;

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

	s_ctx->last_cursor = 0;
	s_ctx->running_index = 0;
	s_ctx->buffer_size = buffer_size;
	s_ctx->dsound = dsound;
	s_ctx->primary = primary_buffer;
	s_ctx->secondary = secondary_buffer;
	InitializeCriticalSectionAndSpinCount(&s_ctx->critical_section, 0x00000400);

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE

	s_ctx->index0 = 0;
	s_ctx->index1 = 0;
	s_ctx->samples_in_circular_buffer = 0;
	s_ctx->sample_count = wide_count * 4;
	s_ctx->inst = inst;
	pthread_mutex_init(&s_ctx->mutex, NULL);

	input.inputProc = cs_memcpy_to_coreaudio;
	input.inputProcRefCon = s_ctx;
	ret = AudioUnitSetProperty(inst, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(input));
	if (ret != noErr) return CUTE_SOUND_ERROR_FAILED_TO_SET_RENDER_CALLBACK; // This leaks memory, oh well.

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

	SDL_memset(&wanted, 0, sizeof(wanted));
	SDL_memset(&have, 0, sizeof(have));
	wanted.freq = play_frequency_in_Hz;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2; /* 1 = mono, 2 = stereo */
	wanted.samples = buffered_samples;
	wanted.callback = cs_sdl_audio_callback;
	wanted.userdata = s_ctx;
	s_ctx->index0 = 0;
	s_ctx->index1 = 0;
	s_ctx->samples_in_circular_buffer = 0;
	s_ctx->sample_count = wide_count * 4;
	s_ctx->dev = SDL_OpenAudioDevice(NULL, 0, &wanted, &have, 0);
	if (s_ctx->dev < 0) return CUTE_SOUND_ERROR_CANT_OPEN_AUDIO_DEVICE; // This leaks memory, oh well.
	SDL_PauseAudioDevice(s_ctx->dev, 0);
	s_ctx->mutex = SDL_CreateMutex();

#endif
	s_ctx->duplicate_capacity = 0;
	s_ctx->duplicate_count = 0;
	s_ctx->duplicates = NULL;
	s_ctx->cull_duplicates = false;
	return CUTE_SOUND_ERROR_NONE;
}

void cs_lock();
void cs_unlock();

void cs_shutdown()
{
	if (!s_ctx) return;
	if (s_ctx->separate_thread) {
		cs_lock();
		s_ctx->running = false;
		cs_unlock();
		while (s_ctx->separate_thread) cs_sleep(1);
	}
#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

	DeleteCriticalSection(&s_ctx->critical_section);
	IDirectSoundBuffer_Release(s_ctx->secondary);
	IDirectSoundBuffer_Release(s_ctx->primary);
	IDirectSoundBuffer_Release(s_ctx->dsound);

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE
#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

	SDL_DestroyMutex(s_ctx->mutex);
	SDL_CloseAudioDevice(s_ctx->dev);

#endif

	if (!cs_list_empty(&s_ctx->playing_sounds)) {
		cs_list_node_t* playing_node = cs_list_begin(&s_ctx->playing_sounds);
		cs_list_node_t* end_node = cs_list_end(&s_ctx->playing_sounds);
		do {
			cs_list_node_t* next_node = playing_node->next;
			cs_sound_inst_t* playing = CUTE_SOUND_LIST_HOST(cs_sound_inst_t, node, playing_node);
			cs_audio_source_t* audio = playing->audio;
			if (audio) audio->playing_count = 0;
			playing_node = next_node;
		} while (playing_node != end_node);
	}

	cs_inst_page_t* page = s_ctx->pages;
	while (page) {
		cs_inst_page_t* next = page->next;
		CUTE_SOUND_FREE(page, s_mem_ctx);
		page = next;
	}

	for (int i = 0; i < s_ctx->audio_sources_to_free_size; ++i) {
		cs_audio_source_t* audio = s_ctx->audio_sources_to_free[i];
		cs_free16(audio->channels[0]);
		CUTE_SOUND_FREE(audio, s_mem_ctx);
	}
	CUTE_SOUND_FREE(s_ctx->audio_sources_to_free, s_mem_ctx);

	cs_free16(s_ctx->floatA);
	cs_free16(s_ctx->floatB);
	cs_free16(s_ctx->samples);
	cs_hashtableterm(&s_ctx->instance_map);
	CUTE_SOUND_FREE(s_ctx, s_mem_ctx);
	s_ctx = NULL;
}

static float s_smoothstep(float x) { return x * x * (3.0f - 2.0f * x); }

void cs_update(float dt)
{
	if (!s_ctx->separate_thread) cs_mix();

	switch (s_ctx->music_state) {
	case CUTE_SOUND_MUSIC_STATE_FADE_OUT:
	{
		s_ctx->t += dt;
		if (s_ctx->t >= s_ctx->fade) {
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_NONE;
			s_ctx->music_playing->active = false;
			s_ctx->music_playing = NULL;
		} else {
			s_ctx->music_playing->volume = s_smoothstep(((s_ctx->fade - s_ctx->t) / s_ctx->fade));;
		}
	}	break;

	case CUTE_SOUND_MUSIC_STATE_FADE_IN:
	{
		s_ctx->t += dt;
		if (s_ctx->t >= s_ctx->fade) {
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_PLAYING;
			s_ctx->t = s_ctx->fade;
		}
		s_ctx->music_playing->volume = s_smoothstep(1.0f - ((s_ctx->fade - s_ctx->t) / s_ctx->fade));
	}	break;

	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0:
	{
		s_ctx->t += dt;
		if (s_ctx->t >= s_ctx->fade) {
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1;
			s_ctx->music_playing->active = false;
			s_ctx->music_playing->volume = 0;
			s_ctx->t = 0;
			s_ctx->fade = s_ctx->fade_switch_1;
			s_ctx->fade_switch_1 = 0;
			s_ctx->music_next->paused = false;
		} else {
			s_ctx->music_playing->volume = s_smoothstep(((s_ctx->fade - s_ctx->t) / s_ctx->fade));;
		}
	}	break;

	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1:
	{
		s_ctx->t += dt;
		if (s_ctx->t >= s_ctx->fade) {
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_PLAYING;
			s_ctx->t = s_ctx->fade;
			s_ctx->music_next->volume = 1.0f;
			s_ctx->music_playing = s_ctx->music_next;
			s_ctx->music_next = NULL;
		} else {
			float t = s_smoothstep(1.0f - ((s_ctx->fade - s_ctx->t) / s_ctx->fade));
			float volume = t;
			s_ctx->music_next->volume = volume;
		}
	}	break;

	case CUTE_SOUND_MUSIC_STATE_CROSSFADE:
	{
		s_ctx->t += dt;
		if (s_ctx->t >= s_ctx->fade) {
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_PLAYING;
			s_ctx->music_playing->active = false;
			s_ctx->music_next->volume = 1.0f;
			s_ctx->music_playing = s_ctx->music_next;
			s_ctx->music_next = NULL;
		} else {
			float t0 = s_smoothstep(((s_ctx->fade - s_ctx->t) / s_ctx->fade));
			float t1 = s_smoothstep(1.0f - ((s_ctx->fade - s_ctx->t) / s_ctx->fade));
			float v0 = t0;
			float v1 = t1;
			s_ctx->music_playing->volume = v0;
			s_ctx->music_next->volume = v1;
		}
	}	break;

	default:
		break;
	}
}

void cs_set_global_volume(float volume_0_to_1)
{
	if (volume_0_to_1 < 0) volume_0_to_1 = 0;
	s_ctx->global_volume = volume_0_to_1;
}

void cs_set_global_pan(float pan_0_to_1)
{
	if (pan_0_to_1 < 0) pan_0_to_1 = 0;
	if (pan_0_to_1 > 1) pan_0_to_1 = 1;
	s_ctx->global_pan = pan_0_to_1;
}

void cs_set_global_pause(bool true_for_paused)
{
	s_ctx->global_pause = true_for_paused;
}

void cs_lock()
{
#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS
	EnterCriticalSection(&s_ctx->critical_section);
#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE
	pthread_mutex_lock(&s_ctx->mutex);
#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL
	SDL_LockMutex(s_ctx->mutex);
#endif
}

void cs_unlock()
{
#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS
	LeaveCriticalSection(&s_ctx->critical_section);
#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE
	pthread_mutex_unlock(&s_ctx->mutex);
#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL
	SDL_UnlockMutex(s_ctx->mutex);
#endif
}

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

static void cs_dsound_get_bytes_to_fill(int* byte_to_lock, int* bytes_to_write)
{
	DWORD play_cursor;
	DWORD write_cursor;
	DWORD lock;
	DWORD target_cursor;
	DWORD write;
	DWORD status;

	HRESULT hr = IDirectSoundBuffer_GetCurrentPosition(s_ctx->secondary, &play_cursor, &write_cursor);
	if (hr != DS_OK) {
		if (hr == DSERR_BUFFERLOST) {
			hr = IDirectSoundBuffer_Restore(s_ctx->secondary);
		}
		*byte_to_lock = write_cursor;
		*bytes_to_write = s_ctx->latency_samples * s_ctx->bps;
		if (!SUCCEEDED(hr)) {
			return;
		}
	}

	s_ctx->last_cursor = write_cursor;

	IDirectSoundBuffer_GetStatus(s_ctx->secondary, &status);
	if (!(status & DSBSTATUS_PLAYING)) {
		hr = IDirectSoundBuffer_Play(s_ctx->secondary, 0, 0, DSBPLAY_LOOPING);
		if (!SUCCEEDED(hr)) {
			return;
		}
	}

	lock = (s_ctx->running_index * s_ctx->bps) % s_ctx->buffer_size;
	target_cursor = (write_cursor + s_ctx->latency_samples * s_ctx->bps);
	if (target_cursor > (DWORD)s_ctx->buffer_size) target_cursor %= s_ctx->buffer_size;
	target_cursor = (DWORD)CUTE_SOUND_TRUNC(target_cursor, 16);

	if (lock > target_cursor) {
		write = (s_ctx->buffer_size - lock) + target_cursor;
	} else {
		write = target_cursor - lock;
	}

	*byte_to_lock = lock;
	*bytes_to_write = write;
}

static void cs_dsound_memcpy_to_driver(int16_t* samples, int byte_to_lock, int bytes_to_write)
{
	// copy mixer buffers to direct sound
	void* region1;
	DWORD size1;
	void* region2;
	DWORD size2;
	HRESULT hr = IDirectSoundBuffer_Lock(s_ctx->secondary, byte_to_lock, bytes_to_write, &region1, &size1, &region2, &size2, 0);
	if (hr == DSERR_BUFFERLOST) {
		IDirectSoundBuffer_Restore(s_ctx->secondary);
		hr = IDirectSoundBuffer_Lock(s_ctx->secondary, byte_to_lock, bytes_to_write, &region1, &size1, &region2, &size2, 0);
	}
	if (!SUCCEEDED(hr)) {
		return;
	}

	unsigned running_index = s_ctx->running_index;
	INT16* sample1 = (INT16*)region1;
	DWORD sample1_count = size1 / s_ctx->bps;
	CUTE_SOUND_MEMCPY(sample1, samples, sample1_count * sizeof(INT16) * 2);
	samples += sample1_count * 2;
	running_index += sample1_count;

	INT16* sample2 = (INT16*)region2;
	DWORD sample2_count = size2 / s_ctx->bps;
	CUTE_SOUND_MEMCPY(sample2, samples, sample2_count * sizeof(INT16) * 2);
	samples += sample2_count * 2;
	running_index += sample2_count;

	IDirectSoundBuffer_Unlock(s_ctx->secondary, region1, size1, region2, size2);
	s_ctx->running_index = running_index;
}

void cs_dsound_dont_run_too_fast()
{
	DWORD status;
	DWORD cursor;
	DWORD junk;
	HRESULT hr;

	hr = IDirectSoundBuffer_GetCurrentPosition(s_ctx->secondary, &junk, &cursor);
	if (hr != DS_OK) {
		if (hr == DSERR_BUFFERLOST) {
			IDirectSoundBuffer_Restore(s_ctx->secondary);
		}
		return;
	}

	// Prevent mixing thread from sending samples too quickly.
	while (cursor == s_ctx->last_cursor) {
		cs_sleep(1);

		IDirectSoundBuffer_GetStatus(s_ctx->secondary, &status);
		if ((status & DSBSTATUS_BUFFERLOST)) {
			IDirectSoundBuffer_Restore(s_ctx->secondary);
			IDirectSoundBuffer_GetStatus(s_ctx->secondary, &status);
			if ((status & DSBSTATUS_BUFFERLOST)) {
				break;
			}
		}

		hr = IDirectSoundBuffer_GetCurrentPosition(s_ctx->secondary, &junk, &cursor);
		if (hr != DS_OK) {
			// Eek! Not much to do here I guess.
			return;
		}
	}
}

#endif // CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

void cs_mix()
{
	cs__m128i* samples;
	cs__m128* floatA;
	cs__m128* floatB;
	int samples_needed;
	int write_offset = 0;

	cs_lock();

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

	int byte_to_lock;
	int bytes_to_write;
	cs_dsound_get_bytes_to_fill(&byte_to_lock, &bytes_to_write);

	if (bytes_to_write < (int)s_ctx->latency_samples) goto unlock;
	samples_needed = bytes_to_write / s_ctx->bps;

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE || CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

	int bytes_to_write;
	samples_needed = cs_samples_to_mix();
	if (!samples_needed) goto unlock;
	bytes_to_write = samples_needed * s_ctx->bps;

#endif

	// Clear mixer buffers.
	floatA = s_ctx->floatA;
	floatB = s_ctx->floatB;
	CUTE_SOUND_MEMSET(floatA, 0, sizeof(cs__m128) * s_ctx->wide_count);
	CUTE_SOUND_MEMSET(floatB, 0, sizeof(cs__m128) * s_ctx->wide_count);

	// Mix all playing sounds into the mixer buffers.
	if (!s_ctx->global_pause && !cs_list_empty(&s_ctx->playing_sounds)) {
		cs_list_node_t* playing_node = cs_list_begin(&s_ctx->playing_sounds);
		cs_list_node_t* end_node = cs_list_end(&s_ctx->playing_sounds);
		do {
			cs_list_node_t* next_node = playing_node->next;
			cs_sound_inst_t* playing = CUTE_SOUND_LIST_HOST(cs_sound_inst_t, node, playing_node);
			cs_audio_source_t* audio = playing->audio;

			if (!audio) goto remove;
			if (!playing->active || !s_ctx->running) goto remove;
			if (playing->paused || playing->pitch==0.0f) goto get_next_playing_sound;
			if (s_ctx->cull_duplicates) {
				for (int i = 0; i < s_ctx->duplicate_count; ++i) {
					if (s_ctx->duplicates[i] == (void*)audio) {
						goto remove;
					}
				}
				if (s_ctx->duplicate_count == s_ctx->duplicate_capacity) {
					int new_capacity = s_ctx->duplicate_capacity ? s_ctx->duplicate_capacity * 2 : 1024;
					void* duplicates = CUTE_SOUND_ALLOC(sizeof(void*) * new_capacity, s_ctx->mem_ctx);
					CUTE_SOUND_MEMCPY(duplicates, s_ctx->duplicates, sizeof(void*) * s_ctx->duplicate_count);
					CUTE_SOUND_FREE(s_ctx->duplicates, s_ctx->mem_ctx);
					s_ctx->duplicates = (void**)duplicates;
					s_ctx->duplicate_capacity = new_capacity;
				}
				s_ctx->duplicates[s_ctx->duplicate_count++] = (void*)audio;
			}

			// Jump here for looping sounds if we need to wrap-around the audio source
			// and continue mixing more samples.
			mix_more:

			{
				cs__m128* cA = (cs__m128*)audio->channels[0];
				cs__m128* cB = (cs__m128*)audio->channels[1];

				// Attempted to play a sound with no audio.
				// Make sure the audio file was loaded properly.
				CUTE_SOUND_ASSERT(cA);

				float gpan0 = 1.0f - s_ctx->global_pan;
				float gpan1 = s_ctx->global_pan;
				float vA0 = playing->volume * playing->pan0 * gpan0 * s_ctx->global_volume;
				float vB0 = playing->volume * playing->pan1 * gpan1 * s_ctx->global_volume;
				if (!playing->is_music) {
					vA0 *= s_ctx->sound_volume;
					vB0 *= s_ctx->sound_volume;
				} else {
					vA0 *= s_ctx->music_volume;
					vB0 *= s_ctx->music_volume;
				}
				cs__m128 vA = cs_mm_set1_ps(vA0);
				cs__m128 vB = cs_mm_set1_ps(vB0);

				int prev_playing_sample_index = playing->sample_index;
				int samples_to_read = (int)(samples_needed * playing->pitch);
				if (samples_to_read + playing->sample_index > audio->sample_count) {
					samples_to_read = audio->sample_count - playing->sample_index;
				} else if (samples_to_read + playing->sample_index < 0) {
					// When pitch shifting is negative, samples_to_read is also negative so that offset needs to
					// be accounted for otherwise the sample index cursor gets stuck at sample count.
					playing->sample_index = audio->sample_count + samples_to_read + playing->sample_index;
				}
				int sample_index_wide = (int)CUTE_SOUND_TRUNC(playing->sample_index, 4) / 4;
				int samples_to_write = (int)(samples_to_read / playing->pitch);
				int write_wide = CUTE_SOUND_ALIGN(samples_to_write, 4) / 4;
				int write_offset_wide = (int)CUTE_SOUND_ALIGN(write_offset, 4) / 4;
				static int written_so_far = 0;
				written_so_far += samples_to_read;

				// Do the actual mixing: Apply volume, load samples into float buffers.
				if (playing->pitch != 1.0f) {
					// Pitch shifting -- We read in samples at a resampled rate (multiply by pitch). These samples
					// are read in one at a time in scalar mode, but then mixed together via SIMD.
					cs__m128 pitch = cs_mm_set1_ps(playing->pitch);
					cs__m128 index_offset = cs_mm_set1_ps((float)playing->sample_index);
					switch (audio->channel_count) {
					case 1:
					{
						for (int i = 0, j = 0; i < write_wide; ++i, j += 4) {
							cs__m128 index = cs_mm_set_ps((float)j + 3, (float)j + 2, (float)j + 1, (float)j);
							index = cs_mm_add_ps(cs_mm_mul_ps(index, pitch), index_offset);
							cs__m128i index_int = cs_mm_cvttps_epi32(index);
							cs__m128 index_frac = cs_mm_sub_ps(index, cs_mm_cvtepi32_ps(index_int));
							int i0 = cs_mm_extract_epi32(index_int, 3);
							int i1 = cs_mm_extract_epi32(index_int, 2);
							int i2 = cs_mm_extract_epi32(index_int, 1);
							int i3 = cs_mm_extract_epi32(index_int, 0);

							cs__m128 loA = cs_mm_set_ps(
								i0 > audio->sample_count ? 0 : i0 < 0 ? audio->sample_count : ((float*)cA)[i0],
								i1 > audio->sample_count ? 0 : i1 < 0 ? audio->sample_count : ((float*)cA)[i1],
								i2 > audio->sample_count ? 0 : i2 < 0 ? audio->sample_count : ((float*)cA)[i2],
								i3 > audio->sample_count ? 0 : i3 < 0 ? audio->sample_count : ((float*)cA)[i3]
							);
							cs__m128 hiA = cs_mm_set_ps(
								i0 + 1 > audio->sample_count ? 0 : i0 + 1 < 0 ? audio->sample_count : ((float*)cA)[i0 + 1],
								i1 + 1 > audio->sample_count ? 0 : i1 + 1 < 0 ? audio->sample_count : ((float*)cA)[i1 + 1],
								i2 + 1 > audio->sample_count ? 0 : i2 + 1 < 0 ? audio->sample_count : ((float*)cA)[i2 + 1],
								i3 + 1 > audio->sample_count ? 0 : i3 + 1 < 0 ? audio->sample_count : ((float*)cA)[i3 + 1]
							);

							cs__m128 A = cs_mm_add_ps(loA, cs_mm_mul_ps(index_frac, cs_mm_sub_ps(hiA, loA)));
							cs__m128 B = cs_mm_mul_ps(A, vB);
							A = cs_mm_mul_ps(A, vA);
							floatA[i + write_offset_wide] = cs_mm_add_ps(floatA[i + write_offset_wide], A);
							floatB[i + write_offset_wide] = cs_mm_add_ps(floatB[i + write_offset_wide], B);
						}
						break;
					}
					case 2:
					{
						for (int i = 0, j = 0; i < write_wide; ++i, j += 4) {
							cs__m128 index = cs_mm_set_ps((float)j + 3, (float)j + 2, (float)j + 1, (float)j);
							index = cs_mm_add_ps(cs_mm_mul_ps(index, pitch), index_offset);
							cs__m128i index_int = cs_mm_cvttps_epi32(index);
							cs__m128 index_frac = cs_mm_sub_ps(index, cs_mm_cvtepi32_ps(index_int));
							int i0 = cs_mm_extract_epi32(index_int, 3);
							int i1 = cs_mm_extract_epi32(index_int, 2);
							int i2 = cs_mm_extract_epi32(index_int, 1);
							int i3 = cs_mm_extract_epi32(index_int, 0);

							cs__m128 loA = cs_mm_set_ps(
								i0 > audio->sample_count ? 0 : i0 < 0 ? audio->sample_count : ((float*)cA)[i0],
								i1 > audio->sample_count ? 0 : i1 < 0 ? audio->sample_count : ((float*)cA)[i1],
								i2 > audio->sample_count ? 0 : i2 < 0 ? audio->sample_count : ((float*)cA)[i2],
								i3 > audio->sample_count ? 0 : i3 < 0 ? audio->sample_count : ((float*)cA)[i3]
							);
							cs__m128 hiA = cs_mm_set_ps(
								i0 + 1 > audio->sample_count ? 0 : i0 + 1 < 0 ? audio->sample_count : ((float*)cA)[i0 + 1],
								i1 + 1 > audio->sample_count ? 0 : i1 + 1 < 0 ? audio->sample_count : ((float*)cA)[i1 + 1],
								i2 + 1 > audio->sample_count ? 0 : i2 + 1 < 0 ? audio->sample_count : ((float*)cA)[i2 + 1],
								i3 + 1 > audio->sample_count ? 0 : i3 + 1 < 0 ? audio->sample_count : ((float*)cA)[i3 + 1]
							);

							cs__m128 loB = cs_mm_set_ps(
								i0 > audio->sample_count ? 0 : i0 < 0 ? audio->sample_count : ((float*)cB)[i0],
								i1 > audio->sample_count ? 0 : i1 < 0 ? audio->sample_count : ((float*)cB)[i1],
								i2 > audio->sample_count ? 0 : i2 < 0 ? audio->sample_count : ((float*)cB)[i2],
								i3 > audio->sample_count ? 0 : i3 < 0 ? audio->sample_count : ((float*)cB)[i3]
							);
							cs__m128 hiB = cs_mm_set_ps(
								i0 + 1 > audio->sample_count ? 0 : i0 + 1 < 0 ? audio->sample_count : ((float*)cB)[i0 + 1],
								i1 + 1 > audio->sample_count ? 0 : i1 + 1 < 0 ? audio->sample_count : ((float*)cB)[i1 + 1],
								i2 + 1 > audio->sample_count ? 0 : i2 + 1 < 0 ? audio->sample_count : ((float*)cB)[i2 + 1],
								i3 + 1 > audio->sample_count ? 0 : i3 + 1 < 0 ? audio->sample_count : ((float*)cB)[i3 + 1]
							);

							cs__m128 A = cs_mm_add_ps(loA, cs_mm_mul_ps(index_frac, cs_mm_sub_ps(hiA, loA)));
							cs__m128 B = cs_mm_add_ps(loB, cs_mm_mul_ps(index_frac, cs_mm_sub_ps(hiB, loB)));

							A = cs_mm_mul_ps(A, vA);
							B = cs_mm_mul_ps(B, vB);
							floatA[i + write_offset_wide] = cs_mm_add_ps(floatA[i + write_offset_wide], A);
							floatB[i + write_offset_wide] = cs_mm_add_ps(floatB[i + write_offset_wide], B);
						}
					}	break;
					}
				} else {
					// No pitch shifting, just add samples together.
					switch (audio->channel_count) {
					case 1:
					{
						for (int i = 0; i < write_wide; ++i) {
							cs__m128 A = cA[i + sample_index_wide];
							cs__m128 B = cs_mm_mul_ps(A, vB);
							A = cs_mm_mul_ps(A, vA);
							floatA[i + write_offset_wide] = cs_mm_add_ps(floatA[i + write_offset_wide], A);
							floatB[i + write_offset_wide] = cs_mm_add_ps(floatB[i + write_offset_wide], B);
						}
						break;
					}
					case 2:
					{
						for (int i = 0; i < write_wide; ++i) {
							cs__m128 A = cA[i + sample_index_wide];
							cs__m128 B = cB[i + sample_index_wide];

							A = cs_mm_mul_ps(A, vA);
							B = cs_mm_mul_ps(B, vB);
							floatA[i + write_offset_wide] = cs_mm_add_ps(floatA[i + write_offset_wide], A);
							floatB[i + write_offset_wide] = cs_mm_add_ps(floatB[i + write_offset_wide], B);
						}
					}	break;
					}
				}

				// playing list logic
				playing->sample_index += samples_to_read;
				CUTE_SOUND_ASSERT(playing->sample_index <= audio->sample_count);
				if (playing->pitch < 0) {
					// When pitch shifting is negative adjust the timing a bit further back from sample count to avoid any clipping.
					if (prev_playing_sample_index - playing->sample_index < 0) {
						if (playing->looped) {
							playing->sample_index = audio->sample_count - samples_needed;
							write_offset += samples_to_write;
							samples_needed -= samples_to_write;
							CUTE_SOUND_ASSERT(samples_needed >= 0);
							if (samples_needed == 0) break;
							goto mix_more;
						}

						goto remove;
					}
				} else if (playing->sample_index == audio->sample_count) {
					if (playing->looped) {
						playing->sample_index = 0;
						write_offset += samples_to_write;
						samples_needed -= samples_to_write;
						CUTE_SOUND_ASSERT(samples_needed >= 0);
						if (samples_needed == 0) break;
						goto mix_more;
					}

					goto remove;
				}
			}

		get_next_playing_sound:
			playing_node = next_node;
			write_offset = 0;
			continue;

		remove:
			playing->sample_index = 0;
			playing->active = false;

			if (playing->audio) {
				if (s_ctx->running) {
					playing->audio->playing_count -= 1;
				} else {
					playing->audio->playing_count = 0;
				}
				CUTE_SOUND_ASSERT(playing->audio->playing_count >= 0);
			}

			cs_list_remove(playing_node);
			cs_list_push_front(&s_ctx->free_sounds, playing_node);
			cs_hashtableremove(&s_ctx->instance_map, playing->id);
			playing_node = next_node;
			write_offset = 0;
			if (s_ctx->on_finish && !playing->is_music) {
				cs_playing_sound_t snd = { playing->id };
				s_ctx->on_finish(snd, s_ctx->on_finish_udata);
			} else if (s_ctx->on_music_finish && playing->is_music) {
				s_ctx->on_music_finish(s_ctx->on_music_finish_udata);
			}
			continue;
		} while (playing_node != end_node);
	}

	s_ctx->duplicate_count = 0;

	// load all floats into 16 bit packed interleaved samples
#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

	samples = s_ctx->samples;
	for (int i = 0; i < s_ctx->wide_count; ++i) {
		cs__m128i a = cs_mm_cvtps_epi32(floatA[i]);
		cs__m128i b = cs_mm_cvtps_epi32(floatB[i]);
		cs__m128i a0b0a1b1 = cs_mm_unpacklo_epi32(a, b);
		cs__m128i a2b2a3b3 = cs_mm_unpackhi_epi32(a, b);
		samples[i] = cs_mm_packs_epi32(a0b0a1b1, a2b2a3b3);
	}
	cs_dsound_memcpy_to_driver((int16_t*)samples, byte_to_lock, bytes_to_write);
	cs_dsound_dont_run_too_fast();

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE || CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

	// Since the ctx->samples array is already in use as a ring buffer
	// reusing floatA to store output is a good way to temporarly store
	// the final samples. Then a single ring buffer push can be used
	// afterwards. Pretty hacky, but whatever :)
	samples = (cs__m128i*)floatA;
	for (int i = 0; i < s_ctx->wide_count; ++i) {
		cs__m128i a = cs_mm_cvtps_epi32(floatA[i]);
		cs__m128i b = cs_mm_cvtps_epi32(floatB[i]);
		cs__m128i a0b0a1b1 = cs_mm_unpacklo_epi32(a, b);
		cs__m128i a2b2a3b3 = cs_mm_unpackhi_epi32(a, b);
		samples[i] = cs_mm_packs_epi32(a0b0a1b1, a2b2a3b3);
	}

	cs_push_bytes(samples, bytes_to_write);

#endif

	// Free up any queue'd free's for audio sources at zero refcount.
	for (int i = 0; i < s_ctx->audio_sources_to_free_size;) {
		cs_audio_source_t* audio = s_ctx->audio_sources_to_free[i];
		if (audio->playing_count == 0) {
			cs_free16(audio->channels[0]);
			CUTE_SOUND_FREE(audio, s_mem_ctx);
			s_ctx->audio_sources_to_free[i] = s_ctx->audio_sources_to_free[--s_ctx->audio_sources_to_free_size];
		} else {
			++i;
		}
	}

	unlock:
	cs_unlock();
}

void cs_spawn_mix_thread()
{
	if (s_ctx->separate_thread) return;
	s_ctx->separate_thread = true;
#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS
	CreateThread(0, 0, cs_ctx_thread, s_ctx, 0, 0);
#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE
	pthread_create(&s_ctx->thread, 0, cs_ctx_thread, s_ctx);
#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL
	s_ctx->thread = SDL_CreateThread(&cs_ctx_thread, "CuteSoundThread", s_ctx);
#endif
}

void cs_mix_thread_sleep_delay(int milliseconds)
{
	s_ctx->sleep_milliseconds = milliseconds;
}

void* cs_get_context_ptr()
{
	return (void*)s_ctx;
}

void cs_set_context_ptr(void* ctx)
{
	s_ctx = (cs_context_t*)ctx;
}

// -------------------------------------------------------------------------------------------------
// Loaded sounds.

static void* cs_read_file_to_memory(const char* path, int* size)
{
	void* data = 0;
	CUTE_SOUND_FILE* fp = CUTE_SOUND_FOPEN(path, "rb");
	int sizeNum = 0;

	if (fp) {
		CUTE_SOUND_FSEEK(fp, 0, CUTE_SOUND_SEEK_END);
		sizeNum = (int)CUTE_SOUND_FTELL(fp);
		CUTE_SOUND_FSEEK(fp, 0, CUTE_SOUND_SEEK_SET);
		data = CUTE_SOUND_ALLOC(sizeNum, s_mem_ctx);
		CUTE_SOUND_FREAD(data, sizeNum, 1, fp);
		CUTE_SOUND_FCLOSE(fp);
	}

	if (size) *size = sizeNum;
	return data;
}

static int cs_four_cc(const char* CC, void* memory)
{
	if (!CUTE_SOUND_MEMCMP(CC, memory, 4)) return 1;
	return 0;
}

static char* cs_next(char* data)
{
	uint32_t size = *(uint32_t*)(data + 4);
	size = (size + 1) & ~1;
	return data + 8 + size;
}

static void cs_last_element(cs__m128* a, int i, int j, int16_t* samples, int offset)
{
	switch (offset) {
	case 1:
		a[i] = cs_mm_set_ps(samples[j], 0.0f, 0.0f, 0.0f);
		break;

	case 2:
		a[i] = cs_mm_set_ps(samples[j], samples[j + 1], 0.0f, 0.0f);
		break;

	case 3:
		a[i] = cs_mm_set_ps(samples[j], samples[j + 1], samples[j + 2], 0.0f);
		break;

	case 0:
		a[i] = cs_mm_set_ps(samples[j], samples[j + 1], samples[j + 2], samples[j + 3]);
		break;
	}
}

cs_audio_source_t* cs_load_wav(const char* path, cs_error_t* err /* = NULL */)
{
	int size;
	void* wav = cs_read_file_to_memory(path, &size);
	if (!wav) return NULL;
	cs_audio_source_t* audio = cs_read_mem_wav(wav, size, err);
	CUTE_SOUND_FREE(wav, s_mem_ctx);
	return audio;
}

cs_audio_source_t* cs_read_mem_wav(const void* memory, size_t size, cs_error_t* err)
{
	if (err) *err = CUTE_SOUND_ERROR_NONE;
	if (!memory) { if (err) *err = CUTE_SOUND_ERROR_FILE_NOT_FOUND; return NULL; }

	#pragma pack(push, 1)
	typedef struct
	{
		uint16_t wFormatTag;
		uint16_t nChannels;
		uint32_t nSamplesPerSec;
		uint32_t nAvgBytesPerSec;
		uint16_t nBlockAlign;
		uint16_t wBitsPerSample;
		uint16_t cbSize;
		uint16_t wValidBitsPerSample;
		uint32_t dwChannelMask;
		uint8_t SubFormat[18];
	} Fmt;
	#pragma pack(pop)

	cs_audio_source_t* audio = NULL;
	char* data = (char*)memory;
	char* end = data + size;
	if (!cs_four_cc("RIFF", data)) { if (err) *err = CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE; return NULL; }
	if (!cs_four_cc("WAVE", data + 8)) { if (err) *err = CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE; return NULL; }

	data += 12;

	while (1) {
		if (!(end > data)) { if (err) *err = CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND; return NULL; }
		if (cs_four_cc("fmt ", data)) break;
		data = cs_next(data);
	}

	Fmt fmt;
	fmt = *(Fmt*)(data + 8);
	if (fmt.wFormatTag != 1) { if (err) *err = CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND; return NULL; }
	if (!(fmt.nChannels == 1 || fmt.nChannels == 2)) { if (err) *err = CUTE_SOUND_ERROR_WAV_ONLY_MONO_OR_STEREO_IS_SUPPORTED; return NULL; }
	if (!(fmt.wBitsPerSample == 16)) { if (err) *err = CUTE_SOUND_ERROR_WAV_ONLY_16_BITS_PER_SAMPLE_SUPPORTED; return NULL; }
	if (!(fmt.nBlockAlign == fmt.nChannels * 2)) { if (err) *err = CUTE_SOUND_ERROR_IMPLEMENTATION_ERROR_PLEASE_REPORT_THIS_ON_GITHUB; return NULL; }

	while (1) {
		if (!(end > data)) { if (err) *err = CUTE_SOUND_ERROR_WAV_DATA_CHUNK_NOT_FOUND; return NULL; }
		if (cs_four_cc("data", data)) break;
		data = cs_next(data);
	}

	audio = (cs_audio_source_t*)CUTE_SOUND_ALLOC(sizeof(cs_audio_source_t), s_mem_ctx);
	CUTE_SOUND_MEMSET(audio, 0, sizeof(*audio));
	audio->sample_rate = (int)fmt.nSamplesPerSec;

	{
		int sample_size = *((uint32_t*)(data + 4));
		int sample_count = sample_size / (fmt.nChannels * sizeof(uint16_t));
		//to account for interpolation in the pitch shifter, we lie about length
		//this fixes random popping at the end of sounds
		audio->sample_count = sample_count-1;
		audio->channel_count = fmt.nChannels;

		int wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4) / 4;
		int wide_offset = sample_count & 3;
		int16_t* samples = (int16_t*)(data + 8);

		switch (audio->channel_count) {
		case 1:
		{
			audio->channels[0] = cs_malloc16(wide_count * sizeof(cs__m128));
			audio->channels[1] = 0;
			cs__m128* a = (cs__m128*)audio->channels[0];
			for (int i = 0, j = 0; i < wide_count - 1; ++i, j += 4) {
				a[i] = cs_mm_set_ps((float)samples[j+3], (float)samples[j+2], (float)samples[j+1], (float)samples[j]);
			}
			cs_last_element(a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset);
		}	break;

		case 2:
		{
			cs__m128* a = (cs__m128*)cs_malloc16(wide_count * sizeof(cs__m128) * 2);
			cs__m128* b = a + wide_count;
			for (int i = 0, j = 0; i < wide_count - 1; ++i, j += 8){
				a[i] = cs_mm_set_ps((float)samples[j+6], (float)samples[j+4], (float)samples[j+2], (float)samples[j]);
				b[i] = cs_mm_set_ps((float)samples[j+7], (float)samples[j+5], (float)samples[j+3], (float)samples[j+1]);
			}
			cs_last_element(a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset);
			cs_last_element(b, wide_count - 1, (wide_count - 1) * 4 + 4, samples, wide_offset);
			audio->channels[0] = a;
			audio->channels[1] = b;
		}	break;

		default:
			if (err) *err = CUTE_SOUND_ERROR_WAV_ONLY_MONO_OR_STEREO_IS_SUPPORTED;
			CUTE_SOUND_ASSERT(false);
		}
	}

	if (err) *err = CUTE_SOUND_ERROR_NONE;
	return audio;
}

void cs_free_audio_source(cs_audio_source_t* audio)
{
	if (s_ctx) {
		cs_lock();
		if (audio->playing_count == 0) {
			cs_free16(audio->channels[0]);
			CUTE_SOUND_FREE(audio, s_mem_ctx);
		} else {
			if (s_ctx->audio_sources_to_free_size == s_ctx->audio_sources_to_free_capacity) {
				int new_capacity = s_ctx->audio_sources_to_free_capacity * 2;
				cs_audio_source_t** new_sources = (cs_audio_source_t**)CUTE_SOUND_ALLOC(new_capacity, s_mem_ctx);
				CUTE_SOUND_MEMCPY(new_sources, s_ctx->audio_sources_to_free, sizeof(cs_audio_source_t*) * s_ctx->audio_sources_to_free_size);
				CUTE_SOUND_FREE(s_ctx->audio_sources_to_free, s_mem_ctx);
				s_ctx->audio_sources_to_free = new_sources;
				s_ctx->audio_sources_to_free_capacity = new_capacity;
			}
			s_ctx->audio_sources_to_free[s_ctx->audio_sources_to_free_size++] = audio;
		}
		cs_unlock();
	} else {
		CUTE_SOUND_ASSERT(audio->playing_count == 0);
		cs_free16(audio->channels[0]);
		CUTE_SOUND_FREE(audio, s_mem_ctx);
	}
}

int cs_get_sample_rate(const cs_audio_source_t* audio)
{
	return audio->sample_rate;   
}

int cs_get_sample_count(const cs_audio_source_t* audio)
{
	return audio->sample_count;
}

int cs_get_channel_count(const cs_audio_source_t* audio)
{
	return audio->channel_count;
}

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL && defined(SDL_rwops_h_) && defined(CUTE_SOUND_SDL_RWOPS)

	// Load an SDL_RWops object's data into memory.
	// Ripped straight from: https://wiki.libsdl.org/SDL_RWread
	static void* cs_read_rw_to_memory(SDL_RWops* rw, int* size)
	{
		Sint64 res_size = SDL_RWsize(rw);
		char* data = (char*)CUTE_SOUND_ALLOC((size_t)(res_size + 1), s_mem_ctx);

		Sint64 nb_read_total = 0, nb_read = 1;
		char* buf = data;
		while (nb_read_total < res_size && nb_read != 0)
		{
			nb_read = SDL_RWread(rw, buf, 1, (size_t)(res_size - nb_read_total));
			nb_read_total += nb_read;
			buf += nb_read;
		}

		SDL_RWclose(rw);

		if (nb_read_total != res_size)
		{
			CUTE_SOUND_FREE(data, s_mem_ctx);
			return NULL;
		}

		if (size) *size = (int)res_size;
		return data;
	}

	cs_audio_source_t* cs_load_wav_rw(SDL_RWops* context, cs_error_t* err)
	{
		int size;
		char* wav = (char*)cs_read_rw_to_memory(context, &size);
		if (!memory) return NULL;
		cs_audio_source_t* audio = cs_read_mem_wav(wav, length, err);
		CUTE_SOUND_FREE(wav, s_mem_ctx);
		return audio;
	}

#endif

// If stb_vorbis was included *before* cute_sound go ahead and create
// some functions for dealing with OGG files.
#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H

cs_audio_source_t* cs_read_mem_ogg(const void* memory, size_t length, cs_error_t* err)
{
	int16_t* samples = 0;
	cs_audio_source_t* audio = NULL;
	int channel_count;
	int sample_rate;
	int sample_count = stb_vorbis_decode_memory((const unsigned char*)memory, (int)length, &channel_count, &sample_rate, &samples);
	if (sample_count <= 0) { if (err) *err = CUTE_SOUND_ERROR_STB_VORBIS_DECODE_FAILED; return NULL; }
	audio = (cs_audio_source_t*)CUTE_SOUND_ALLOC(sizeof(cs_audio_source_t), s_mem_ctx);
	CUTE_SOUND_MEMSET(audio, 0, sizeof(*audio));

	{
		int wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4) / 4;
		int wide_offset = sample_count & 3;
		cs__m128* a = NULL;
		cs__m128* b = NULL;

		switch (channel_count)
		{
		case 1:
		{
			a = (cs__m128*)cs_malloc16(wide_count * sizeof(cs__m128));
			b = 0;
			for (int i = 0, j = 0; i < wide_count - 1; ++i, j += 4) {
				a[i] = cs_mm_set_ps((float)samples[j+3], (float)samples[j+2], (float)samples[j+1], (float)samples[j]);
			}
			cs_last_element(a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset);
		}	break;

		case 2:
			a = (cs__m128*)cs_malloc16(wide_count * sizeof(cs__m128) * 2);
			b = a + wide_count;
			for (int i = 0, j = 0; i < wide_count - 1; ++i, j += 8) {
				a[i] = cs_mm_set_ps((float)samples[j+6], (float)samples[j+4], (float)samples[j+2], (float)samples[j]);
				b[i] = cs_mm_set_ps((float)samples[j+7], (float)samples[j+5], (float)samples[j+3], (float)samples[j+1]);
			}
			cs_last_element(a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset);
			cs_last_element(b, wide_count - 1, (wide_count - 1) * 4 + 4, samples, wide_offset);
			break;

		default:
			if (err) *err = CUTE_SOUND_ERROR_OGG_UNSUPPORTED_CHANNEL_COUNT;
			CUTE_SOUND_ASSERT(false);
		}

		audio->sample_rate = sample_rate;
		audio->sample_count = sample_count;
		audio->channel_count = channel_count;
		audio->channels[0] = a;
		audio->channels[1] = b;
		audio->playing_count = 0;
		CUTE_SOUND_FREE(samples, s_mem_ctx);
	}

	if (err) *err = CUTE_SOUND_ERROR_NONE;
	return audio;
}

cs_audio_source_t* cs_load_ogg(const char* path, cs_error_t* err)
{
	int length;
	void* memory = cs_read_file_to_memory(path, &length);
	if (!memory) return NULL;
	cs_audio_source_t* audio = cs_read_mem_ogg(memory, length, err);
	CUTE_SOUND_FREE(memory, s_mem_ctx);
	return audio;
}

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL && defined(SDL_rwops_h_) && defined(CUTE_SOUND_SDL_RWOPS)

	cs_audio_source_t* cs_load_ogg_rw(SDL_RWops* rw, cs_error_t* err)
	{
		int length;
		void* memory = cs_read_rw_to_memory(rw, &length);
		if (!memory) return NULL;
		cs_audio_source_t* audio = cs_read_ogg_wav(memory, length, err);
		CUTE_SOUND_FREE(memory, s_mem_ctx);
		return audio;
	}

#endif
#endif // STB_VORBIS_INCLUDE_STB_VORBIS_H

// -------------------------------------------------------------------------------------------------
// Music sounds.

static void s_insert(cs_sound_inst_t* inst)
{
	cs_lock();
	cs_list_push_back(&s_ctx->playing_sounds, &inst->node);
	inst->audio->playing_count += 1;
	inst->active = true;
	inst->id = s_ctx->instance_id_gen++;
	cs_hashtableinsert(&s_ctx->instance_map, inst->id, &inst);
	cs_unlock();
}

static cs_sound_inst_t* s_inst_music(cs_audio_source_t* src, float volume)
{
	if (cs_list_empty(&s_ctx->free_sounds)) {
		s_add_page();
	}
	CUTE_SOUND_ASSERT(!cs_list_empty(&s_ctx->free_sounds));
	cs_sound_inst_t* inst = CUTE_SOUND_LIST_HOST(cs_sound_inst_t, node, cs_list_pop_back(&s_ctx->free_sounds));
	inst->is_music = true;
	inst->looped = s_ctx->music_looped;
	if (!s_ctx->music_paused) inst->paused = false;
	inst->volume = volume;
	inst->pan0 = 0.5f;
	inst->pan1 = 0.5f;
	inst->pitch = 1.0f;
	inst->audio = src;
	inst->sample_index = 0;
	cs_list_init_node(&inst->node);
	s_insert(inst);
	return inst;
}

static cs_sound_inst_t* s_inst(cs_audio_source_t* src, cs_sound_params_t params)
{
	if (cs_list_empty(&s_ctx->free_sounds)) {
		s_add_page();
	}
	CUTE_SOUND_ASSERT(!cs_list_empty(&s_ctx->free_sounds));
	cs_sound_inst_t* inst = CUTE_SOUND_LIST_HOST(cs_sound_inst_t, node, cs_list_pop_back(&s_ctx->free_sounds));
	float pan = params.pan;
	if (pan > 1.0f) pan = 1.0f;
	else if (pan < 0.0f) pan = 0.0f;
	float panl = 1.0f - pan;
	float panr = pan;
	inst->is_music = false;
	inst->paused = params.paused;
	inst->looped = params.looped;
	inst->volume = params.volume;
	inst->pan0 = panl;
	inst->pan1 = panr;
	inst->pitch = params.pitch;
	inst->audio = src;
	inst->sample_index = params.sample_index;
	CUTE_SOUND_ASSERT(inst->sample_index < src->sample_count);
	cs_list_init_node(&inst->node);
	s_insert(inst);
	return inst;
}

void cs_music_play(cs_audio_source_t* audio_source, float fade_in_time)
{
	if (s_ctx->music_state != CUTE_SOUND_MUSIC_STATE_PLAYING) {
		cs_music_stop(0);
	}

	if (fade_in_time < 0) fade_in_time = 0;
	if (fade_in_time) {
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_FADE_IN;
	} else {
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_PLAYING;
	}
	s_ctx->fade = fade_in_time;
	s_ctx->t = 0;

	cs_sound_inst_t* inst = s_inst_music(audio_source, fade_in_time == 0 ? 1.0f : 0);
	s_ctx->music_playing = inst;
	s_ctx->music_next = NULL;
}

void cs_music_stop(float fade_out_time)
{
	if (fade_out_time < 0) fade_out_time = 0;

	if (fade_out_time == 0) {
		// Immediately turn off all music if no fade out time.
		if (s_ctx->music_playing) {
			s_ctx->music_playing->active = false;
			s_ctx->music_playing->paused = false;
		}
		if (s_ctx->music_next) {
			s_ctx->music_next->active = false;
			s_ctx->music_next->paused = false;
		}
		s_ctx->music_playing = NULL;
		s_ctx->music_next = NULL;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_NONE;
	} else {
		switch (s_ctx->music_state) {
		case CUTE_SOUND_MUSIC_STATE_NONE:
			break;

		case CUTE_SOUND_MUSIC_STATE_PLAYING:
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_FADE_OUT;
			s_ctx->fade = fade_out_time;
			s_ctx->t = 0;
			break;

		case CUTE_SOUND_MUSIC_STATE_FADE_OUT:
			break;

		case CUTE_SOUND_MUSIC_STATE_FADE_IN:
		{
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_FADE_OUT;
			s_ctx->t = s_smoothstep(((s_ctx->fade - s_ctx->t) / s_ctx->fade));
			s_ctx->fade = fade_out_time;
		}	break;

		case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0:
		{
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_FADE_OUT;
			s_ctx->t = s_smoothstep(((s_ctx->fade - s_ctx->t) / s_ctx->fade));
			s_ctx->fade = fade_out_time;
			s_ctx->music_next = NULL;
		}	break;

		case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1:
			// Fall-through.

		case CUTE_SOUND_MUSIC_STATE_CROSSFADE:
		{
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_FADE_OUT;
			s_ctx->t = s_smoothstep(((s_ctx->fade - s_ctx->t) / s_ctx->fade));
			s_ctx->fade = fade_out_time;
			s_ctx->music_playing = s_ctx->music_next;
			s_ctx->music_next = NULL;
		}	break;

		case CUTE_SOUND_MUSIC_STATE_PAUSED:
			cs_music_stop(0);
		}
	}
}

void cs_music_set_volume(float volume_0_to_1)
{
	if (volume_0_to_1 < 0) volume_0_to_1 = 0;
	s_ctx->music_volume = volume_0_to_1;
	if (s_ctx->music_playing) s_ctx->music_playing->volume = volume_0_to_1;
	if (s_ctx->music_next) s_ctx->music_next->volume = volume_0_to_1;
}

void cs_music_set_pitch(float pitch)
{
	s_ctx->music_pitch = pitch;
	if (s_ctx->music_playing) s_ctx->music_playing->pitch = pitch;
	if (s_ctx->music_next) s_ctx->music_next->pitch = pitch;
}

void cs_music_set_loop(bool true_to_loop)
{
	s_ctx->music_looped = true_to_loop;
	if (s_ctx->music_playing) s_ctx->music_playing->looped = true_to_loop;
	if (s_ctx->music_next) s_ctx->music_next->looped = true_to_loop;
}

void cs_music_pause()
{
	if (s_ctx->music_state == CUTE_SOUND_MUSIC_STATE_PAUSED) return;
	if (s_ctx->music_playing) s_ctx->music_playing->paused = true;
	if (s_ctx->music_next) s_ctx->music_next->paused = true;
	s_ctx->music_paused = true;
	s_ctx->music_state_to_resume_from_paused = s_ctx->music_state;
	s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_PAUSED;
}

void cs_music_resume()
{
	if (s_ctx->music_state != CUTE_SOUND_MUSIC_STATE_PAUSED) return;
	if (s_ctx->music_playing) s_ctx->music_playing->paused = false;
	if (s_ctx->music_next) s_ctx->music_next->paused = false;
	s_ctx->music_state = s_ctx->music_state_to_resume_from_paused;
}

void cs_music_switch_to(cs_audio_source_t* audio_source, float fade_out_time, float fade_in_time)
{
	if (fade_in_time < 0) fade_in_time = 0;
	if (fade_out_time < 0) fade_out_time = 0;

	switch (s_ctx->music_state) {
	case CUTE_SOUND_MUSIC_STATE_NONE:
		cs_music_play(audio_source, fade_in_time);
		break;

	case CUTE_SOUND_MUSIC_STATE_PLAYING:
	{
		CUTE_SOUND_ASSERT(s_ctx->music_next == NULL);
		cs_sound_inst_t* inst = s_inst_music(audio_source, fade_in_time == 0 ? 1.0f : 0);
		s_ctx->music_next = inst;

		s_ctx->fade = fade_out_time;
		s_ctx->fade_switch_1 = fade_in_time;
		s_ctx->t = 0;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_FADE_OUT:
	{
		CUTE_SOUND_ASSERT(s_ctx->music_next == NULL);
		cs_sound_inst_t* inst = s_inst_music(audio_source, fade_in_time == 0 ? 1.0f : 0);
		s_ctx->music_next = inst;

		s_ctx->fade_switch_1 = fade_in_time;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_FADE_IN:
	{
		CUTE_SOUND_ASSERT(s_ctx->music_next == NULL);
		cs_sound_inst_t* inst = s_inst_music(audio_source, fade_in_time == 0 ? 1.0f : 0);
		s_ctx->music_next = inst;

		s_ctx->fade_switch_1 = fade_in_time;
		s_ctx->t = s_smoothstep(((s_ctx->fade - s_ctx->t) / s_ctx->fade));
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0:
	{
		CUTE_SOUND_ASSERT(s_ctx->music_next != NULL);
		cs_sound_inst_t* inst = s_inst_music(audio_source, fade_in_time == 0 ? 1.0f : 0);
		s_ctx->music_next->active = false;
		s_ctx->music_next = inst;
		s_ctx->fade_switch_1 = fade_in_time;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_CROSSFADE: // Fall-through.
	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1:
	{
		CUTE_SOUND_ASSERT(s_ctx->music_next != NULL);
		cs_sound_inst_t* inst = s_inst_music(audio_source, fade_in_time == 0 ? 1.0f : 0);
		s_ctx->music_playing = s_ctx->music_next;
		s_ctx->music_next = inst;

		s_ctx->t = s_smoothstep(((s_ctx->fade - s_ctx->t) / s_ctx->fade));
		s_ctx->fade_switch_1 = fade_in_time;
		s_ctx->fade = fade_out_time;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_PAUSED:
		cs_music_stop(0);
		cs_music_switch_to(audio_source, fade_out_time, fade_in_time);
		break;
	}
}

void cs_music_crossfade(cs_audio_source_t* audio_source, float cross_fade_time)
{
	if (cross_fade_time < 0) cross_fade_time = 0;

	switch (s_ctx->music_state) {
	case CUTE_SOUND_MUSIC_STATE_NONE:
		cs_music_play(audio_source, cross_fade_time);

	case CUTE_SOUND_MUSIC_STATE_PLAYING:
	{
		CUTE_SOUND_ASSERT(s_ctx->music_next == NULL);
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_CROSSFADE;

		cs_sound_inst_t* inst = s_inst_music(audio_source, cross_fade_time == 0 ? 1.0f : 0);
		inst->paused = false;
		s_ctx->music_next = inst;

		s_ctx->fade = cross_fade_time;
		s_ctx->t = 0;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_FADE_OUT:
		CUTE_SOUND_ASSERT(s_ctx->music_next == NULL);
		// Fall-through.

	case CUTE_SOUND_MUSIC_STATE_FADE_IN:
	{
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_CROSSFADE;

		cs_sound_inst_t* inst = s_inst_music(audio_source, cross_fade_time == 0 ? 1.0f : 0);
		inst->paused = false;
		s_ctx->music_next = inst;

		s_ctx->fade = cross_fade_time;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0:
	{
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_CROSSFADE;
		s_ctx->music_next->active = false;

		cs_sound_inst_t* inst = s_inst_music(audio_source, cross_fade_time == 0 ? 1.0f : 0);
		inst->paused = false;
		s_ctx->music_next = inst;

		s_ctx->fade = cross_fade_time;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1: // Fall-through.
	case CUTE_SOUND_MUSIC_STATE_CROSSFADE:
	{
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_CROSSFADE;
		s_ctx->music_playing->active = false;
		s_ctx->music_playing = s_ctx->music_next;

		cs_sound_inst_t* inst = s_inst_music(audio_source, cross_fade_time == 0 ? 1.0f : 0);
		inst->paused = false;
		s_ctx->music_next = inst;

		s_ctx->fade = cross_fade_time;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_PAUSED:
		cs_music_stop(0);
		cs_music_crossfade(audio_source, cross_fade_time);
	}
}

int cs_music_get_sample_index()
{
	if (s_ctx->music_playing) return 0;
	else return s_ctx->music_playing->sample_index;
}

cs_error_t cs_music_set_sample_index(int sample_index)
{
	if (s_ctx->music_playing) return CUTE_SOUND_ERROR_INVALID_SOUND;
	if (sample_index > s_ctx->music_playing->audio->sample_count) return CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT;
	s_ctx->music_playing->sample_index = sample_index;
	return CUTE_SOUND_ERROR_NONE;
}

// -------------------------------------------------------------------------------------------------
// Playing sounds.

cs_sound_params_t cs_sound_params_default()
{
	cs_sound_params_t params;
	params.paused = false;
	params.looped = false;
	params.volume = 1.0f;
	params.pan = 0.5f;
	params.pitch = 1.0f;
	params.sample_index = 0;
	return params;
}

static cs_sound_inst_t* s_get_inst(cs_playing_sound_t sound)
{
	cs_sound_inst_t** inst = (cs_sound_inst_t**)cs_hashtablefind(&s_ctx->instance_map, sound.id);
	if (inst) return *inst;
	return NULL;
}

cs_playing_sound_t cs_play_sound(cs_audio_source_t* audio, cs_sound_params_t params)
{
	cs_sound_inst_t* inst = s_inst(audio, params);
	cs_playing_sound_t sound = { inst->id };
	return sound;
}

void cs_on_sound_finished_callback(void (*on_finish)(cs_playing_sound_t, void*), void* udata)
{
	s_ctx->on_finish = on_finish;
	s_ctx->on_finish_udata = udata;
}

void cs_on_music_finished_callback(void (*on_finish)(void*), void* udata)
{
	s_ctx->on_music_finish = on_finish;
	s_ctx->on_music_finish_udata = udata;
}

bool cs_sound_is_active(cs_playing_sound_t sound)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return false;
	return inst->active;
}

bool cs_sound_get_is_paused(cs_playing_sound_t sound)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return false;
	return inst->paused;
}

bool cs_sound_get_is_looped(cs_playing_sound_t sound)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return false;
	return inst->looped;
}

float cs_sound_get_volume(cs_playing_sound_t sound)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return 0;
	return inst->volume;
}

float cs_sound_get_pitch(cs_playing_sound_t sound)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return 0;
	return inst->pitch;
}

float cs_sound_get_pan(cs_playing_sound_t sound)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return 0;
	return inst->pan1;
}

int cs_sound_get_sample_index(cs_playing_sound_t sound)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return 0;
	return inst->sample_index;
}

void cs_sound_set_is_paused(cs_playing_sound_t sound, bool true_for_paused)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return;
	inst->paused = true_for_paused;
}

void cs_sound_set_is_looped(cs_playing_sound_t sound, bool true_for_looped)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return;
	inst->looped = true_for_looped;
}

void cs_sound_set_volume(cs_playing_sound_t sound, float volume_0_to_1)
{
	if (volume_0_to_1 < 0) volume_0_to_1 = 0;
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return;
	inst->volume = volume_0_to_1;
}

void cs_sound_set_pitch(cs_playing_sound_t sound, float pitch)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return;
	inst->pitch = pitch;
}

void cs_sound_set_pan(cs_playing_sound_t sound, float pan_0_to_1)
{
	if (pan_0_to_1 < 0) pan_0_to_1 = 0;
	if (pan_0_to_1 > 1) pan_0_to_1 = 1;
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return;
	inst->pan0 = 1.0f - pan_0_to_1;
	inst->pan1 = pan_0_to_1;
}

cs_error_t cs_sound_set_sample_index(cs_playing_sound_t sound, int sample_index)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return CUTE_SOUND_ERROR_INVALID_SOUND;
	if (sample_index > inst->audio->sample_count) return CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT;
	inst->sample_index = sample_index;
	return CUTE_SOUND_ERROR_NONE;
}

void cs_sound_stop(cs_playing_sound_t sound)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return;
	inst->active = false;
}

void cs_set_playing_sounds_volume(float volume_0_to_1)
{
	if (volume_0_to_1 < 0) volume_0_to_1 = 0;
	s_ctx->sound_volume = volume_0_to_1;
}

void cs_stop_all_playing_sounds()
{
	cs_lock();

	// Set all playing sounds (that aren't music) active to false.
	if (cs_list_empty(&s_ctx->playing_sounds)) {
		cs_unlock();
		return;
	}
	cs_list_node_t* playing_sound = cs_list_begin(&s_ctx->playing_sounds);
	cs_list_node_t* end = cs_list_end(&s_ctx->playing_sounds);

	do {
		cs_sound_inst_t* inst = CUTE_SOUND_LIST_HOST(cs_sound_inst_t, node, playing_sound);
		cs_list_node_t* next = playing_sound->next;
		if (inst != s_ctx->music_playing && inst != s_ctx->music_next) {
			inst->active = false; // Let cs_mix handle cleaning this up.
		}
		playing_sound = next;
	} while (playing_sound != end);

	cs_unlock();
}

void cs_cull_duplicates(bool true_to_enable)
{
	s_ctx->cull_duplicates = true_to_enable;
}

void* cs_get_global_context()
{
	return s_ctx;
}

void cs_set_global_context(void* context)
{
	s_ctx = (cs_context_t*)context;
}

void* cs_get_global_user_allocator_context()
{
	return s_mem_ctx;
}

void cs_set_global_user_allocator_context(void* user_allocator_context)
{
	s_mem_ctx = user_allocator_context;
}

#endif // CUTE_SOUND_IMPLEMENTATION_ONCE
#endif // CUTE_SOUND_IMPLEMENTATION

/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2023 Randy Gaul https://randygaul.github.io/
	This software is provided 'as-is', without any express or implied warranty.
	In no event will the authors be held liable for any damages arising from
	the use of this software.
	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:
	  1. The origin of this software must not be misrepresented; you must not
		 claim that you wrote the original software. If you use this software
		 in a product, an acknowledgment in the product documentation would be
		 appreciated but is not required.
	  2. Altered source versions must be plainly marked as such, and must not
		 be misrepresented as being the original software.
	  3. This notice may not be removed or altered from any source distribution.
	------------------------------------------------------------------------------
	ALTERNATIVE B - Public Domain (www.unlicense.org)
	This is free and unencumbered software released into the public domain.
	Anyone is free to copy, modify, publish, use, compile, sell, or distribute this
	software, either in source code form or as a compiled binary, for any purpose,
	commercial or non-commercial, and by any means.
	In jurisdictions that recognize copyright laws, the author or authors of this
	software dedicate any and all copyright interest in the software to the public
	domain. We make this dedication for the benefit of the public at large and to
	the detriment of our heirs and successors. We intend this dedication to be an
	overt act of relinquishment in perpetuity of all present and future rights to
	this software under copyright law.
	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
	ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
	------------------------------------------------------------------------------
*/
