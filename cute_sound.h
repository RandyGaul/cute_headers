/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_sound.h - v3.00

	To create implementation (the function definitions)
		#define CUTE_SOUND_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file


	SUMMARY

		cute_sound is a C API for loading, playing, looping, panning and fading mono
		and stereo sounds. It requires SDL3 for audio output, and optionally
		stb_vorbis.c for ogg file loading.


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
		2.09 (08/10/2024) Upgrade to SDL3.
		3.00 (02/01/2026) Removed platform-specific code, now SDL3 only.
		                * Simplified hashtable and removed redundant circular buffer.
		                * Fixed SIMD sample loading bugs causing audio popping.
		                * Fixed pitch shift bug at end of tracks (interpolate beyond last sample).
		                * Refactored mixer to be simpler.
		                * Replaced sample_index API with time values in seconds.
		                * Fixed looped/reverse playback pitch shifting (audio crackling).
		                * Removed cs_cull_duplicates (better handled user-side).
		                * Fixed music fade to incorporate user volume.
		                * WAV loader now supports 8/16/24/32-bit PCM and 32/64-bit float.


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


	DOCUMENTATION (very quick intro)

		Steps to play audio:

			1. create context (call cs_init)
			2. load sounds from disk into memory (call cs_load_wav, or cs_load_ogg with stb_vorbis.c)
			3. play sounds (cs_play_sound), or music (cs_music_play)

	DISABLE SSE/NEON SIMD ACCELERATION

		If for whatever reason you don't want to use SIMD intrinsics and instead would prefer
		plain C (for example if your platform does not support SSE/NEON) then define
		CUTE_SOUND_SCALAR_MODE before including cute_sound.h while also defining the
		symbol definitions. Here's an example:

			#define CUTE_SOUND_SCALAR_MODE
			#define CUTE_SOUND_IMPLEMENTATION
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
	CUTE_SOUND_ERROR_FILE_NOT_FOUND,
	CUTE_SOUND_ERROR_INVALID_SOUND,
	CUTE_SOUND_ERROR_CANT_OPEN_AUDIO_DEVICE,
	CUTE_SOUND_ERROR_CANT_INIT_SDL_AUDIO,
	CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE,
	CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND,
	CUTE_SOUND_ERROR_WAV_DATA_CHUNK_NOT_FOUND,
	CUTE_SOUND_ERROR_WAV_ONLY_MONO_OR_STEREO_IS_SUPPORTED,
	CUTE_SOUND_ERROR_WAV_UNSUPPORTED_FORMAT,
	CUTE_SOUND_ERROR_CANNOT_SWITCH_MUSIC_WHILE_PAUSED,
	CUTE_SOUND_ERROR_CANNOT_CROSSFADE_WHILE_MUSIC_IS_PAUSED,
	CUTE_SOUND_ERROR_CANNOT_FADEOUT_WHILE_MUSIC_IS_PAUSED,
	CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT,
	CUTE_SOUND_ERROR_STB_VORBIS_DECODE_FAILED,
	CUTE_SOUND_ERROR_OGG_UNSUPPORTED_CHANNEL_COUNT,
	CUTE_SOUND_ERROR_IMPLEMENTATION_ERROR_PLEASE_REPORT_THIS_ON_GITHUB, // https://github.com/RandyGaul/cute_headers/blob/master/cute_sound.h
} cs_error_t;

const char* cs_error_as_string(cs_error_t error);

// -------------------------------------------------------------------------------------------------
// Cute sound context functions.

/**
 * play_frequency_in_Hz depends on your audio files, 44100 is typical.
 */
cs_error_t cs_init(unsigned play_frequency_in_Hz, void* user_allocator_context /* = NULL */);
void cs_shutdown();

/**
 * Call this function once per game-tick.
 */
void cs_update(float dt);
void cs_set_global_volume(float volume_0_to_1);
void cs_set_global_pan(float pan_0_to_1);
void cs_set_global_pause(bool true_for_paused);

/**
 * Sometimes useful for dynamic library shenanigans.
 */
void* cs_get_context_ptr();
void cs_set_context_ptr(void* ctx);

// -------------------------------------------------------------------------------------------------
// Loaded sounds.

typedef struct cs_audio_source_t cs_audio_source_t;

/**
 * Load a WAV file from disk or memory.
 *
 * Supported WAV formats:
 *   - PCM 8-bit unsigned
 *   - PCM 16-bit signed
 *   - PCM 24-bit signed
 *   - PCM 32-bit signed
 *   - IEEE float 32-bit
 *   - IEEE float 64-bit
 *
 * Only mono and stereo channel configurations are supported.
 */
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
double cs_music_get_time();
cs_error_t cs_music_set_time(double time_in_seconds);

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
	double start_time /* = 0.0 */; // Start time in seconds.
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
double cs_sound_get_time(cs_playing_sound_t sound);
void cs_sound_set_is_paused(cs_playing_sound_t sound, bool true_for_paused);
void cs_sound_set_is_looped(cs_playing_sound_t sound, bool true_for_looped);
void cs_sound_set_volume(cs_playing_sound_t sound, float volume_0_to_1);
void cs_sound_set_pan(cs_playing_sound_t sound, float pan_0_to_1);
void cs_sound_set_pitch(cs_playing_sound_t sound, float pitch);
cs_error_t cs_sound_set_time(cs_playing_sound_t sound, double time_in_seconds);
void cs_sound_stop(cs_playing_sound_t sound);

void cs_set_playing_sounds_volume(float volume_0_to_1);
void cs_stop_all_playing_sounds();

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

// Internal mixer buffer size in samples. 4096 samples at 44100Hz is ~93ms.
#ifndef CUTE_SOUND_MIXER_BUFFER_SIZE
#	define CUTE_SOUND_MIXER_BUFFER_SIZE 4096
#endif

// Bytes per sample frame (16-bit stereo = 4 bytes).
#define CUTE_SOUND_BYTES_PER_SAMPLE_FRAME 4

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

// SDL3 is required.
#ifndef SDL_h_
	#ifndef CUTE_SOUND_SDL_H
		#define CUTE_SOUND_SDL_H <SDL3/SDL.h>
	#endif
	#include CUTE_SOUND_SDL_H
#endif

// Automatically select SSE/NEON, or default to scalar if `CUTE_SOUND_SCALAR_MODE` is defined.
#if !defined(CUTE_SOUND_SCALAR_MODE) && defined(__ARM_NEON) || defined(__ARM_NEON__)

	#include <arm_neon.h>

	#define cs__m128 float32x4_t
	#define cs__m128i int32x4_t

	#define cs_mm_set_ps(e3, e2, e1, e0) vsetq_lane_f32(e3, vsetq_lane_f32(e2, vsetq_lane_f32(e1, vsetq_lane_f32(e0, vdupq_n_f32(0), 0), 1), 2), 3)
	#define cs_mm_set1_ps(e) vdupq_n_f32(e)
	#define cs_mm_add_ps(a, b) vaddq_f32(a, b)
	#define cs_mm_sub_ps(a, b) vsubq_f32(a, b)
	#define cs_mm_mul_ps(a, b) vmulq_f32(a, b)
	#define cs_mm_cvtps_epi32(a) vcvtq_s32_f32(a)
	#define cs_mm_unpacklo_epi32(a, b) vzipq_s32(a, b).val[0]
	#define cs_mm_unpackhi_epi32(a, b) vzipq_s32(a, b).val[1]
	#define cs_mm_packs_epi32(a, b) vcombine_s16(vqmovn_s32(a), vqmovn_s32(b))
	#define cs_mm_cvttps_epi32(a) vcvtq_s32_f32(a)
	#define cs_mm_cvtepi32_ps(a) vcvtq_f32_s32(a)
	#define cs_mm_extract_epi32(a, imm8) vgetq_lane_s32(a, imm8)
	#define cs_mm_set1_epi32(a) vdupq_n_s32(a)
	#define cs_mm_sub_epi32(a, b) vsubq_s32(a, b)
	#define cs_mm_and_si128(a, b) vandq_s32(a, b)
	#define cs_mm_cmplt_ps(a, b) vreinterpretq_f32_u32(vcltq_f32(a, b))
	#define cs_mm_castps_si128(a) vreinterpretq_s32_f32(a)

#elif !defined(CUTE_SOUND_SCALAR_MODE) && defined(__SSE__) || defined(__SSE2__) || defined(__SSE3__) || defined(__SSE4_1__) || defined(__SSE4_2__)

	#include <immintrin.h>

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
	#define cs_mm_set1_epi32 _mm_set1_epi32
	#define cs_mm_sub_epi32 _mm_sub_epi32
	#define cs_mm_and_si128 _mm_and_si128
	#define cs_mm_cmplt_ps _mm_cmplt_ps
	#define cs_mm_castps_si128 _mm_castps_si128

#else // Scalar mode as fallback.

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

	cs__m128i cs_mm_set1_epi32(int32_t e)
	{
		cs__m128i a;
		a.a = e;
		a.b = e;
		a.c = e;
		a.d = e;
		return a;
	}

	cs__m128i cs_mm_sub_epi32(cs__m128i a, cs__m128i b)
	{
		cs__m128i c;
		c.a = a.a - b.a;
		c.b = a.b - b.b;
		c.c = a.c - b.c;
		c.d = a.d - b.d;
		return c;
	}

	cs__m128i cs_mm_and_si128(cs__m128i a, cs__m128i b)
	{
		cs__m128i c;
		c.a = a.a & b.a;
		c.b = a.b & b.b;
		c.c = a.c & b.c;
		c.d = a.d & b.d;
		return c;
	}

	cs__m128 cs_mm_cmplt_ps(cs__m128 a, cs__m128 b)
	{
		cs__m128 c;
		union { float f; uint32_t u; } mask;
		mask.u = 0xFFFFFFFF;
		c.a = a.a < b.a ? mask.f : 0.0f;
		c.b = a.b < b.b ? mask.f : 0.0f;
		c.c = a.c < b.c ? mask.f : 0.0f;
		c.d = a.d < b.d ? mask.f : 0.0f;
		return c;
	}

	cs__m128i cs_mm_castps_si128(cs__m128 a)
	{
		union { cs__m128 f; cs__m128i i; } u;
		u.f = a;
		return u.i;
	}

#endif // End of SIMD wrappers.

#define CUTE_SOUND_ALIGN(X, Y) ((((size_t)X) + ((Y) - 1)) & ~((Y) - 1))
#define CUTE_SOUND_TRUNC(X, Y) ((size_t)(X) & ~((Y) - 1))

// -------------------------------------------------------------------------------------------------
// Simple hash map for uint64_t keys to void* values.

typedef struct cs_map_slot_t
{
	uint64_t key;
	void* val;
} cs_map_slot_t;

typedef struct cs_map_t
{
	void* memctx;
	cs_map_slot_t* slots;
	int capacity;
	int count;
} cs_map_t;

static uint32_t cs_map_hash(uint64_t key)
{
	key = (~key) + (key << 18);
	key = key ^ (key >> 31);
	key = key * 21;
	key = key ^ (key >> 11);
	key = key + (key << 6);
	key = key ^ (key >> 22);
	return (uint32_t)key;
}

static void cs_map_init(cs_map_t* map, int capacity, void* memctx)
{
	map->memctx = memctx;
	map->capacity = capacity;
	map->count = 0;
	size_t size = (size_t)capacity * sizeof(cs_map_slot_t);
	map->slots = (cs_map_slot_t*)CUTE_SOUND_ALLOC(size, memctx);
	CUTE_SOUND_MEMSET(map->slots, 0, size);
}

static void cs_map_term(cs_map_t* map)
{
	CUTE_SOUND_FREE(map->slots, map->memctx);
}

static void cs_map_grow(cs_map_t* map)
{
	int old_capacity = map->capacity;
	cs_map_slot_t* old_slots = map->slots;

	map->capacity *= 2;
	size_t size = (size_t)map->capacity * sizeof(cs_map_slot_t);
	map->slots = (cs_map_slot_t*)CUTE_SOUND_ALLOC(size, map->memctx);
	CUTE_SOUND_MEMSET(map->slots, 0, size);

	int mask = map->capacity - 1;
	for (int i = 0; i < old_capacity; ++i) {
		if (old_slots[i].key) {
			uint32_t h = cs_map_hash(old_slots[i].key);
			int idx = h & mask;
			while (map->slots[idx].key) {
				idx = (idx + 1) & mask;
			}
			map->slots[idx] = old_slots[i];
		}
	}

	CUTE_SOUND_FREE(old_slots, map->memctx);
}

static void cs_map_insert(cs_map_t* map, uint64_t key, void* val)
{
	CUTE_SOUND_ASSERT(key != 0);
	if (map->count >= map->capacity / 2) {
		cs_map_grow(map);
	}

	int mask = map->capacity - 1;
	uint32_t h = cs_map_hash(key);
	int idx = h & mask;
	while (map->slots[idx].key && map->slots[idx].key != key) {
		idx = (idx + 1) & mask;
	}

	if (!map->slots[idx].key) {
		++map->count;
	}
	map->slots[idx].key = key;
	map->slots[idx].val = val;
}

static void* cs_map_find(cs_map_t* map, uint64_t key)
{
	if (!key) return NULL;
	int mask = map->capacity - 1;
	uint32_t h = cs_map_hash(key);
	int idx = h & mask;
	while (map->slots[idx].key) {
		if (map->slots[idx].key == key) {
			return map->slots[idx].val;
		}
		idx = (idx + 1) & mask;
	}
	return NULL;
}

static void cs_map_remove(cs_map_t* map, uint64_t key)
{
	if (!key) return;
	int mask = map->capacity - 1;
	uint32_t h = cs_map_hash(key);
	int idx = h & mask;

	while (map->slots[idx].key) {
		if (map->slots[idx].key == key) {
			map->slots[idx].key = 0;
			map->slots[idx].val = NULL;
			--map->count;

			// Reinsert subsequent entries to maintain probe chain.
			int next = (idx + 1) & mask;
			while (map->slots[next].key) {
				cs_map_slot_t slot = map->slots[next];
				map->slots[next].key = 0;
				map->slots[next].val = NULL;
				--map->count;
				cs_map_insert(map, slot.key, slot.val);
				next = (next + 1) & mask;
			}
			return;
		}
		idx = (idx + 1) & mask;
	}
}

static void cs_map_clear(cs_map_t* map)
{
	CUTE_SOUND_MEMSET(map->slots, 0, (size_t)map->capacity * sizeof(cs_map_slot_t));
	map->count = 0;
}

// -------------------------------------------------------------------------------------------------

const char* cs_error_as_string(cs_error_t error) {
	switch (error) {
	case CUTE_SOUND_ERROR_NONE: return "CUTE_SOUND_ERROR_NONE";
	case CUTE_SOUND_ERROR_FILE_NOT_FOUND: return "CUTE_SOUND_ERROR_FILE_NOT_FOUND";
	case CUTE_SOUND_ERROR_INVALID_SOUND: return "CUTE_SOUND_ERROR_INVALID_SOUND";
	case CUTE_SOUND_ERROR_CANT_OPEN_AUDIO_DEVICE: return "CUTE_SOUND_ERROR_CANT_OPEN_AUDIO_DEVICE";
	case CUTE_SOUND_ERROR_CANT_INIT_SDL_AUDIO: return "CUTE_SOUND_ERROR_CANT_INIT_SDL_AUDIO";
	case CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE: return "CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE";
	case CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND: return "CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND";
	case CUTE_SOUND_ERROR_WAV_DATA_CHUNK_NOT_FOUND: return "CUTE_SOUND_ERROR_WAV_DATA_CHUNK_NOT_FOUND";
	case CUTE_SOUND_ERROR_WAV_ONLY_MONO_OR_STEREO_IS_SUPPORTED: return "CUTE_SOUND_ERROR_WAV_ONLY_MONO_OR_STEREO_IS_SUPPORTED";
	case CUTE_SOUND_ERROR_WAV_UNSUPPORTED_FORMAT: return "CUTE_SOUND_ERROR_WAV_UNSUPPORTED_FORMAT";
	case CUTE_SOUND_ERROR_CANNOT_SWITCH_MUSIC_WHILE_PAUSED: return "CUTE_SOUND_ERROR_CANNOT_SWITCH_MUSIC_WHILE_PAUSED";
	case CUTE_SOUND_ERROR_CANNOT_CROSSFADE_WHILE_MUSIC_IS_PAUSED: return "CUTE_SOUND_ERROR_CANNOT_CROSSFADE_WHILE_MUSIC_IS_PAUSED";
	case CUTE_SOUND_ERROR_CANNOT_FADEOUT_WHILE_MUSIC_IS_PAUSED: return "CUTE_SOUND_ERROR_CANNOT_FADEOUT_WHILE_MUSIC_IS_PAUSED";
	case CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT: return "CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT";
	case CUTE_SOUND_ERROR_STB_VORBIS_DECODE_FAILED: return "CUTE_SOUND_ERROR_STB_VORBIS_DECODE_FAILED";
	case CUTE_SOUND_ERROR_OGG_UNSUPPORTED_CHANNEL_COUNT: return "CUTE_SOUND_ERROR_OGG_UNSUPPORTED_CHANNEL_COUNT";
	case CUTE_SOUND_ERROR_IMPLEMENTATION_ERROR_PLEASE_REPORT_THIS_ON_GITHUB: return "CUTE_SOUND_ERROR_IMPLEMENTATION_ERROR_PLEASE_REPORT_THIS_ON_GITHUB";
	default: return "UNKNOWN";
	}
}

// Cute sound context functions.

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
	double sample_index;
	cs_audio_source_t* audio;
	struct cs_sound_inst_t* next;
	struct cs_sound_inst_t* prev;
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
	void (*on_finish)(cs_playing_sound_t, void*); /* = NULL */;
	void* on_finish_udata /* = NULL */;
	void (*on_music_finish)(void*); /* = NULL */;
	void* on_music_finish_udata /* = NULL */;

	bool music_paused /* = false */;
	bool music_looped /* = true */;
	float t /* = 0 */;
	float fade /* = 0 */;
	float fade_switch_1 /* = 0 */;
	float fade_start_volume /* = 0 */;
	cs_music_state_t music_state /* = MUSIC_STATE_NONE */;
	cs_music_state_t music_state_to_resume_from_paused /* = MUSIC_STATE_NONE */;
	cs_sound_inst_t* music_playing /* = NULL */;
	cs_sound_inst_t* music_next /* = NULL */;

	int audio_sources_to_free_capacity /* = 0 */;
	int audio_sources_to_free_size /* = 0 */;
	cs_audio_source_t** audio_sources_to_free /* = NULL */;
	uint64_t instance_id_gen /* = 1 */;
	cs_map_t instance_map;
	cs_inst_page_t* pages /* = NULL */;
	cs_sound_inst_t* playing_sounds /* = NULL */;
	cs_sound_inst_t* free_sounds /* = NULL */;

	int wide_count;
	cs__m128* floatA;
	cs__m128* floatB;
	cs__m128i* samples;
	bool running;
	SDL_AudioStream* stream;
	SDL_Mutex* mutex;
} cs_context_t;

void* s_mem_ctx;
cs_context_t* s_ctx = NULL;

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

static void cs_mix(int bytes_to_write);

static void cs_sdl_audio_callback(void* udata, SDL_AudioStream* stream, int additional_amount, int total_amount)
{
	if (additional_amount > 0) {
		cs_mix(additional_amount);
		SDL_PutAudioStreamData(stream, s_ctx->samples, additional_amount);
	}
}

static void s_add_page()
{
	cs_inst_page_t* page = (cs_inst_page_t*)CUTE_SOUND_ALLOC(sizeof(cs_inst_page_t), user_allocator_context);
	for (int i = 0; i < CUTE_SOUND_PAGE_INSTANCE_COUNT; ++i) {
		cs_sound_inst_t* inst = &page->instances[i];
		inst->next = s_ctx->free_sounds;
		inst->prev = NULL;
		if (s_ctx->free_sounds) s_ctx->free_sounds->prev = inst;
		s_ctx->free_sounds = inst;
	}
	page->next = s_ctx->pages;
	s_ctx->pages = page;
}

cs_error_t cs_init(unsigned play_frequency_in_Hz, void* user_allocator_context /* = NULL */)
{
	int wide_count = (int)CUTE_SOUND_ALIGN(CUTE_SOUND_MIXER_BUFFER_SIZE, 4);

	SDL_AudioSpec wanted = { SDL_AUDIO_S16, 2, (int)play_frequency_in_Hz };
	if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return CUTE_SOUND_ERROR_CANT_INIT_SDL_AUDIO;

	s_mem_ctx = user_allocator_context;
	s_ctx = (cs_context_t*)CUTE_SOUND_ALLOC(sizeof(cs_context_t), user_allocator_context);
	CUTE_SOUND_MEMSET(s_ctx, 0, sizeof(cs_context_t));
	s_ctx->global_pan = 0.5f;
	s_ctx->global_volume = 1.0f;
	s_ctx->music_volume = 1.0f;
	s_ctx->music_pitch = 1.0f;
	s_ctx->sound_volume = 1.0f;
	s_ctx->music_looped = true;
	s_ctx->audio_sources_to_free_capacity = 32;
	s_ctx->audio_sources_to_free = (cs_audio_source_t**)CUTE_SOUND_ALLOC(sizeof(cs_audio_source_t*) * s_ctx->audio_sources_to_free_capacity, s_mem_ctx);
	s_ctx->instance_id_gen = 1;
	cs_map_init(&s_ctx->instance_map, 1024, user_allocator_context);
	s_ctx->playing_sounds = NULL;
	s_ctx->free_sounds = NULL;
	s_add_page();
	s_ctx->wide_count = wide_count;
	s_ctx->floatA = (cs__m128*)cs_malloc16(sizeof(cs__m128) * wide_count);
	s_ctx->floatB = (cs__m128*)cs_malloc16(sizeof(cs__m128) * wide_count);
	s_ctx->samples = (cs__m128i*)cs_malloc16(sizeof(cs__m128i) * wide_count);
	s_ctx->running = true;
	s_ctx->mutex = SDL_CreateMutex();

	s_ctx->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &wanted, cs_sdl_audio_callback, NULL);
	if (!s_ctx->stream) return CUTE_SOUND_ERROR_CANT_OPEN_AUDIO_DEVICE;
	SDL_ResumeAudioStreamDevice(s_ctx->stream);

	return CUTE_SOUND_ERROR_NONE;
}

void cs_shutdown()
{
	if (!s_ctx) return;

	s_ctx->running = false;
	SDL_DestroyAudioStream(s_ctx->stream);
	SDL_DestroyMutex(s_ctx->mutex);

	for (cs_sound_inst_t* inst = s_ctx->playing_sounds; inst; inst = inst->next) {
		if (inst->audio) inst->audio->playing_count = 0;
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
	cs_map_term(&s_ctx->instance_map);
	CUTE_SOUND_FREE(s_ctx, s_mem_ctx);
	s_ctx = NULL;
}

static float s_smoothstep(float x) { return x * x * (3.0f - 2.0f * x); }
static void cs_free_queued_audio_sources();

void cs_update(float dt)
{
	switch (s_ctx->music_state) {
	case CUTE_SOUND_MUSIC_STATE_FADE_OUT:
	{
		s_ctx->t += dt;
		if (s_ctx->t >= s_ctx->fade) {
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_NONE;
			s_ctx->music_playing->active = false;
			s_ctx->music_playing = NULL;
		} else {
			float progress = s_smoothstep(s_ctx->t / s_ctx->fade);
			s_ctx->music_playing->volume = s_ctx->fade_start_volume * (1.0f - progress);
		}
	}	break;

	case CUTE_SOUND_MUSIC_STATE_FADE_IN:
	{
		s_ctx->t += dt;
		if (s_ctx->t >= s_ctx->fade) {
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_PLAYING;
			s_ctx->music_playing->volume = s_ctx->music_volume;
		} else {
			float progress = s_smoothstep(s_ctx->t / s_ctx->fade);
			s_ctx->music_playing->volume = s_ctx->music_volume * progress;
		}
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
			float progress = s_smoothstep(s_ctx->t / s_ctx->fade);
			s_ctx->music_playing->volume = s_ctx->fade_start_volume * (1.0f - progress);
		}
	}	break;

	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1:
	{
		s_ctx->t += dt;
		if (s_ctx->t >= s_ctx->fade) {
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_PLAYING;
			s_ctx->music_next->volume = s_ctx->music_volume;
			s_ctx->music_playing = s_ctx->music_next;
			s_ctx->music_next = NULL;
		} else {
			float progress = s_smoothstep(s_ctx->t / s_ctx->fade);
			s_ctx->music_next->volume = s_ctx->music_volume * progress;
		}
	}	break;

	case CUTE_SOUND_MUSIC_STATE_CROSSFADE:
	{
		s_ctx->t += dt;
		if (s_ctx->t >= s_ctx->fade) {
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_PLAYING;
			s_ctx->music_playing->active = false;
			s_ctx->music_next->volume = s_ctx->music_volume;
			s_ctx->music_playing = s_ctx->music_next;
			s_ctx->music_next = NULL;
		} else {
			float progress = s_smoothstep(s_ctx->t / s_ctx->fade);
			s_ctx->music_playing->volume = s_ctx->fade_start_volume * (1.0f - progress);
			s_ctx->music_next->volume = s_ctx->music_volume * progress;
		}
	}	break;

	default:
		break;
	}

	SDL_LockMutex(s_ctx->mutex);
	cs_free_queued_audio_sources();
	SDL_UnlockMutex(s_ctx->mutex);
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

// Calculate volume for left/right channels with panning and global settings.
static void cs_calc_volume(cs_sound_inst_t* playing, float* out_vA, float* out_vB)
{
	float gpan0 = 1.0f - s_ctx->global_pan;
	float gpan1 = s_ctx->global_pan;
	float vA = playing->volume * playing->pan0 * gpan0 * s_ctx->global_volume;
	float vB = playing->volume * playing->pan1 * gpan1 * s_ctx->global_volume;
	float type_vol = playing->is_music ? s_ctx->music_volume : s_ctx->sound_volume;
	*out_vA = vA * type_vol;
	*out_vB = vB * type_vol;
}

// Stop a sound instance and move it to the free list.
static void cs_stop_sound_internal(cs_sound_inst_t* inst)
{
	inst->sample_index = 0;
	inst->active = false;

	if (inst->audio) {
		inst->audio->playing_count -= s_ctx->running ? 1 : inst->audio->playing_count;
		CUTE_SOUND_ASSERT(inst->audio->playing_count >= 0);
	}

	// Remove from playing list.
	if (inst->prev) inst->prev->next = inst->next;
	else s_ctx->playing_sounds = inst->next;
	if (inst->next) inst->next->prev = inst->prev;

	// Add to free list.
	inst->next = s_ctx->free_sounds;
	inst->prev = NULL;
	if (s_ctx->free_sounds) s_ctx->free_sounds->prev = inst;
	s_ctx->free_sounds = inst;

	cs_map_remove(&s_ctx->instance_map, inst->id);

	if (s_ctx->on_finish && !inst->is_music) {
		cs_playing_sound_t snd = { inst->id };
		s_ctx->on_finish(snd, s_ctx->on_finish_udata);
	} else if (s_ctx->on_music_finish && inst->is_music) {
		s_ctx->on_music_finish(s_ctx->on_music_finish_udata);
	}
}

// Sample with bounds check - returns 0 for out of bounds (used for non-looping sounds).
#define CS_CLAMP_SAMPLE(buf, idx, count) ((unsigned)(idx) >= (unsigned)(count) ? 0.0f : ((float*)(buf))[idx])

// Sample with wraparound - wraps index modularly (used for looping sounds).
// Handles negative indices correctly: (-1) wraps to (count-1).
static inline int cs_wrap_index(int idx, int count)
{
	int r = idx % count;
	return r < 0 ? r + count : r;
}
#define CS_WRAP_SAMPLE(buf, idx, count) (((float*)(buf))[cs_wrap_index(idx, count)])

// Floor function for SIMD - converts truncate-toward-zero to floor.
static inline cs__m128i cs_mm_floor_epi32(cs__m128 v)
{
	cs__m128i trunc = cs_mm_cvttps_epi32(v);
	cs__m128 trunc_f = cs_mm_cvtepi32_ps(trunc);
	// If v < trunc (i.e., v was negative with fractional part), subtract 1.
	cs__m128 mask = cs_mm_cmplt_ps(v, trunc_f);
	cs__m128i adjust = cs_mm_and_si128(cs_mm_castps_si128(mask), cs_mm_set1_epi32(1));
	return cs_mm_sub_epi32(trunc, adjust);
}

// Mix with pitch shifting (resampling with linear interpolation).
static void cs_mix_pitched(cs_audio_source_t* audio, cs__m128 vA, cs__m128 vB, int write_offset_wide, int write_wide, float pitch, double sample_index, bool looped)
{
	cs__m128* floatA = s_ctx->floatA;
	cs__m128* floatB = s_ctx->floatB;
	cs__m128* cA = (cs__m128*)audio->channels[0];
	cs__m128* cB = (cs__m128*)audio->channels[1];
	cs__m128 pitch_v = cs_mm_set1_ps(pitch);
	cs__m128 index_offset = cs_mm_set1_ps((float)sample_index);
	int sample_count = audio->sample_count;

	for (int i = 0, j = 0; i < write_wide; ++i, j += 4) {
		cs__m128 index = cs_mm_set_ps((float)j + 3, (float)j + 2, (float)j + 1, (float)j);
		index = cs_mm_add_ps(cs_mm_mul_ps(index, pitch_v), index_offset);
		cs__m128i index_int = cs_mm_floor_epi32(index);
		cs__m128 index_frac = cs_mm_sub_ps(index, cs_mm_cvtepi32_ps(index_int));
		int i0 = cs_mm_extract_epi32(index_int, 3);
		int i1 = cs_mm_extract_epi32(index_int, 2);
		int i2 = cs_mm_extract_epi32(index_int, 1);
		int i3 = cs_mm_extract_epi32(index_int, 0);

		cs__m128 loA, hiA;
		if (looped) {
			loA = cs_mm_set_ps(
				CS_WRAP_SAMPLE(cA, i0, sample_count),
				CS_WRAP_SAMPLE(cA, i1, sample_count),
				CS_WRAP_SAMPLE(cA, i2, sample_count),
				CS_WRAP_SAMPLE(cA, i3, sample_count)
			);
			hiA = cs_mm_set_ps(
				CS_WRAP_SAMPLE(cA, i0 + 1, sample_count),
				CS_WRAP_SAMPLE(cA, i1 + 1, sample_count),
				CS_WRAP_SAMPLE(cA, i2 + 1, sample_count),
				CS_WRAP_SAMPLE(cA, i3 + 1, sample_count)
			);
		} else {
			loA = cs_mm_set_ps(
				CS_CLAMP_SAMPLE(cA, i0, sample_count),
				CS_CLAMP_SAMPLE(cA, i1, sample_count),
				CS_CLAMP_SAMPLE(cA, i2, sample_count),
				CS_CLAMP_SAMPLE(cA, i3, sample_count)
			);
			hiA = cs_mm_set_ps(
				CS_CLAMP_SAMPLE(cA, i0 + 1, sample_count),
				CS_CLAMP_SAMPLE(cA, i1 + 1, sample_count),
				CS_CLAMP_SAMPLE(cA, i2 + 1, sample_count),
				CS_CLAMP_SAMPLE(cA, i3 + 1, sample_count)
			);
		}
		cs__m128 A = cs_mm_add_ps(loA, cs_mm_mul_ps(index_frac, cs_mm_sub_ps(hiA, loA)));

		if (audio->channel_count == 2) {
			cs__m128 loB, hiB;
			if (looped) {
				loB = cs_mm_set_ps(
					CS_WRAP_SAMPLE(cB, i0, sample_count),
					CS_WRAP_SAMPLE(cB, i1, sample_count),
					CS_WRAP_SAMPLE(cB, i2, sample_count),
					CS_WRAP_SAMPLE(cB, i3, sample_count)
				);
				hiB = cs_mm_set_ps(
					CS_WRAP_SAMPLE(cB, i0 + 1, sample_count),
					CS_WRAP_SAMPLE(cB, i1 + 1, sample_count),
					CS_WRAP_SAMPLE(cB, i2 + 1, sample_count),
					CS_WRAP_SAMPLE(cB, i3 + 1, sample_count)
				);
			} else {
				loB = cs_mm_set_ps(
					CS_CLAMP_SAMPLE(cB, i0, sample_count),
					CS_CLAMP_SAMPLE(cB, i1, sample_count),
					CS_CLAMP_SAMPLE(cB, i2, sample_count),
					CS_CLAMP_SAMPLE(cB, i3, sample_count)
				);
				hiB = cs_mm_set_ps(
					CS_CLAMP_SAMPLE(cB, i0 + 1, sample_count),
					CS_CLAMP_SAMPLE(cB, i1 + 1, sample_count),
					CS_CLAMP_SAMPLE(cB, i2 + 1, sample_count),
					CS_CLAMP_SAMPLE(cB, i3 + 1, sample_count)
				);
			}
			cs__m128 B = cs_mm_add_ps(loB, cs_mm_mul_ps(index_frac, cs_mm_sub_ps(hiB, loB)));
			A = cs_mm_mul_ps(A, vA);
			B = cs_mm_mul_ps(B, vB);
			floatA[i + write_offset_wide] = cs_mm_add_ps(floatA[i + write_offset_wide], A);
			floatB[i + write_offset_wide] = cs_mm_add_ps(floatB[i + write_offset_wide], B);
		} else {
			cs__m128 B = cs_mm_mul_ps(A, vB);
			A = cs_mm_mul_ps(A, vA);
			floatA[i + write_offset_wide] = cs_mm_add_ps(floatA[i + write_offset_wide], A);
			floatB[i + write_offset_wide] = cs_mm_add_ps(floatB[i + write_offset_wide], B);
		}
	}
}

#undef CS_CLAMP_SAMPLE
#undef CS_WRAP_SAMPLE

// Mix without pitch shifting (direct copy with volume).
static void cs_mix_simple(cs_audio_source_t* audio, cs__m128 vA, cs__m128 vB, int write_offset_wide, int write_wide, int sample_index_wide)
{
	cs__m128* floatA = s_ctx->floatA;
	cs__m128* floatB = s_ctx->floatB;
	cs__m128* cA = (cs__m128*)audio->channels[0];
	cs__m128* cB = (cs__m128*)audio->channels[1];

	if (audio->channel_count == 2) {
		for (int i = 0; i < write_wide; ++i) {
			cs__m128 A = cs_mm_mul_ps(cA[i + sample_index_wide], vA);
			cs__m128 B = cs_mm_mul_ps(cB[i + sample_index_wide], vB);
			floatA[i + write_offset_wide] = cs_mm_add_ps(floatA[i + write_offset_wide], A);
			floatB[i + write_offset_wide] = cs_mm_add_ps(floatB[i + write_offset_wide], B);
		}
	} else {
		for (int i = 0; i < write_wide; ++i) {
			cs__m128 A = cA[i + sample_index_wide];
			cs__m128 B = cs_mm_mul_ps(A, vB);
			A = cs_mm_mul_ps(A, vA);
			floatA[i + write_offset_wide] = cs_mm_add_ps(floatA[i + write_offset_wide], A);
			floatB[i + write_offset_wide] = cs_mm_add_ps(floatB[i + write_offset_wide], B);
		}
	}
}

// Free queued audio sources with zero refcount.
static void cs_free_queued_audio_sources()
{
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
}

static void cs_mix(int bytes_to_write)
{
	SDL_LockMutex(s_ctx->mutex);

	int samples_needed = bytes_to_write / CUTE_SOUND_BYTES_PER_SAMPLE_FRAME;
	if (!samples_needed) {
		SDL_UnlockMutex(s_ctx->mutex);
		return;
	}

	// Clear mixer buffers.
	cs__m128* floatA = s_ctx->floatA;
	cs__m128* floatB = s_ctx->floatB;
	CUTE_SOUND_MEMSET(floatA, 0, sizeof(cs__m128) * s_ctx->wide_count);
	CUTE_SOUND_MEMSET(floatB, 0, sizeof(cs__m128) * s_ctx->wide_count);

	// Mix all playing sounds into the mixer buffers.
	if (!s_ctx->global_pause) {
		for (cs_sound_inst_t* playing = s_ctx->playing_sounds; playing; ) {
			cs_sound_inst_t* next = playing->next;
			cs_audio_source_t* audio = playing->audio;

			// Check if sound should be removed.
			if (!audio || !playing->active || !s_ctx->running) {
				cs_stop_sound_internal(playing);
				playing = next;
				continue;
			}

			// Check if sound should be skipped this frame.
			if (playing->paused || playing->pitch == 0.0f) {
				playing = next;
				continue;
			}

			CUTE_SOUND_ASSERT(audio->channels[0]);

			// Calculate volume.
			float vA0, vB0;
			cs_calc_volume(playing, &vA0, &vB0);
			cs__m128 vA = cs_mm_set1_ps(vA0);
			cs__m128 vB = cs_mm_set1_ps(vB0);

			// Mix samples, handling looping.
			int samples_remaining = samples_needed;
			int write_offset = 0;
			bool pitched = playing->pitch != 1.0f;

			while (samples_remaining > 0) {
				// Check for end of sound before mixing.
				bool at_end = (playing->pitch >= 0)
					? (playing->sample_index >= (double)audio->sample_count)
					: (playing->sample_index <= 0.0);

				if (at_end) {
					if (playing->looped) {
						// Wrap sample_index back into valid range.
						if (playing->pitch >= 0) {
							while (playing->sample_index >= (double)audio->sample_count) {
								playing->sample_index -= (double)audio->sample_count;
							}
						} else {
							while (playing->sample_index < 0.0) {
								playing->sample_index += (double)audio->sample_count;
							}
						}
					} else {
						cs_stop_sound_internal(playing);
						break;
					}
				}

				// Calculate how many output samples we can write.
				int samples_to_write = samples_remaining;

				// For looped pitched sounds, CS_WRAP_SAMPLE handles wraparound so we can
				// mix the full buffer. For non-looped or non-pitched sounds, we must clamp.
				if (!playing->looped || !pitched) {
					if (playing->pitch >= 0) {
						double input_remaining = (double)audio->sample_count - playing->sample_index;
						int max_output = (int)(input_remaining / playing->pitch);
						if (samples_to_write > max_output) {
							samples_to_write = max_output;
						}
					} else {
						double input_remaining = playing->sample_index;
						int max_output = (int)(input_remaining / -playing->pitch);
						if (samples_to_write > max_output) {
							samples_to_write = max_output;
						}
					}
				}

				if (samples_to_write <= 0) break;

				int write_wide = CUTE_SOUND_ALIGN(samples_to_write, 4) / 4;
				int write_offset_wide = (int)CUTE_SOUND_ALIGN(write_offset, 4) / 4;
				int sample_index_wide = (int)CUTE_SOUND_TRUNC((int)playing->sample_index, 4) / 4;

				// Mix the samples.
				if (pitched) {
					cs_mix_pitched(audio, vA, vB, write_offset_wide, write_wide, playing->pitch, playing->sample_index, playing->looped);
				} else {
					cs_mix_simple(audio, vA, vB, write_offset_wide, write_wide, sample_index_wide);
				}

				// Advance by exact fractional amount.
				playing->sample_index += (double)samples_to_write * (double)playing->pitch;
				write_offset += samples_to_write;
				samples_remaining -= samples_to_write;
			}

			playing = next;
		}
	}

	// Convert floats to 16-bit packed interleaved samples.
	cs__m128i* samples = s_ctx->samples;
	for (int i = 0; i < s_ctx->wide_count; ++i) {
		cs__m128i a = cs_mm_cvtps_epi32(floatA[i]);
		cs__m128i b = cs_mm_cvtps_epi32(floatB[i]);
		cs__m128i a0b0a1b1 = cs_mm_unpacklo_epi32(a, b);
		cs__m128i a2b2a3b3 = cs_mm_unpackhi_epi32(a, b);
		samples[i] = cs_mm_packs_epi32(a0b0a1b1, a2b2a3b3);
	}

	SDL_UnlockMutex(s_ctx->mutex);
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

static int cs_four_cc(const char* CC, const void* memory)
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

static void cs_last_element(cs__m128* a, int i, float s0, float s1, float s2, float s3, int offset)
{
	switch (offset) {
	case 1: a[i] = cs_mm_set_ps(0.0f, 0.0f, 0.0f, s0); break;
	case 2: a[i] = cs_mm_set_ps(0.0f, 0.0f, s1, s0); break;
	case 3: a[i] = cs_mm_set_ps(0.0f, s2, s1, s0); break;
	case 0: a[i] = cs_mm_set_ps(s3, s2, s1, s0); break;
	}
}

// Sample conversion helpers.
static inline float cs_pcm8_to_float(uint8_t s) { return ((float)s - 128.0f) * (1.0f / 128.0f); }
static inline float cs_pcm16_to_float(int16_t s) { return (float)s; }
static inline float cs_pcm24_to_float(const uint8_t* p) {
	int32_t s = (int32_t)p[0] | ((int32_t)p[1] << 8) | ((int32_t)p[2] << 16);
	if (s & 0x800000) s |= 0xFF000000; // Sign extend.
	return (float)s * (1.0f / 256.0f); // Scale to 16-bit range.
}
static inline float cs_pcm32_to_float(int32_t s) { return (float)s * (1.0f / 65536.0f); } // Scale to 16-bit range.
static inline float cs_float32_to_float(float s) { return s * 32767.0f; } // Scale to 16-bit range.
static inline float cs_float64_to_float(double s) { return (float)s * 32767.0f; } // Scale to 16-bit range.

cs_audio_source_t* cs_load_wav(const char* path, cs_error_t* err /* = NULL */)
{
	int size;
	void* wav = cs_read_file_to_memory(path, &size);
	if (!wav) return NULL;
	cs_audio_source_t* audio = cs_read_mem_wav(wav, size, err);
	CUTE_SOUND_FREE(wav, s_mem_ctx);
	return audio;
}

// Helper to read a sample at a given index based on format.
typedef float (*cs_sample_fn)(const uint8_t* data, int idx, int stride);
static float cs_sample_pcm8(const uint8_t* data, int idx, int stride) { return cs_pcm8_to_float(data[idx * stride]); }
static float cs_sample_pcm16(const uint8_t* data, int idx, int stride) { return cs_pcm16_to_float(((int16_t*)data)[idx * stride]); }
static float cs_sample_pcm24(const uint8_t* data, int idx, int stride) { return cs_pcm24_to_float(data + idx * stride * 3); }
static float cs_sample_pcm32(const uint8_t* data, int idx, int stride) { return cs_pcm32_to_float(((int32_t*)data)[idx * stride]); }
static float cs_sample_float32(const uint8_t* data, int idx, int stride) { return cs_float32_to_float(((float*)data)[idx * stride]); }
static float cs_sample_float64(const uint8_t* data, int idx, int stride) { return cs_float64_to_float(((double*)data)[idx * stride]); }

cs_audio_source_t* cs_read_mem_wav(const void* memory, size_t size, cs_error_t* err)
{
	#define CS_WAV_FAIL(e) do { if (err) *err = e; return NULL; } while (0)
	if (err) *err = CUTE_SOUND_ERROR_NONE;
	if (!memory) CS_WAV_FAIL(CUTE_SOUND_ERROR_FILE_NOT_FOUND);
	if (size < 12) CS_WAV_FAIL(CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE);

	const uint8_t* data = (const uint8_t*)memory;
	const uint8_t* end = data + size;

	// Validate RIFF/WAVE header.
	if (!cs_four_cc("RIFF", data)) CS_WAV_FAIL(CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE);
	if (!cs_four_cc("WAVE", data + 8)) CS_WAV_FAIL(CUTE_SOUND_ERROR_THE_FILE_IS_NOT_A_WAV_FILE);
	data += 12;

	// Find fmt chunk.
	while (1) {
		if (end - data < 8) CS_WAV_FAIL(CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND);
		if (cs_four_cc("fmt ", data)) break;
		data = (const uint8_t*)cs_next((char*)data);
	}
	uint32_t fmt_size = *(uint32_t*)(data + 4);
	if (end - data < 8 + fmt_size || fmt_size < 16) CS_WAV_FAIL(CUTE_SOUND_ERROR_WAV_FILE_FORMAT_CHUNK_NOT_FOUND);

	// Parse fmt chunk.
	uint16_t format_tag = *(uint16_t*)(data + 8);
	uint16_t channels = *(uint16_t*)(data + 10);
	uint32_t sample_rate = *(uint32_t*)(data + 12);
	uint16_t bits_per_sample = *(uint16_t*)(data + 22);

	// Validate format.
	if (!(channels == 1 || channels == 2)) CS_WAV_FAIL(CUTE_SOUND_ERROR_WAV_ONLY_MONO_OR_STEREO_IS_SUPPORTED);

	cs_sample_fn sample_fn = NULL;
	int bytes_per_sample = 0;
	if (format_tag == 1) {
		// PCM
		switch (bits_per_sample) {
		case 8:  sample_fn = cs_sample_pcm8;  bytes_per_sample = 1; break;
		case 16: sample_fn = cs_sample_pcm16; bytes_per_sample = 2; break;
		case 24: sample_fn = cs_sample_pcm24; bytes_per_sample = 3; break;
		case 32: sample_fn = cs_sample_pcm32; bytes_per_sample = 4; break;
		default: CS_WAV_FAIL(CUTE_SOUND_ERROR_WAV_UNSUPPORTED_FORMAT);
		}
	} else if (format_tag == 3) {
		// IEEE float
		switch (bits_per_sample) {
		case 32: sample_fn = cs_sample_float32; bytes_per_sample = 4; break;
		case 64: sample_fn = cs_sample_float64; bytes_per_sample = 8; break;
		default: CS_WAV_FAIL(CUTE_SOUND_ERROR_WAV_UNSUPPORTED_FORMAT);
		}
	} else {
		CS_WAV_FAIL(CUTE_SOUND_ERROR_WAV_UNSUPPORTED_FORMAT);
	}

	// Find data chunk.
	while (1) {
		if (end - data < 8) CS_WAV_FAIL(CUTE_SOUND_ERROR_WAV_DATA_CHUNK_NOT_FOUND);
		if (cs_four_cc("data", data)) break;
		data = (const uint8_t*)cs_next((char*)data);
	}
	uint32_t data_size = *(uint32_t*)(data + 4);
	const uint8_t* samples = data + 8;
	if (end - samples < data_size) CS_WAV_FAIL(CUTE_SOUND_ERROR_WAV_DATA_CHUNK_NOT_FOUND);

	// Calculate sample count.
	int64_t frame_size = channels * bytes_per_sample;
	int64_t sample_count = data_size / frame_size;

	// Allocate audio source.
	cs_audio_source_t* audio = (cs_audio_source_t*)CUTE_SOUND_ALLOC(sizeof(cs_audio_source_t), s_mem_ctx);
	CUTE_SOUND_MEMSET(audio, 0, sizeof(*audio));
	audio->sample_rate = (int)sample_rate;
	audio->sample_count = (int)sample_count;
	audio->channel_count = channels;

	int wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4) / 4;
	int wide_offset = (int)(sample_count & 3);

	if (channels == 1) {
		audio->channels[0] = cs_malloc16(wide_count * sizeof(cs__m128));
		audio->channels[1] = NULL;
		cs__m128* a = (cs__m128*)audio->channels[0];
		for (int i = 0; i < wide_count - 1; ++i) {
			int j = i * 4;
			a[i] = cs_mm_set_ps(sample_fn(samples, j+3, 1), sample_fn(samples, j+2, 1), sample_fn(samples, j+1, 1), sample_fn(samples, j, 1));
		}
		int j = (wide_count - 1) * 4;
		float s0 = sample_fn(samples, j, 1);
		float s1 = wide_offset > 1 ? sample_fn(samples, j+1, 1) : 0;
		float s2 = wide_offset > 2 ? sample_fn(samples, j+2, 1) : 0;
		float s3 = wide_offset > 3 ? sample_fn(samples, j+3, 1) : 0;
		cs_last_element(a, wide_count - 1, s0, s1, s2, s3, wide_offset);
	} else {
		cs__m128* a = (cs__m128*)cs_malloc16(wide_count * sizeof(cs__m128) * 2);
		cs__m128* b = a + wide_count;
		for (int i = 0; i < wide_count - 1; ++i) {
			int j = i * 4;
			a[i] = cs_mm_set_ps(sample_fn(samples, (j+3)*2, 1), sample_fn(samples, (j+2)*2, 1), sample_fn(samples, (j+1)*2, 1), sample_fn(samples, j*2, 1));
			b[i] = cs_mm_set_ps(sample_fn(samples, (j+3)*2+1, 1), sample_fn(samples, (j+2)*2+1, 1), sample_fn(samples, (j+1)*2+1, 1), sample_fn(samples, j*2+1, 1));
		}
		int j = (wide_count - 1) * 4;
		float a0 = sample_fn(samples, j*2, 1);
		float a1 = wide_offset > 1 ? sample_fn(samples, (j+1)*2, 1) : 0;
		float a2 = wide_offset > 2 ? sample_fn(samples, (j+2)*2, 1) : 0;
		float a3 = wide_offset > 3 ? sample_fn(samples, (j+3)*2, 1) : 0;
		float b0 = sample_fn(samples, j*2+1, 1);
		float b1 = wide_offset > 1 ? sample_fn(samples, (j+1)*2+1, 1) : 0;
		float b2 = wide_offset > 2 ? sample_fn(samples, (j+2)*2+1, 1) : 0;
		float b3 = wide_offset > 3 ? sample_fn(samples, (j+3)*2+1, 1) : 0;
		cs_last_element(a, wide_count - 1, a0, a1, a2, a3, wide_offset);
		cs_last_element(b, wide_count - 1, b0, b1, b2, b3, wide_offset);
		audio->channels[0] = a;
		audio->channels[1] = b;
	}

	#undef CS_WAV_FAIL
	return audio;
}

void cs_free_audio_source(cs_audio_source_t* audio)
{
	if (s_ctx) {
		SDL_LockMutex(s_ctx->mutex);
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
		SDL_UnlockMutex(s_ctx->mutex);
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

#if defined(SDL_rwops_h_) && defined(CUTE_SOUND_SDL_RWOPS)

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
		if (!wav) return NULL;
		cs_audio_source_t* audio = cs_read_mem_wav(wav, size, err);
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
			int j = (wide_count - 1) * 4;
			float s0 = (float)samples[j];
			float s1 = wide_offset > 1 ? (float)samples[j+1] : 0;
			float s2 = wide_offset > 2 ? (float)samples[j+2] : 0;
			float s3 = wide_offset > 3 ? (float)samples[j+3] : 0;
			cs_last_element(a, wide_count - 1, s0, s1, s2, s3, wide_offset);
		}	break;

		case 2:
		{
			a = (cs__m128*)cs_malloc16(wide_count * sizeof(cs__m128) * 2);
			b = a + wide_count;
			for (int i = 0, j = 0; i < wide_count - 1; ++i, j += 8) {
				a[i] = cs_mm_set_ps((float)samples[j+6], (float)samples[j+4], (float)samples[j+2], (float)samples[j]);
				b[i] = cs_mm_set_ps((float)samples[j+7], (float)samples[j+5], (float)samples[j+3], (float)samples[j+1]);
			}
			int j = (wide_count - 1) * 8;
			float a0 = (float)samples[j];
			float a1 = wide_offset > 1 ? (float)samples[j+2] : 0;
			float a2 = wide_offset > 2 ? (float)samples[j+4] : 0;
			float a3 = wide_offset > 3 ? (float)samples[j+6] : 0;
			float b0 = (float)samples[j+1];
			float b1 = wide_offset > 1 ? (float)samples[j+3] : 0;
			float b2 = wide_offset > 2 ? (float)samples[j+5] : 0;
			float b3 = wide_offset > 3 ? (float)samples[j+7] : 0;
			cs_last_element(a, wide_count - 1, a0, a1, a2, a3, wide_offset);
			cs_last_element(b, wide_count - 1, b0, b1, b2, b3, wide_offset);
		}	break;

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

#if defined(SDL_rwops_h_) && defined(CUTE_SOUND_SDL_RWOPS)

	cs_audio_source_t* cs_load_ogg_rw(SDL_RWops* rw, cs_error_t* err)
	{
		int length;
		void* memory = cs_read_rw_to_memory(rw, &length);
		if (!memory) return NULL;
		cs_audio_source_t* audio = cs_read_mem_ogg(memory, length, err);
		CUTE_SOUND_FREE(memory, s_mem_ctx);
		return audio;
	}

#endif
#endif // STB_VORBIS_INCLUDE_STB_VORBIS_H

// -------------------------------------------------------------------------------------------------
// Music sounds.

static cs_sound_inst_t* s_alloc_inst()
{
	if (!s_ctx->free_sounds) {
		s_add_page();
	}
	CUTE_SOUND_ASSERT(s_ctx->free_sounds);
	cs_sound_inst_t* inst = s_ctx->free_sounds;
	s_ctx->free_sounds = inst->next;
	if (s_ctx->free_sounds) s_ctx->free_sounds->prev = NULL;
	return inst;
}

static void s_insert(cs_sound_inst_t* inst)
{
	SDL_LockMutex(s_ctx->mutex);
	// Add to playing list.
	inst->next = s_ctx->playing_sounds;
	inst->prev = NULL;
	if (s_ctx->playing_sounds) s_ctx->playing_sounds->prev = inst;
	s_ctx->playing_sounds = inst;
	inst->audio->playing_count += 1;
	inst->active = true;
	inst->id = s_ctx->instance_id_gen++;
	cs_map_insert(&s_ctx->instance_map, inst->id, inst);
	SDL_UnlockMutex(s_ctx->mutex);
}

static cs_sound_inst_t* s_inst_music(cs_audio_source_t* src, float volume)
{
	cs_sound_inst_t* inst = s_alloc_inst();
	inst->is_music = true;
	inst->looped = s_ctx->music_looped;
	if (!s_ctx->music_paused) inst->paused = false;
	inst->volume = volume;
	inst->pan0 = 0.5f;
	inst->pan1 = 0.5f;
	inst->pitch = 1.0f;
	inst->audio = src;
	inst->sample_index = 0;
	s_insert(inst);
	return inst;
}

static cs_sound_inst_t* s_inst(cs_audio_source_t* src, cs_sound_params_t params)
{
	cs_sound_inst_t* inst = s_alloc_inst();
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
	inst->sample_index = params.start_time * (double)src->sample_rate;
	CUTE_SOUND_ASSERT(inst->sample_index < (double)src->sample_count);
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
			s_ctx->fade_start_volume = s_ctx->music_playing->volume;
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_FADE_OUT;
			s_ctx->fade = fade_out_time;
			s_ctx->t = 0;
			break;

		case CUTE_SOUND_MUSIC_STATE_FADE_OUT:
			break;

		case CUTE_SOUND_MUSIC_STATE_FADE_IN:
			s_ctx->fade_start_volume = s_ctx->music_playing->volume;
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_FADE_OUT;
			s_ctx->fade = fade_out_time;
			s_ctx->t = 0;
			break;

		case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0:
			s_ctx->fade_start_volume = s_ctx->music_playing->volume;
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_FADE_OUT;
			s_ctx->fade = fade_out_time;
			s_ctx->t = 0;
			s_ctx->music_next->active = false;
			s_ctx->music_next = NULL;
			break;

		case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1:
			// Fall-through.
		case CUTE_SOUND_MUSIC_STATE_CROSSFADE:
			// Fade out the incoming track (which is louder).
			s_ctx->fade_start_volume = s_ctx->music_next->volume;
			s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_FADE_OUT;
			s_ctx->fade = fade_out_time;
			s_ctx->t = 0;
			s_ctx->music_playing->active = false;
			s_ctx->music_playing = s_ctx->music_next;
			s_ctx->music_next = NULL;
			break;

		case CUTE_SOUND_MUSIC_STATE_PAUSED:
			cs_music_stop(0);
			break;
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
		s_ctx->fade_start_volume = s_ctx->music_playing->volume;
		s_ctx->fade = fade_out_time;
		s_ctx->fade_switch_1 = fade_in_time;
		s_ctx->t = 0;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_FADE_OUT:
	{
		// Continue existing fade out, just add the incoming track.
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
		s_ctx->fade_start_volume = s_ctx->music_playing->volume;
		s_ctx->fade = fade_out_time;
		s_ctx->fade_switch_1 = fade_in_time;
		s_ctx->t = 0;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0:
	{
		// Replace pending incoming track with new one.
		CUTE_SOUND_ASSERT(s_ctx->music_next != NULL);
		cs_sound_inst_t* inst = s_inst_music(audio_source, fade_in_time == 0 ? 1.0f : 0);
		s_ctx->music_next->active = false;
		s_ctx->music_next = inst;
		s_ctx->fade_switch_1 = fade_in_time;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_CROSSFADE:
	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1:
	{
		// Incoming track becomes the one to fade out.
		CUTE_SOUND_ASSERT(s_ctx->music_next != NULL);
		cs_sound_inst_t* inst = s_inst_music(audio_source, fade_in_time == 0 ? 1.0f : 0);
		s_ctx->music_playing->active = false;
		s_ctx->music_playing = s_ctx->music_next;
		s_ctx->music_next = inst;
		s_ctx->fade_start_volume = s_ctx->music_playing->volume;
		s_ctx->fade = fade_out_time;
		s_ctx->fade_switch_1 = fade_in_time;
		s_ctx->t = 0;
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
		break;

	case CUTE_SOUND_MUSIC_STATE_PLAYING:
	{
		CUTE_SOUND_ASSERT(s_ctx->music_next == NULL);
		cs_sound_inst_t* inst = s_inst_music(audio_source, cross_fade_time == 0 ? 1.0f : 0);
		inst->paused = false;
		s_ctx->music_next = inst;
		s_ctx->fade_start_volume = s_ctx->music_playing->volume;
		s_ctx->fade = cross_fade_time;
		s_ctx->t = 0;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_CROSSFADE;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_FADE_OUT:
		CUTE_SOUND_ASSERT(s_ctx->music_next == NULL);
		// Fall-through.

	case CUTE_SOUND_MUSIC_STATE_FADE_IN:
	{
		cs_sound_inst_t* inst = s_inst_music(audio_source, cross_fade_time == 0 ? 1.0f : 0);
		inst->paused = false;
		s_ctx->music_next = inst;
		s_ctx->fade_start_volume = s_ctx->music_playing->volume;
		s_ctx->fade = cross_fade_time;
		s_ctx->t = 0;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_CROSSFADE;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_0:
	{
		s_ctx->music_next->active = false;
		cs_sound_inst_t* inst = s_inst_music(audio_source, cross_fade_time == 0 ? 1.0f : 0);
		inst->paused = false;
		s_ctx->music_next = inst;
		s_ctx->fade_start_volume = s_ctx->music_playing->volume;
		s_ctx->fade = cross_fade_time;
		s_ctx->t = 0;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_CROSSFADE;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_SWITCH_TO_1:
		// Fall-through.

	case CUTE_SOUND_MUSIC_STATE_CROSSFADE:
	{
		s_ctx->music_playing->active = false;
		s_ctx->music_playing = s_ctx->music_next;
		cs_sound_inst_t* inst = s_inst_music(audio_source, cross_fade_time == 0 ? 1.0f : 0);
		inst->paused = false;
		s_ctx->music_next = inst;
		s_ctx->fade_start_volume = s_ctx->music_playing->volume;
		s_ctx->fade = cross_fade_time;
		s_ctx->t = 0;
		s_ctx->music_state = CUTE_SOUND_MUSIC_STATE_CROSSFADE;
	}	break;

	case CUTE_SOUND_MUSIC_STATE_PAUSED:
		cs_music_stop(0);
		cs_music_crossfade(audio_source, cross_fade_time);
		break;
	}
}

double cs_music_get_time()
{
	if (!s_ctx->music_playing) return 0.0;
	return s_ctx->music_playing->sample_index / (double)s_ctx->music_playing->audio->sample_rate;
}

cs_error_t cs_music_set_time(double time_in_seconds)
{
	if (!s_ctx->music_playing) return CUTE_SOUND_ERROR_INVALID_SOUND;
	double sample_index = time_in_seconds * (double)s_ctx->music_playing->audio->sample_rate;
	if (sample_index > (double)s_ctx->music_playing->audio->sample_count) return CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT;
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
	params.start_time = 0.0;
	return params;
}

static cs_sound_inst_t* s_get_inst(cs_playing_sound_t sound)
{
	return (cs_sound_inst_t*)cs_map_find(&s_ctx->instance_map, sound.id);
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

double cs_sound_get_time(cs_playing_sound_t sound)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return 0.0;
	return inst->sample_index / (double)inst->audio->sample_rate;
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

cs_error_t cs_sound_set_time(cs_playing_sound_t sound, double time_in_seconds)
{
	cs_sound_inst_t* inst = s_get_inst(sound);
	if (!inst) return CUTE_SOUND_ERROR_INVALID_SOUND;
	double sample_index = time_in_seconds * (double)inst->audio->sample_rate;
	if (sample_index > (double)inst->audio->sample_count) return CUTE_SOUND_ERROR_TRIED_TO_SET_SAMPLE_INDEX_BEYOND_THE_AUDIO_SOURCES_SAMPLE_COUNT;
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
	SDL_LockMutex(s_ctx->mutex);
	// Set all playing sounds (that aren't music) active to false.
	for (cs_sound_inst_t* inst = s_ctx->playing_sounds; inst; inst = inst->next) {
		if (inst != s_ctx->music_playing && inst != s_ctx->music_next) {
			inst->active = false; // Let cs_mix handle cleaning this up.
		}
	}
	SDL_UnlockMutex(s_ctx->mutex);
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
	Copyright (c) 2026 Randy Gaul https://randygaul.github.io/
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
