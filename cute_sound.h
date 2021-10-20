/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_sound.h - v1.12


	To create implementation (the function definitions)
		#define CUTE_SOUND_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file


	SUMMARY

		cute_sound is a C API for loading, playing, looping, panning and fading mono
		and stereo sounds, without any external dependencies other than things that ship
		with standard OSs, or SDL2 for more uncommon OSs.

		For Windows cute_sound uses DirectSound. Due to the use of SSE intrinsics, MinGW
		builds must be made with the compiler option: -march=native, or optionally SSE
		can be disabled with CUTE_SOUND_SCALAR_MODE. More on this mode written below.

		For Apple machines cute_sound uses CoreAudio.

		For Linux builds cute_sound uses ALSA (implementation is currently buggy, and
		cute_sound's SDL2 implementation is recommended instead).

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

		1.0  (06/04/2016) initial release
		1.01 (06/06/2016) load WAV from memory
		                  separate portable and OS-specific code in cs_mix
		                  fixed bug causing audio glitches when sounds ended
		                  added stb_vorbis loaders + demo example
		1.02 (06/08/2016) error checking + strings in vorbis loaders
		                  SSE2 implementation of mixer
		                  fix typos on docs/comments
		                  corrected volume bug introduced in 1.01
		1.03 (07/05/2016) size calculation helper (to know size of sound in
		                  bytes on the heap) cs_sound_size
		1.04 (12/06/2016) merged in Aaron Balint's contributions
		                  SFFT and pitch functions from Stephan M. Bernsee
		                  cs_mix can run on its own thread with cs_spawn_mix_thread
		                  updated documentation, typo fixes
		                  fixed typo in cs_malloc16 that caused heap corruption
		1.05 (12/08/2016) cs_stop_all_sounds, suggested by Aaron Balint
		1.06 (02/17/2017) port to CoreAudio for Apple machines
		1.07 (06/18/2017) SIMD the pitch shift code; swapped out old Bernsee
		                  code for a new re-write, updated docs as necessary,
		                  support for compiling as .c and .cpp on Windows,
		                  port for SDL (for Linux, or any other platform).
		                  Special thanks to DeXP (Dmitry Hrabrov) for 90% of
		                  the work on the SDL port!
		1.08 (09/06/2017) SDL_RWops support by RobLoach
		1.09 (05/20/2018) Load wav funcs can skip all irrelevant chunks
		                  Ref counting for playing sounds
		1.10 (08/24/2019) Introduced plugin interface, reimplemented pitch shifting
		                  as a plugin, added optional `ctx` to alloc functions
		1.11 (04/23/2020) scalar SIMD mode and various compiler warning/error fixes
		1.12 (10/20/2021) removed old and broken assert if driver requested non-
		                  power of two sample size for mixing updates


	CONTRIBUTORS

		Aaron Balint      1.04 - real time pitch
		                  1.04 - separate thread for cs_mix
		                  1.04 - bugfix, removed extra cs_free16 call for second channel
		DeXP              1.07 - initial work on SDL port
		RobLoach          1.08 - SDL_RWops support
		Matt Rosen        1.10 - Initial experiments with cute_dsp to figure out plugin
		                         interface needs and use-cases
		fluffrabbit       1.11 - scalar SIMD mode and various compiler warning/error fixes


	DOCUMENTATION (very quick intro)

		1. create context
		2. load sounds from disk into memory
		3. play sounds
		4. free context

		1. cs_context_t* ctx = cs_make_context(hwnd, frequency, buffered_samples, N, NULL);
		2. cs_play_sound_def_t def = cs_make_def(&cs_load_wav("path_to_file/filename.wav"));
		3. cs_play_sound(ctx, def);
		4. cs_shutdown_context(ctx);


	DOCUMENTATION (longer introduction)

		cute_sound consists of cs_loaded_sound_t, cs_playing_sound_t and the cs_context_t.
		The cs_context_t encapsulates an OS sound API, as well as buffers + settings.
		cs_loaded_sound_t holds raw samples of a sound. cs_playing_sound_t is an instance
		of a cs_loaded_sound_t that represents a sound that can be played through the
		cs_context_t.

		There are two main versions of the API, the low-level and the high-level
		API. The low-level API does not manage any memory for cs_playing_sound_ts. The
		high level api holds a memory pool of playing sounds.

		To actually mix sounds together and send audio to the sound card, be sure
		to call either cs_mix periodically or call cs_spawn_mix_thread once.


	High-level API

		First create a context and pass in non-zero to the final parameter. This
		final parameter controls how large of a memory pool to use for cs_playing_sound_t's.
		Here's an example where N is the size of the internal pool:

		cs_context_t* ctx = cs_make_context(hwnd, frequency, buffered_samples, N, NULL);

		We create cs_playing_sound_t's indirectly with cs_play_def_t structs. cs_play_def_t is a
		POD struct so feel free to make them straight on the stack. The cs_play_def
		sets up initialization parameters. Here's an example to load a wav and
		play it:

		cs_loaded_sound_t loaded = cs_load_wav("path_to_file/filename.wav");
		cs_play_sound_def_t def = cs_make_def(&loaded);
		cs_playing_sound_t* sound = cs_play_sound(ctx, def);

		The same def can be used to play as many sounds as desired (even simultaneously)
		as long as the context playing sound pool is large enough.

		You can work with some low-level functionality by iterating through the linked list
		ctx->playing, which can be accessed by calling cs_get_playing(ctx). If you spawned a
		thread via cs_spawn_mix_thread, remember to call cs_lock before you work with
		cs_get_playing and call cs_unlock afterwards so sound can play again.


	Low-level API

		First create a context and pass 0 in the final parameter (0 here means
		the context will *not* allocate a cs_playing_sound_t memory pool):

		cs_context_t* ctx = cs_make_context(hwnd, frequency, buffered_samples, 0, NULL);

		parameters:
			hwnd             --  HWND, handle to window (on OSX just pass in 0)
			frequency        --  int, represents Hz frequency rate in which samples are played
			buffered_samples --  int, number of samples the internal ring buffers can hold at once
			0                --  int, number of elements in cs_playing_sound_t pool
			NULL             --  optional pointer for custom allocator, just set to NULL

		We create a cs_playing_sound_t like so:
		cs_loaded_sound_t loaded = cs_load_wav("path_to_file/filename.wav");
		cs_playing_sound_t playing_sound = cs_make_playing_sound(&loaded);

		Then to play the sound we do:
		cs_insert_sound(ctx, &playing_sound);

		The above cs_insert_sound function call will place playing_sound into
		a singly-linked list inside the context. The context will remove
		the sound from its internal list when it finishes playing.

	WARNING: The high-level API cannot be mixed with the low-level API. If you
	try then the internal code will assert and crash. Pick one and stick with it.
	Usually he high-level API will be used, but if someone is *really* picky about
	their memory usage, or wants more control, the low-level API can be used.

	Here is the Low-Level API:
		cs_playing_sound_t cs_make_playing_sound(cs_loaded_sound_t* loaded);
		int cs_insert_sound(cs_context_t* ctx, cs_playing_sound_t* sound);

	Here is the High-Level API:
		cs_playing_sound_t* cs_play_sound(cs_context_t* ctx, cs_play_sound_def_t def);
		cs_play_sound_def_t cs_make_def(cs_loaded_sound_t* sound);
		void cs_stop_all_sounds(cs_context_t(ctx);

	Be sure to link against dsound.dll (or dsound.lib) on Windows.

	Read the rest of the header for specific details on all available functions
	and struct types.


	PLUGINS

		Cute sound can add plugins at run-time to modify audio before it gets mixed. Please
		refer to all the documentation near `cs_plugin_interface_t`.

	DISABLE SSE SIMD ACCELERATION

		If for whatever reason you don't want to use SSE intrinsics and instead would prefer
		plain C (for example if your platform does not support SSE) then define
		CUTE_SOUND_SCALAR_MODE before including cute_sound.h while also defining the
		symbol definitions. Here's an example:

			#define CUTE_SOUND_IMPLEMENTATION
			#define CUTE_SOUND_SCALAR_MODE
			#include <cute_sound.h>

	KNOWN LIMITATIONS

		* PCM mono/stereo format is the only formats the LoadWAV function supports. I don't
			guarantee it will work for all kinds of wav files, but it certainly does for the common
			kind (and can be changed fairly easily if someone wanted to extend it).
		* Only supports 16 bits per sample.
		* Mixer does not do any fancy clipping. The algorithm is to convert all 16 bit samples
			to float, mix all samples, and write back to audio API as 16 bit integers. In
			practice this works very well and clipping is not often a big problem.


	FAQ

		Q : Why DirectSound instead of (insert API here) on Windows?
		A : Casey Muratori documented DS on Handmade Hero, other APIs do not have such good docs. DS has
		shipped on Windows XP all the way through Windows 10 -- using this header effectively intro-
		duces zero dependencies for the foreseeable future. The DS API itself is sane enough to quickly
		implement needed features, and users won't hear the difference between various APIs. Latency is
		not that great with DS but it is shippable. Additionally, many other APIs will in the end speak
		to Windows through the DS API.

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

/*
	Some past discussion threads:
	https://www.reddit.com/r/gamedev/comments/6i39j2/tinysound_the_cutest_library_to_get_audio_into/
	https://www.reddit.com/r/gamedev/comments/4ml6l9/tinysound_singlefile_c_audio_library/
	https://forums.tigsource.com/index.php?topic=58706.0
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

// read this in the event of cs_load_wav/cs_load_ogg errors
// also read this in the event of certain errors from cs_make_context
extern const char* cs_error_reason;

// stores a loaded sound in memory
typedef struct cs_loaded_sound_t
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
} cs_loaded_sound_t;

// Don't change this unless necessary. 32 is a huge number to begin with.
#define CUTE_SOUND_PLUGINS_MAX (32)

// represents an instance of a cs_loaded_sound_t, can be played through the cs_context_t
typedef struct cs_playing_sound_t
{
	int active;
	int paused;
	int looped;
	float volume0;
	float volume1;
	float pan0;
	float pan1;
	int sample_index;
	cs_loaded_sound_t* loaded_sound;
	struct cs_playing_sound_t* next;
	void* plugin_udata[CUTE_SOUND_PLUGINS_MAX];
} cs_playing_sound_t;

// holds audio API info and other info
struct cs_context_t;
typedef struct cs_context_t cs_context_t;

// The returned struct will contain a null pointer in cs_loaded_sound_t::channel[0]
// in the case of errors. Read cs_error_reason string for details on what happened.
// Calls cs_read_mem_wav internally.
cs_loaded_sound_t cs_load_wav(const char* path);

// Reads a WAV file from memory. Still allocates memory for the cs_loaded_sound_t since
// WAV format will interlace stereo, and we need separate data streams to do SIMD
// properly.
void cs_read_mem_wav(const void* memory, int size, cs_loaded_sound_t* sound);

// If stb_vorbis was included *before* cute_sound go ahead and create
// some functions for dealing with OGG files.
#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H

	void cs_read_mem_ogg(const void* memory, int length, cs_loaded_sound_t* sound);
	cs_loaded_sound_t cs_load_ogg(const char* path);

#endif

// Uses cs_free16 (aligned free, implemented later in this file) to free up both of
// the channels stored within sound
void cs_free_sound(cs_loaded_sound_t* sound);

// Returns the size, in bytes, of all heap-allocated memory for this particular
// loaded sound
int cs_sound_size(cs_loaded_sound_t* sound);

// playing_pool_count -- 0 to setup low-level API, non-zero to size the internal
// memory pool for cs_playing_sound_t instances
// `user_allocator_context` can be NULL - it is passed to `CUTE_SOUND_ALLOC` and
// `CUTE_SOUND_FREE`.
cs_context_t* cs_make_context(void* hwnd, unsigned play_frequency_in_Hz, int buffered_samples, int playing_pool_count, void* user_allocator_context);
void cs_shutdown_context(cs_context_t* ctx);

// Call cs_spawn_mix_thread once to setup a separate thread for the context to run
// upon. The separate thread will continually call cs_mix and perform mixing
// operations.
void cs_spawn_mix_thread(cs_context_t* ctx);

// Use cs_thread_sleep_delay to specify a custom sleep delay time.
// A sleep will occur after each call to cs_mix. By default YieldProcessor
// is used, and no sleep occurs. Use a sleep delay to conserve CPU bandwidth.
// A recommended sleep time is a little less than 1/2 your predicted 1/FPS.
// 60 fps is 16 ms, so about 1-5 should work well in most cases.
void cs_thread_sleep_delay(cs_context_t* ctx, int milliseconds);

// Lock the thread before working with some of the lower-level stuff.
void cs_lock(cs_context_t* ctx);

// Unlock the thread after you've done that stuff.
void cs_unlock(cs_context_t* ctx);

// Call this manually, once per game tick recommended, if you haven't ever
// called cs_spawn_mix_thread. Otherwise the thread will call cs_mix itself.
// num_samples_to_write is not used on Windows. On Mac it is used to push
// samples into a circular buffer while CoreAudio simultaneously pulls samples
// off of the buffer. num_samples_to_write should be computed each update tick
// as delta_time * play_frequency_in_Hz + 1.
void cs_mix(cs_context_t* ctx);

// All of the functions in this next section should only be called if cs_is_active
// returns true. Calling them otherwise probably won't do anything bad, but it
// won't do anything at all. If a sound is active it resides in the context's
// internal list of playing sounds.
int cs_is_active(cs_playing_sound_t* sound);

// Flags sound for removal. Upon next cs_mix call will remove sound from playing
// list. If high-level API used sound is placed onto the internal free list.
void cs_stop_sound(cs_playing_sound_t* sound);

void cs_loop_sound(cs_playing_sound_t* sound, int zero_for_no_loop);
void cs_pause_sound(cs_playing_sound_t* sound, int one_for_paused);

// lerp from 0 to 1, 0 full left, 1 full right
void cs_set_pan(cs_playing_sound_t* sound, float pan);

// explicitly set volume of each channel. Can be used as panning (but it's
// recommended to use the cs_set_pan function for panning).
void cs_set_volume(cs_playing_sound_t* sound, float volume_left, float volume_right);

// Delays sound before actually playing it. Requires context to be passed in
// since there's a conversion from seconds to samples per second.
// If one were so inclined another version could be implemented like:
// void cs_set_delay(cs_playing_sound_t* sound, float delay, int samples_per_second)
void cs_set_delay(cs_context_t* ctx, cs_playing_sound_t* sound, float delay_in_seconds);

// Return the linked list ctx->playing, be sure to use cs_lock or cs_unlock if mixing on
// another thread.
cs_playing_sound_t* cs_get_playing(cs_context_t* ctx);

// Portable sleep function. Do not call this with milliseconds > 999.
void cs_sleep(int milliseconds);

// LOW-LEVEL API
cs_playing_sound_t cs_make_playing_sound(cs_loaded_sound_t* loaded);
int cs_insert_sound(cs_context_t* ctx, cs_playing_sound_t* sound); // returns 1 if sound was successfully inserted, 0 otherwise

// HIGH-LEVEL API

// This def struct is just used to pass parameters to `cs_play_sound`.
// Be careful since `loaded` points to a `cs_loaded_sound_t` struct, so make
// sure the `cs_loaded_sound_t` struct persists in memory!
typedef struct cs_play_sound_def_t
{
	int paused;
	int looped;
	float volume_left;
	float volume_right;
	float pan;
	float delay;
	cs_loaded_sound_t* loaded;
} cs_play_sound_def_t;

cs_playing_sound_t* cs_play_sound(cs_context_t* ctx, cs_play_sound_def_t def);
cs_play_sound_def_t cs_make_def(cs_loaded_sound_t* sound);
void cs_stop_all_sounds(cs_context_t* ctx);

// SDL_RWops specific functions
#if defined(SDL_rwops_h_) && defined(CUTE_SOUND_SDL_RWOPS)

	// Provides the ability to use cs_load_wav with an SDL_RWops object.
	cs_loaded_sound_t cs_load_wav_rw(SDL_RWops* context);

	#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H

		// Provides the ability to use cs_load_ogg with an SDL_RWops object.
		cs_loaded_sound_t cs_load_ogg_rw(SDL_RWops* rw);

	#endif

#endif // SDL_rwops_h_

/**
 * Uniquely identifies a plugin once added to a `cs_context_t` via `cs_add_plugin`.
 */
typedef int cs_plugin_id_t;

/**
 * Plugin interface.
 *
 * A plugin is anyone who implements one of these cs_plugin_interface_t structs
 * and then calls `cs_add_plugin(ctx, plugin_ptr)`. Plugins can be implemented to
 * perform custom operations on playing sounds before they mixed to the audio driver.
 */
typedef struct cs_plugin_interface_t
{
	/**
	 * This pointer is used to represent your plugin instance, and is pased to all callbacks as the
	 * `plugin_instance` pointer. This pointer is not managed by cute sound in any way. You are
	 * responsible for allocating and releasing all resources associated with the plugin instance.
	 */
	void* plugin_instance;

	/**
	 * Called whenever a new sound is starting to play. `playing_sound_udata` is optional, and should
	 * point to whatever data you would like to associate with playing sounds, on a per-playing-sound
	 * basis.
	 *
	 * This function is only called from user threads whenever `cs_play_sound` or `cs_insert_sound`
	 * is called.
	 */
	void (*on_make_playing_sound_fn)(cs_context_t* cs_ctx, void* plugin_instance, void** playing_sound_udata, const cs_playing_sound_t* sound);

	/**
	 * Called once for each time `on_make_playing_sound_fn` is called. The pointer originally assigned
	 * in `on_make_playing_sound_fn` will be passed back here as `playing_sound_udata`. The intent is
	 * to let the user free up any resources associated with the playing sound instance before the sound
	 * is released completely internally.
	 *
	 * This function can be called from the user thread, or from the mixer thread.
	 */
	void (*on_free_playing_sound_fn)(cs_context_t* cs_ctx, void* plugin_instance, void* playing_sound_udata, const cs_playing_sound_t* sound);

	/**
	 * Called when mixing each playing sound instance, once per channel on each playing sound instance.
	 * This function gives the plugin a chance to alter any audio before being mixed by cute_sound to
	 * the audio driver. The source audio is not alterable, however, the plugin can copy `samples_in`
	 * and then output modified samples in `samples_out`.
	 *
	 * `channel_index`       This function is called once per source channel, so `channel_index` will be either
	 *                       0 or 1, depending on the channel count.
	 * `samples_in`          All audio from the playing sound's source.
	 * `sample_count`        The number of samples in `samples_in` and expected in `samples_out`.
	 * `samples_out`         Required to point to a valid samples buffer. The simplest case is to assign `samples_out`
	 *                       directly as `samples_in`, performing no modifications. In most cases though the plugin
	 *                       will make a copy of `(float*)samples_in`, alter the samples, and assign `samples_out` to
	 *                       point to the modified samples. `samples_out` is expected to point to a valid buffer until
	 *                       the next time `on_mix_fn` is called. Typically the plugin can hold a single buffer used
	 *                       for altered samples, and simply reuse the same buffer each time `on_mix_fn` is called.
	 * `playing_sound_udata` The user data `playing_sound_udata` assigned when `on_make_playing_sound_fn` was called.
	 *
	 * This function can be called from the user thread, or from the mixer thread.
	 */
	void (*on_mix_fn)(cs_context_t* cs_ctx, void* plugin_instance, int channel_index, const float* samples_in, int sample_count, float** samples_out, void* playing_sound_udata, const cs_playing_sound_t* sound);
} cs_plugin_interface_t;

cs_plugin_id_t cs_add_plugin(cs_context_t* ctx, const cs_plugin_interface_t* plugin);

#define CUTE_SOUND_H
#endif

#ifdef CUTE_SOUND_IMPLEMENTATION
#ifndef CUTE_SOUND_IMPLEMENTATION_ONCE
#define CUTE_SOUND_IMPLEMENTATION_ONCE

#ifndef CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES
#	define CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES 1024
#endif

#if !defined(CUTE_SOUND_ALLOC)
	#include <stdlib.h>	// malloc, free
	#define CUTE_SOUND_ALLOC(size, ctx) malloc(size)
	#define CUTE_SOUND_FREE(mem, ctx) free(mem)
#endif

#include <stdio.h>	// fopen, fclose
#include <string.h>	// memcmp, memset, memcpy

// Platform detection.
#define CUTE_SOUND_WINDOWS 1
#define CUTE_SOUND_APPLE   2
#define CUTE_SOUND_LINUX   3
#define CUTE_SOUND_SDL     4

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

#elif defined(__linux__)

	#define CUTE_SOUND_PLATFORM CUTE_SOUND_LINUX

#else

	// Just use SDL on other esoteric platforms.
	#define CUTE_SOUND_PLATFORM CUTE_SOUND_SDL

#endif

// Use CUTE_SOUND_FORCE_SDL to override the above macros and use the SDL port.
#ifdef CUTE_SOUND_FORCE_SDL

	#undef CUTE_SOUND_PLATFORM
	#define CUTE_SOUND_PLATFORM CUTE_SOUND_SDL

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

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_LINUX

	#define ALSA_PCM_NEW_HW_PARAMS_API
	#include <alsa/asoundlib.h>
	#include <unistd.h> // nanosleep
	#include <dlfcn.h> // dlopen, dclose, dlsym
	#define timespec // Avoids duplicate definitions.
	#undef timespec // You must compile with POSIX features enabled.
	#include <pthread.h>
	#include <alloca.h>
	#include <assert.h>

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

	#include <SDL2/SDL.h>
	#ifndef _WIN32
		#include <alloca.h>
	#endif

#else

	#error Unsupported platform - please choose one of CUTE_SOUND_WINDOWS, CUTE_SOUND_APPLE, CUTE_SOUND_LINUX or CUTE_SOUND_SDL.

#endif

#ifdef CUTE_SOUND_SCALAR_MODE

	#include <limits.h>

	#define CS_SATURATE16(X) (int16_t)((X) > SHRT_MAX ? SHRT_MAX : ((X) < SHRT_MIN ? SHRT_MIN : (X)))

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
		b.a = a.a;
		b.b = a.b;
		b.c = a.c;
		b.d = a.d;
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
		dst.c[0] = CS_SATURATE16(a.a);
		dst.c[1] = CS_SATURATE16(a.b);
		dst.c[2] = CS_SATURATE16(a.c);
		dst.c[3] = CS_SATURATE16(a.d);
		dst.c[4] = CS_SATURATE16(b.a);
		dst.c[5] = CS_SATURATE16(b.b);
		dst.c[6] = CS_SATURATE16(b.c);
		dst.c[7] = CS_SATURATE16(b.d);
		return dst.m;
	}

#else

	#include <xmmintrin.h>
	#include <emmintrin.h>

	#define cs__m128 __m128
	#define cs__m128i __m128i

	#define cs_mm_set_ps _mm_set_ps
	#define cs_mm_set1_ps _mm_set1_ps
	#define cs_mm_load_ps _mm_load_ps
	#define cs_mm_add_ps _mm_add_ps
	#define cs_mm_mul_ps _mm_mul_ps
	#define cs_mm_cvtps_epi32 _mm_cvtps_epi32
	#define cs_mm_unpacklo_epi32 _mm_unpacklo_epi32
	#define cs_mm_unpackhi_epi32 _mm_unpackhi_epi32
	#define cs_mm_packs_epi32 _mm_packs_epi32

#endif // CUTE_SOUND_SCALAR_MODE

#define CUTE_SOUND_CHECK(X, Y) do { if (!(X)) { cs_error_reason = Y; goto ts_err; } } while (0)
#ifdef __clang__
	#define CUTE_SOUND_ASSERT_INTERNAL __builtin_trap()
#else
	#define CUTE_SOUND_ASSERT_INTERNAL *(int*)0 = 0
#endif
#define CUTE_SOUND_ASSERT(X) do { if (!(X)) CUTE_SOUND_ASSERT_INTERNAL; } while (0)
#define CUTE_SOUND_ALIGN(X, Y) ((((size_t)X) + ((Y) - 1)) & ~((Y) - 1))
#define CUTE_SOUND_TRUNC(X, Y) ((size_t)(X) & ~((Y) - 1))

const char* cs_error_reason;

static void* cs_read_file_to_memory(const char* path, int* size, void* mem_ctx)
{
	(void)mem_ctx;
	void* data = 0;
	FILE* fp = fopen(path, "rb");
	int sizeNum = 0;

	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		sizeNum = (int)ftell(fp);
		fseek(fp, 0, SEEK_SET);
		data = CUTE_SOUND_ALLOC(sizeNum, mem_ctx);
		(void)(fread(data, sizeNum, 1, fp) + 1);
		fclose(fp);
	}

	if (size) *size = sizeNum;
	return data;
}

static int cs_four_cc(const char* CC, void* memory)
{
	if (!memcmp(CC, memory, 4)) return 1;
	return 0;
}

static char* cs_next(char* data)
{
	uint32_t size = *(uint32_t*)(data + 4);
	size = (size + 1) & ~1;
	return data + 8 + size;
}

static void* cs_malloc16(size_t size, void* mem_ctx)
{
	(void)mem_ctx;
	void* p = CUTE_SOUND_ALLOC(size + 16, mem_ctx);
	if (!p) return 0;
	unsigned char offset = (size_t)p & 15;
	p = (void*)CUTE_SOUND_ALIGN(p + 1, 16);
	*((char*)p - 1) = 16 - offset;
	CUTE_SOUND_ASSERT(!((size_t)p & 15));
	return p;
}

static void cs_free16(void* p, void* mem_ctx)
{
	(void)mem_ctx;
	if (!p) return;
	CUTE_SOUND_FREE((char*)p - (((size_t)*((char*)p - 1)) & 0xFF), NULL);
}

static void cs_last_element(cs__m128* a, int i, int j, int16_t* samples, int offset)
{
	switch (offset)
	{
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

void cs_read_mem_wav(const void* memory, int size, cs_loaded_sound_t* sound)
{
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

	sound->playing_count = 0;

	char* data = (char*)memory;
	char* end = data + size;
	CUTE_SOUND_CHECK(data, "Unable to read input file (file doesn't exist, or could not allocate heap memory.");
	CUTE_SOUND_CHECK(cs_four_cc("RIFF", data), "Incorrect file header; is this a WAV file?");
	CUTE_SOUND_CHECK(cs_four_cc("WAVE", data + 8), "Incorrect file header; is this a WAV file?");

	data += 12;

	while (1)
	{
		CUTE_SOUND_CHECK(end > data, "Error searching for fmt chunk.");
		if (cs_four_cc("fmt ", data)) break;
		data = cs_next(data);
	}

	Fmt fmt;
	fmt = *(Fmt*)(data + 8);
	CUTE_SOUND_CHECK(fmt.wFormatTag == 1, "Only PCM WAV files are supported.");
	CUTE_SOUND_CHECK(fmt.nChannels == 1 || fmt.nChannels == 2, "Only mono or stereo supported (too many channels detected).");
	CUTE_SOUND_CHECK(fmt.wBitsPerSample == 16, "Only 16 bits per sample supported.");
	CUTE_SOUND_CHECK(fmt.nBlockAlign == fmt.nChannels * 2, "implementation error");

	sound->sample_rate = (int)fmt.nSamplesPerSec;

	while (1)
	{
		CUTE_SOUND_CHECK(end > data, "Error searching for data chunk.");
		if (cs_four_cc("data", data)) break;
		data = cs_next(data);
	}

	{
		int sample_size = *((uint32_t*)(data + 4));
		int sample_count = sample_size / (fmt.nChannels * sizeof(uint16_t));
		sound->sample_count = sample_count;
		sound->channel_count = fmt.nChannels;

		int wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4);
		wide_count /= 4;
		int wide_offset = sample_count & 3;
		int16_t* samples = (int16_t*)(data + 8);
		float* sample = (float*)alloca(sizeof(float) * 4 + 16);
		sample = (float*)CUTE_SOUND_ALIGN(sample, 16);

		switch (sound->channel_count)
		{
		case 1:
		{
			sound->channels[0] = cs_malloc16(wide_count * sizeof(cs__m128), NULL);
			sound->channels[1] = 0;
			cs__m128* a = (cs__m128*)sound->channels[0];

			for (int i = 0, j = 0; i < wide_count - 1; ++i, j += 4)
			{
				sample[0] = (float)samples[j];
				sample[1] = (float)samples[j + 1];
				sample[2] = (float)samples[j + 2];
				sample[3] = (float)samples[j + 3];
				a[i] = cs_mm_load_ps(sample);
			}

			cs_last_element(a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset);
		}	break;

		case 2:
		{
			cs__m128* a = (cs__m128*)cs_malloc16(wide_count * sizeof(cs__m128) * 2, NULL);
			cs__m128* b = a + wide_count;

			for (int i = 0, j = 0; i < wide_count - 1; ++i, j += 8)
			{
				sample[0] = (float)samples[j];
				sample[1] = (float)samples[j + 2];
				sample[2] = (float)samples[j + 4];
				sample[3] = (float)samples[j + 6];
				a[i] = cs_mm_load_ps(sample);

				sample[0] = (float)samples[j + 1];
				sample[1] = (float)samples[j + 3];
				sample[2] = (float)samples[j + 5];
				sample[3] = (float)samples[j + 7];
				b[i] = cs_mm_load_ps(sample);
			}

			cs_last_element(a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset);
			cs_last_element(b, wide_count - 1, (wide_count - 1) * 4 + 4, samples, wide_offset);
			sound->channels[0] = a;
			sound->channels[1] = b;
		}	break;

		default:
			CUTE_SOUND_CHECK(0, "unsupported channel count (only support mono and stereo).");
		}
	}

	return;

ts_err:
	memset(&sound, 0, sizeof(sound));
}

cs_loaded_sound_t cs_load_wav(const char* path)
{
	cs_loaded_sound_t sound = { 0, 0, 0, 0, { NULL, NULL } };
	int size;
	char* wav = (char*)cs_read_file_to_memory(path, &size, NULL);
	cs_read_mem_wav(wav, size, &sound);
	CUTE_SOUND_FREE(wav, NULL);
	return sound;
}

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL && defined(SDL_rwops_h_) && defined(CUTE_SOUND_SDL_RWOPS)

// Load an SDL_RWops object's data into memory.
// Ripped straight from: https://wiki.libsdl.org/SDL_RWread
static void* cs_read_rw_to_memory(SDL_RWops* rw, int* size, void* mem_ctx)
{
	Sint64 res_size = SDL_RWsize(rw);
	char* data = (char*)CUTE_SOUND_ALLOC((size_t)(res_size + 1), mem_ctx);

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
		CUTE_SOUND_FREE(data, NULL);
		return NULL;
	}

	if (size) *size = (int)res_size;
	return data;
}

cs_loaded_sound_t cs_load_wav_rw(SDL_RWops* context)
{
	cs_loaded_sound_t sound = { 0 };
	int size;
	char* wav = (char*)cs_read_rw_to_memory(context, &size, NULL);
	cs_read_mem_wav(wav, size, &sound);
	CUTE_SOUND_FREE(wav, NULL);
	return sound;
}

#endif

// If stb_vorbis was included *before* cute_sound go ahead and create
// some functions for dealing with OGG files.
#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H

void cs_read_mem_ogg(const void* memory, int length, cs_loaded_sound_t* sound)
{
	int16_t* samples = 0;
	int channel_count;
	int sample_count = stb_vorbis_decode_memory((const unsigned char*)memory, length, &channel_count, &sound->sample_rate, &samples);

	CUTE_SOUND_CHECK(sample_count > 0, "stb_vorbis_decode_memory failed. Make sure your file exists and is a valid OGG file.");

	{
		int wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4) / 4;
		int wide_offset = sample_count & 3;
		float* sample = (float*)alloca(sizeof(float) * 4 + 16);
		sample = (float*)CUTE_SOUND_ALIGN(sample, 16);
		cs__m128* a;
		cs__m128* b;

		switch (channel_count)
		{
		case 1:
		{
			a = (cs__m128*)cs_malloc16(wide_count * sizeof(cs__m128), NULL);
			b = 0;

			for (int i = 0, j = 0; i < wide_count - 1; ++i, j += 4)
			{
				sample[0] = (float)samples[j];
				sample[1] = (float)samples[j + 1];
				sample[2] = (float)samples[j + 2];
				sample[3] = (float)samples[j + 3];
				a[i] = cs_mm_load_ps(sample);
			}

			cs_last_element(a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset);
		}	break;

		case 2:
			a = (cs__m128*)cs_malloc16(wide_count * sizeof(cs__m128) * 2, NULL);
			b = a + wide_count;

			for (int i = 0, j = 0; i < wide_count - 1; ++i, j += 8)
			{
				sample[0] = (float)samples[j];
				sample[1] = (float)samples[j + 2];
				sample[2] = (float)samples[j + 4];
				sample[3] = (float)samples[j + 6];
				a[i] = cs_mm_load_ps(sample);

				sample[0] = (float)samples[j + 1];
				sample[1] = (float)samples[j + 3];
				sample[2] = (float)samples[j + 5];
				sample[3] = (float)samples[j + 7];
				b[i] = cs_mm_load_ps(sample);
			}

			cs_last_element(a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset);
			cs_last_element(b, wide_count - 1, (wide_count - 1) * 4 + 4, samples, wide_offset);
			break;

		default:
			CUTE_SOUND_CHECK(0, "Unsupported channel count.");
		}

		sound->sample_count = sample_count;
		sound->channel_count = channel_count;
		sound->channels[0] = a;
		sound->channels[1] = b;
		sound->playing_count = 0;
		free(samples);
	}
	return;

ts_err:
	free(samples);
	memset(sound, 0, sizeof(cs_loaded_sound_t));
}

cs_loaded_sound_t cs_load_ogg(const char* path)
{
	int length;
	void* memory = cs_read_file_to_memory(path, &length, NULL);
	cs_loaded_sound_t sound = { 0, 0, 0, 0, { NULL, NULL } };
	cs_read_mem_ogg(memory, length, &sound);
	CUTE_SOUND_FREE(memory, NULL);

	return sound;
}

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL && defined(SDL_rwops_h_) && defined(CUTE_SOUND_SDL_RWOPS)

cs_loaded_sound_t cs_load_ogg_rw(SDL_RWops* rw)
{
	int length;
	void* memory = cs_read_rw_to_memory(rw, &length, NULL);
	cs_loaded_sound_t sound = { 0 };
	cs_read_mem_ogg(memory, length, &sound);
	CUTE_SOUND_FREE(memory, NULL);

	return sound;
}

#endif

#endif

void cs_free_sound(cs_loaded_sound_t* sound)
{
	cs_free16(sound->channels[0], NULL);
	memset(sound, 0, sizeof(cs_loaded_sound_t));
}

int cs_sound_size(cs_loaded_sound_t* sound)
{
	return sound->sample_count * sound->channel_count * sizeof(uint16_t);
}

cs_playing_sound_t cs_make_playing_sound(cs_loaded_sound_t* loaded)
{
	cs_playing_sound_t playing;
	playing.active = 0;
	playing.paused = 0;
	playing.looped = 0;
	playing.volume0 = 1.0f;
	playing.volume1 = 1.0f;
	playing.pan0 = 0.5f;
	playing.pan1 = 0.5f;
	playing.sample_index = 0;
	playing.loaded_sound = loaded;
	playing.next = 0;
	for (int i = 0; i < CUTE_SOUND_PLUGINS_MAX; ++i) playing.plugin_udata[i] = NULL;
	return playing;
}

int cs_is_active(cs_playing_sound_t* sound)
{
	return sound->active;
}

void cs_stop_sound(cs_playing_sound_t* sound)
{
	sound->loaded_sound = 0;
	sound->active = 0;
}

void cs_loop_sound(cs_playing_sound_t* sound, int zero_for_no_loop)
{
	sound->looped = zero_for_no_loop;
}

void cs_pause_sound(cs_playing_sound_t* sound, int one_for_paused)
{
	sound->paused = one_for_paused;
}

void cs_set_pan(cs_playing_sound_t* sound, float pan)
{
	if (pan > 1.0f) pan = 1.0f;
	else if (pan < 0.0f) pan = 0.0f;
	float left = 1.0f - pan;
	float right = pan;
	sound->pan0 = left;
	sound->pan1 = right;
}

void cs_set_volume(cs_playing_sound_t* sound, float volume_left, float volume_right)
{
	if (volume_left < 0.0f) volume_left = 0.0f;
	if (volume_right < 0.0f) volume_right = 0.0f;
	sound->volume0 = volume_left;
	sound->volume1 = volume_right;
}

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

void cs_sleep(int milliseconds)
{
	Sleep(milliseconds);
}

struct cs_context_t
{
	unsigned latency_samples;
	unsigned running_index;
	int Hz;
	int bps;
	int buffer_size;
	int wide_count;
	cs_playing_sound_t* playing;
	cs__m128* floatA;
	cs__m128* floatB;
	cs__m128i* samples;
	cs_playing_sound_t* playing_pool;
	cs_playing_sound_t* playing_free;

	// platform specific stuff
	LPDIRECTSOUND dsound;
	LPDIRECTSOUNDBUFFER buffer;
	LPDIRECTSOUNDBUFFER primary;

	// data for cs_mix thread, enable these with cs_spawn_mix_thread
	CRITICAL_SECTION critical_section;
	int separate_thread;
	int running;
	int sleep_milliseconds;

	int plugin_count;
	cs_plugin_interface_t plugins[CUTE_SOUND_PLUGINS_MAX];

	void* mem_ctx;
};

static void cs_release_context(cs_context_t* ctx)
{
	DeleteCriticalSection(&ctx->critical_section);
#ifdef __cplusplus
	ctx->buffer->Release();
	ctx->primary->Release();
	ctx->dsound->Release();
#else
	ctx->buffer->lpVtbl->Release(ctx->buffer);
	ctx->primary->lpVtbl->Release(ctx->primary);
	ctx->dsound->lpVtbl->Release(ctx->dsound);
#endif
	cs_playing_sound_t* playing = ctx->playing;
	while (playing)
	{
		for (int i = 0; i < ctx->plugin_count; ++i)
		{
			cs_plugin_interface_t* plugin = ctx->plugins + i;
			plugin->on_free_playing_sound_fn(ctx, plugin->plugin_instance, playing->plugin_udata[i], playing);
		}
		playing = playing->next;
	}

	CUTE_SOUND_FREE(ctx, ctx->mem_ctx);
}

static DWORD WINAPI cs_ctx_thread(LPVOID lpParameter)
{
	cs_context_t* ctx = (cs_context_t*)lpParameter;

	while (ctx->running)
	{
		cs_mix(ctx);
		if (ctx->sleep_milliseconds) cs_sleep(ctx->sleep_milliseconds);
		else YieldProcessor();
	}

	ctx->separate_thread = 0;
	return 0;
}

void cs_lock(cs_context_t* ctx)
{
	EnterCriticalSection(&ctx->critical_section);
}

void cs_unlock(cs_context_t* ctx)
{
	LeaveCriticalSection(&ctx->critical_section);
}

cs_context_t* cs_make_context(void* hwnd, unsigned play_frequency_in_Hz, int buffered_samples, int playing_pool_count, void* user_allocator_ctx)
{
	buffered_samples = buffered_samples < CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES ? CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES : buffered_samples;
	int bps = sizeof(INT16) * 2;
	int buffer_size = buffered_samples * bps;
	cs_context_t* ctx = 0;
	WAVEFORMATEX format = { 0, 0, 0, 0, 0, 0, 0 };
	DSBUFFERDESC bufdesc = { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } };
	LPDIRECTSOUND dsound;

	CUTE_SOUND_CHECK(hwnd, "Invalid hwnd passed to cs_make_context.");
	{
		HRESULT res = DirectSoundCreate(0, &dsound, 0);
		CUTE_SOUND_CHECK(res == DS_OK, "DirectSoundCreate failed");
#ifdef __cplusplus
		dsound->SetCooperativeLevel((HWND)hwnd, DSSCL_PRIORITY);
#else
		dsound->lpVtbl->SetCooperativeLevel(dsound, (HWND)hwnd, DSSCL_PRIORITY);
#endif
		bufdesc.dwSize = sizeof(bufdesc);
		bufdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

		LPDIRECTSOUNDBUFFER primary_buffer;
#ifdef __cplusplus
		res = dsound->CreateSoundBuffer(&bufdesc, &primary_buffer, 0);
#else
		res = dsound->lpVtbl->CreateSoundBuffer(dsound, &bufdesc, &primary_buffer, 0);
#endif
		CUTE_SOUND_CHECK(res == DS_OK, "Failed to create primary sound buffer");

		format.wFormatTag = WAVE_FORMAT_PCM;
		format.nChannels = 2;
		format.nSamplesPerSec = play_frequency_in_Hz;
		format.wBitsPerSample = 16;
		format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
		format.cbSize = 0;
#ifdef __cplusplus
		res = primary_buffer->SetFormat(&format);
#else
		res = primary_buffer->lpVtbl->SetFormat(primary_buffer, &format);
#endif
		CUTE_SOUND_CHECK(res == DS_OK, "Failed to set format on primary buffer");

		LPDIRECTSOUNDBUFFER secondary_buffer;
		bufdesc.dwSize = sizeof(bufdesc);
		bufdesc.dwFlags = 0;
		bufdesc.dwBufferBytes = buffer_size;
		bufdesc.lpwfxFormat = &format;
#ifdef __cplusplus
		res = dsound->CreateSoundBuffer(&bufdesc, &secondary_buffer, 0);
#else
		res = dsound->lpVtbl->CreateSoundBuffer(dsound, &bufdesc, &secondary_buffer, 0);
#endif
		CUTE_SOUND_CHECK(res == DS_OK, "Failed to set format on secondary buffer");

		int sample_count = buffered_samples;
		int wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4);
		int pool_size = playing_pool_count * sizeof(cs_playing_sound_t);
		int mix_buffers_size = sizeof(cs__m128) * wide_count * 2;
		int sample_buffer_size = sizeof(cs__m128i) * wide_count;
		ctx = (cs_context_t*)CUTE_SOUND_ALLOC(sizeof(cs_context_t) + mix_buffers_size + sample_buffer_size + 16 + pool_size, user_allocator_ctx);
		ctx->latency_samples = 4096;
		ctx->running_index = 0;
		ctx->Hz = play_frequency_in_Hz;
		ctx->bps = bps;
		ctx->buffer_size = buffer_size;
		ctx->wide_count = wide_count;
		ctx->dsound = dsound;
		ctx->buffer = secondary_buffer;
		ctx->primary = primary_buffer;
		ctx->playing = 0;
		ctx->floatA = (cs__m128*)(ctx + 1);
		ctx->floatA = (cs__m128*)CUTE_SOUND_ALIGN(ctx->floatA, 16);
		CUTE_SOUND_ASSERT(!((size_t)ctx->floatA & 15));
		ctx->floatB = ctx->floatA + wide_count;
		ctx->samples = (cs__m128i*)ctx->floatB + wide_count;
		ctx->running = 1;
		ctx->separate_thread = 0;
		ctx->sleep_milliseconds = 0;
		ctx->plugin_count = 0;
		ctx->mem_ctx = user_allocator_ctx;
		InitializeCriticalSectionAndSpinCount(&ctx->critical_section, 0x00000400);

		if (playing_pool_count)
		{
			ctx->playing_pool = (cs_playing_sound_t*)(ctx->samples + wide_count);
			for (int i = 0; i < playing_pool_count - 1; ++i)
				ctx->playing_pool[i].next = ctx->playing_pool + i + 1;
			ctx->playing_pool[playing_pool_count - 1].next = 0;
			ctx->playing_free = ctx->playing_pool;

		}

		else
		{
			ctx->playing_pool = 0;
			ctx->playing_free = 0;
		}

		return ctx;
	}

ts_err:
	CUTE_SOUND_FREE(ctx, ctx->mem_ctx);
	return 0;
}

void cs_spawn_mix_thread(cs_context_t* ctx)
{
	if (ctx->separate_thread) return;
	ctx->separate_thread = 1;
	CreateThread(0, 0, cs_ctx_thread, ctx, 0, 0);
}

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE

void cs_sleep(int milliseconds)
{
	struct timespec ts = { 0, milliseconds * 1000000 };
	nanosleep(&ts, NULL);
}

struct cs_context_t
{
	unsigned latency_samples;
	unsigned index0; // read
	unsigned index1; // write
	unsigned samples_in_circular_buffer;
	int Hz;
	int bps;
	int wide_count;
	int sample_count;
	cs_playing_sound_t* playing;
	cs__m128* floatA;
	cs__m128* floatB;
	cs__m128i* samples;
	cs_playing_sound_t* playing_pool;
	cs_playing_sound_t* playing_free;

	// platform specific stuff
	AudioComponentInstance inst;

	// data for cs_mix thread, enable these with cs_spawn_mix_thread
	pthread_t thread;
	pthread_mutex_t mutex;
	int separate_thread;
	int running;
	int sleep_milliseconds;

	int plugin_count;
	cs_plugin_interface_t plugins[CUTE_SOUND_PLUGINS_MAX];

	void* mem_ctx;
};

static void cs_release_context(cs_context_t* ctx)
{
	pthread_mutex_destroy(&ctx->mutex);
	AudioOutputUnitStop(ctx->inst);
	AudioUnitUninitialize(ctx->inst);
	AudioComponentInstanceDispose(ctx->inst);
	cs_playing_sound_t* playing = ctx->playing;
	while (playing)
	{
		for (int i = 0; i < ctx->plugin_count; ++i)
		{
			cs_plugin_interface_t* plugin = ctx->plugins + i;
			plugin->on_free_playing_sound_fn(ctx, plugin->plugin_instance, playing->plugin_udata[i], playing);
		}
		playing = playing->next;
	}

	CUTE_SOUND_FREE(ctx, ctx->mem_ctx);
}

static void* cs_ctx_thread(void* udata)
{
	cs_context_t* ctx = (cs_context_t*)udata;

	while (ctx->running)
	{
        printf("Entering mix...\n");
		cs_mix(ctx);
		if (ctx->sleep_milliseconds) cs_sleep(ctx->sleep_milliseconds);
		else pthread_yield_np();
	}

	ctx->separate_thread = 0;
	pthread_exit(0);
	return 0;
}

void cs_lock(cs_context_t* ctx)
{
	pthread_mutex_lock(&ctx->mutex);
}

void cs_unlock(cs_context_t* ctx)
{
	pthread_mutex_unlock(&ctx->mutex);
}

static OSStatus cs_memcpy_to_coreaudio(void* udata, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData);

cs_context_t* cs_make_context(void* unused, unsigned play_frequency_in_Hz, int buffered_samples, int playing_pool_count, void* user_allocator_ctx)
{
	buffered_samples = buffered_samples < CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES ? CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES : buffered_samples;
	int bps = sizeof(uint16_t) * 2;

	AudioComponentDescription comp_desc = { 0 };
	comp_desc.componentType = kAudioUnitType_Output;
	comp_desc.componentSubType = kAudioUnitSubType_DefaultOutput;
	comp_desc.componentFlags = 0;
	comp_desc.componentFlagsMask = 0;
	comp_desc.componentManufacturer = kAudioUnitManufacturer_Apple;

	AudioComponent comp = AudioComponentFindNext(NULL, &comp_desc);
	if (!comp)
	{
		cs_error_reason = "Failed to create output unit from AudioComponentFindNext.";
		return 0;
	}

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

	int sample_count = buffered_samples;
	int wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4) / 4;
	int pool_size = playing_pool_count * sizeof(cs_playing_sound_t);
	int mix_buffers_size = sizeof(cs__m128) * wide_count * 2;
	int sample_buffer_size = sizeof(cs__m128i) * wide_count;
	cs_context_t* ctx = (cs_context_t*)CUTE_SOUND_ALLOC(sizeof(cs_context_t) + mix_buffers_size + sample_buffer_size + 16 + pool_size, user_allocator_ctx);
	CUTE_SOUND_CHECK(ret == noErr, "AudioComponentInstanceNew failed");
	ctx->latency_samples = 4096;
	ctx->index0 = 0;
	ctx->index1 = 0;
	ctx->samples_in_circular_buffer = 0;
	ctx->Hz = play_frequency_in_Hz;
	ctx->bps = bps;
	ctx->wide_count = wide_count;
	ctx->sample_count = wide_count * 4;
	ctx->inst = inst;
	ctx->playing = 0;
	ctx->floatA = (cs__m128*)(ctx + 1);
	ctx->floatA = (cs__m128*)CUTE_SOUND_ALIGN(ctx->floatA, 16);
	CUTE_SOUND_ASSERT(!((size_t)ctx->floatA & 15));
	ctx->floatB = ctx->floatA + wide_count;
	ctx->samples = (cs__m128i*)ctx->floatB + wide_count;
	ctx->running = 1;
	ctx->separate_thread = 0;
	ctx->sleep_milliseconds = 0;
	ctx->plugin_count = 0;
	ctx->mem_ctx = user_allocator_ctx;

	ret = AudioUnitSetProperty(inst, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &stream_desc, sizeof(stream_desc));
	CUTE_SOUND_CHECK(ret == noErr, "Failed to set stream forat");

	input.inputProc = cs_memcpy_to_coreaudio;
	input.inputProcRefCon = ctx;
	ret = AudioUnitSetProperty(inst, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof(input));
	CUTE_SOUND_CHECK(ret == noErr, "AudioUnitSetProperty failed");

	ret = AudioUnitInitialize(inst);
	CUTE_SOUND_CHECK(ret == noErr, "Couldn't initialize output unit");

	ret = AudioOutputUnitStart(inst);
	CUTE_SOUND_CHECK(ret == noErr, "Couldn't start output unit");
	pthread_mutex_init(&ctx->mutex, NULL);

	if (playing_pool_count)
	{
		ctx->playing_pool = (cs_playing_sound_t*)(ctx->samples + wide_count);
		for (int i = 0; i < playing_pool_count - 1; ++i)
			ctx->playing_pool[i].next = ctx->playing_pool + i + 1;
		ctx->playing_pool[playing_pool_count - 1].next = 0;
		ctx->playing_free = ctx->playing_pool;
	}

	else
	{
		ctx->playing_pool = 0;
		ctx->playing_free = 0;
	}

	return ctx;

ts_err:
	CUTE_SOUND_FREE(ctx, ctx->mem_ctx);
	return 0;
}

void cs_spawn_mix_thread(cs_context_t* ctx)
{
	if (ctx->separate_thread) return;
	ctx->separate_thread = 1;
	pthread_create(&ctx->thread, 0, cs_ctx_thread, ctx);
}

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_LINUX

void cs_sleep(int milliseconds)
{
	struct timespec ts = { 0, milliseconds * 1000000 };
	nanosleep(&ts, NULL);
}

// Load up ALSA functions manually, so nobody has to deal with compiler linker flags.
typedef struct cs_pcm_functions_t
{
	int (*snd_pcm_open)(snd_pcm_t**, const char*, snd_pcm_stream_t, int);
	int (*snd_pcm_close)(snd_pcm_t* pcm);
	int (*snd_pcm_hw_params_any)(snd_pcm_t*, snd_pcm_hw_params_t*);
	int (*snd_pcm_hw_params_set_access)(snd_pcm_t*, snd_pcm_hw_params_t*, snd_pcm_access_t);
	int (*snd_pcm_hw_params_set_format)(snd_pcm_t*, snd_pcm_hw_params_t*, snd_pcm_format_t);
	int (*snd_pcm_hw_params_set_rate_near)(snd_pcm_t*, snd_pcm_hw_params_t*, unsigned int*, int*);
	int (*snd_pcm_hw_params_set_channels)(snd_pcm_t*, snd_pcm_hw_params_t*, unsigned int);
	int (*snd_pcm_hw_params_set_period_size_near)(snd_pcm_t*, snd_pcm_hw_params_t*, snd_pcm_uframes_t*, int*);
	int (*snd_pcm_hw_params_set_periods_min)(snd_pcm_t*, snd_pcm_hw_params_t*, unsigned int*, int*);
	int (*snd_pcm_hw_params_set_periods_first)(snd_pcm_t*, snd_pcm_hw_params_t*, unsigned int*, int*);
    int (*snd_pcm_hw_params)(snd_pcm_t*, snd_pcm_hw_params_t*);
	int (*snd_pcm_sw_params_current)(snd_pcm_t*, snd_pcm_sw_params_t*);
	int (*snd_pcm_sw_params_set_avail_min)(snd_pcm_t*, snd_pcm_sw_params_t*, snd_pcm_uframes_t);
	int (*snd_pcm_sw_params_set_start_threshold)(snd_pcm_t*, snd_pcm_sw_params_t*, snd_pcm_uframes_t);
	int (*snd_pcm_sw_params)(snd_pcm_t*, snd_pcm_sw_params_t*);
	snd_pcm_sframes_t (*snd_pcm_avail)(snd_pcm_t*);
	snd_pcm_sframes_t (*snd_pcm_writei)(snd_pcm_t*, const void*, snd_pcm_uframes_t);
	size_t (*snd_pcm_hw_params_sizeof)(void);
	size_t (*snd_pcm_sw_params_sizeof)(void);
	int (*snd_pcm_drain)(snd_pcm_t*);
	int (*snd_pcm_recover)(snd_pcm_t*, int, int);
} cs_pcm_functions_t;

#define CUTE_SOUND_DLSYM(fn) \
	do { \
		void* ptr = dlsym(handle, #fn); \
		assert(ptr); \
		memcpy(&fns->fn, &ptr, sizeof(void*)); \
	} while (0)

static void* cs_load_alsa_functions(cs_pcm_functions_t* fns)
{
	void* handle = dlopen("libasound.so", RTLD_NOW);
	if (!handle) return NULL;

	memset(fns, 0, sizeof(*fns));

	CUTE_SOUND_DLSYM(snd_pcm_open);
	CUTE_SOUND_DLSYM(snd_pcm_close);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_any);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_set_access);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_set_format);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_set_rate_near);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_set_format);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_set_channels);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_set_period_size_near);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_set_periods_min);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_set_periods_first);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params);
    CUTE_SOUND_DLSYM(snd_pcm_sw_params);
	CUTE_SOUND_DLSYM(snd_pcm_sw_params_current);
	CUTE_SOUND_DLSYM(snd_pcm_sw_params_set_avail_min);
	CUTE_SOUND_DLSYM(snd_pcm_sw_params_set_start_threshold);
	CUTE_SOUND_DLSYM(snd_pcm_avail);
	CUTE_SOUND_DLSYM(snd_pcm_writei);
	CUTE_SOUND_DLSYM(snd_pcm_hw_params_sizeof);
	CUTE_SOUND_DLSYM(snd_pcm_sw_params_sizeof);
	CUTE_SOUND_DLSYM(snd_pcm_drain);
	CUTE_SOUND_DLSYM(snd_pcm_recover);

	return handle;
}

struct cs_context_t
{
	unsigned latency_samples;
	unsigned index0; // read
	unsigned index1; // write
	unsigned samples_in_circular_buffer;
	int Hz;
	int bps;
	int buffer_size;
	int wide_count;
	int sample_count;
	cs_playing_sound_t* playing;
	cs__m128* floatA;
	cs__m128* floatB;
	cs__m128i* samples;
	cs_playing_sound_t* playing_pool;
	cs_playing_sound_t* playing_free;

	// Platform specific stuff.
	snd_pcm_t* pcm_handle;
	cs_pcm_functions_t fns;
	void *alsa_so;

	// data for cs_mix thread, enable these with cs_spawn_mix_thread
	pthread_t thread;
	pthread_mutex_t mutex;
	int separate_thread;
	int running;
	int sleep_milliseconds;

	int plugin_count;
	cs_plugin_interface_t plugins[CUTE_SOUND_PLUGINS_MAX];

	void* mem_ctx;
};

static void cs_release_context(cs_context_t* ctx)
{
	pthread_mutex_destroy(&ctx->mutex);
	cs_playing_sound_t* playing = ctx->playing;
	while (playing)
	{
		for (int i = 0; i < ctx->plugin_count; ++i)
		{
			cs_plugin_interface_t* plugin = ctx->plugins + i;
			plugin->on_free_playing_sound_fn(ctx, plugin->plugin_instance, playing->plugin_udata[i], playing);
		}
		playing = playing->next;
	}
	ctx->fns.snd_pcm_drain(ctx->pcm_handle);
	ctx->fns.snd_pcm_close(ctx->pcm_handle);
	dlclose(ctx->alsa_so);

	CUTE_SOUND_FREE(ctx, ctx->mem_ctx);
}

int cs_ctx_thread(void* udata)
{
	cs_context_t* ctx = (cs_context_t*)udata;

	while (ctx->running)
	{
		cs_mix(ctx);
		if (ctx->sleep_milliseconds) cs_sleep(ctx->sleep_milliseconds);
		else cs_sleep(1);
	}

	ctx->separate_thread = 0;
	return 0;
}

void cs_lock(cs_context_t* ctx)
{
	pthread_mutex_lock(&ctx->mutex);
}

void cs_unlock(cs_context_t* ctx)
{
	pthread_mutex_unlock(&ctx->mutex);
}

cs_context_t* cs_make_context(void* unused, unsigned play_frequency_in_Hz, int buffered_samples, int playing_pool_count, void* user_allocator_ctx)
{
	buffered_samples = buffered_samples < CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES ? CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES : buffered_samples;
	(void)unused;
	int sample_count = buffered_samples;
	int bps;
	int wide_count;
	int pool_size;
	int mix_buffers_size;
	int sample_buffer_size;
	int res;
	cs_context_t* ctx = NULL;

	// Platform specific things.
	snd_pcm_t* pcm_handle;
	cs_pcm_functions_t fns;
	snd_pcm_uframes_t period_size;
	const char* default_device = "hw:0,0";
	unsigned int periods = 2;
    int dir = 0;
	snd_pcm_hw_params_t *hw_params = NULL;
	snd_pcm_sw_params_t *sw_params = NULL;

// This hack is necessary for some inline'd code in ALSA's headers that tries to
// call these sizeof functions, which we are loading into the cute sound context manually.
#define snd_pcm_hw_params_sizeof fns.snd_pcm_hw_params_sizeof
#define snd_pcm_sw_params_sizeof fns.snd_pcm_sw_params_sizeof

	void* alsa_so = cs_load_alsa_functions(&fns);
	CUTE_SOUND_CHECK(alsa_so, "Unable to load ALSA functions from shared library.");
    printf("%p\n", fns.snd_pcm_open);
	res = fns.snd_pcm_open(&pcm_handle, default_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
	CUTE_SOUND_CHECK(res >= 0, "Failed to open default audio device.");
	snd_pcm_hw_params_alloca(&hw_params);
	res = fns.snd_pcm_hw_params_any(pcm_handle, hw_params);
	CUTE_SOUND_CHECK(res >= 0, "Failed to set default parameters.");
	res = fns.snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
	CUTE_SOUND_CHECK(res >= 0, "Only interleaved sample output is supported by cute_sound, and is not available.");
	res = fns.snd_pcm_hw_params_set_format(pcm_handle, hw_params, SND_PCM_FORMAT_S16_LE);
	CUTE_SOUND_CHECK(res >= 0, "Failed to set sample format.");
	res = fns.snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, (unsigned*)&play_frequency_in_Hz, &dir);
	CUTE_SOUND_CHECK(res >= 0, "Failed to set sample rate.");
	res = fns.snd_pcm_hw_params_set_channels(pcm_handle, hw_params, 2);
	CUTE_SOUND_CHECK(res >= 0, "Failed to set channel count.");
	period_size = (snd_pcm_uframes_t)sample_count;
	res = fns.snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &period_size, &dir);
	CUTE_SOUND_CHECK(res >= 0, "Failed to set buffer size, likely not enough RAM for these settings.");
	res = fns.snd_pcm_hw_params_set_periods_min(pcm_handle, hw_params, &periods, &dir);
	CUTE_SOUND_CHECK(res >= 0, "Failed to set double buffering (min), likely not enough RAM for these settings.");
	res = fns.snd_pcm_hw_params_set_periods_first(pcm_handle, hw_params, &periods, &dir);
	CUTE_SOUND_CHECK(res >= 0, "Failed to set double buffering (first), likely not enough RAM for these settings.");
	res = fns.snd_pcm_hw_params(pcm_handle, hw_params);
	CUTE_SOUND_CHECK(res >= 0, "Failed to send hardware parameters to the driver.");
    snd_pcm_sw_params_alloca(&sw_params);
	CUTE_SOUND_CHECK(res >= 0, "Failed to allocate software parameters.");
	res = fns.snd_pcm_sw_params_current(pcm_handle, sw_params);
	CUTE_SOUND_CHECK(res >= 0, "Failed to initialize software parameters.");
	res = fns.snd_pcm_sw_params_set_avail_min(pcm_handle, sw_params, period_size);
	CUTE_SOUND_CHECK(res >= 0, "Failed to set minimum available count.");
	res = fns.snd_pcm_sw_params_set_start_threshold(pcm_handle, sw_params, 1);
	CUTE_SOUND_CHECK(res >= 0, "Failed to set start threshold.");
	res = fns.snd_pcm_sw_params(pcm_handle, sw_params);
	CUTE_SOUND_CHECK(res >= 0, "Failed to send software parameters to the driver.");

	// Platform independent things.
	sample_count = (int)period_size;
	bps = sizeof(uint16_t) * 2;
	wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4) / 4;
	pool_size = playing_pool_count * sizeof(cs_playing_sound_t);
	mix_buffers_size = sizeof(cs__m128) * wide_count * 2;
	sample_buffer_size = sizeof(cs__m128i) * wide_count;

	ctx = (cs_context_t*)CUTE_SOUND_ALLOC(sizeof(cs_context_t) + mix_buffers_size + sample_buffer_size + 16 + pool_size, user_allocator_ctx);
	CUTE_SOUND_CHECK(ctx != NULL, "Can't create audio context");
	ctx->latency_samples = 4096;
	ctx->index0 = 0;
	ctx->index1 = 0;
	ctx->samples_in_circular_buffer = 0;
	ctx->Hz = play_frequency_in_Hz;
	ctx->bps = bps;
	ctx->wide_count = wide_count;
	ctx->sample_count = wide_count * 4;
	ctx->playing = 0;
	ctx->floatA = (cs__m128*)(ctx + 1);
	ctx->floatA = (cs__m128*)CUTE_SOUND_ALIGN(ctx->floatA, 16);
	CUTE_SOUND_ASSERT(!((size_t)ctx->floatA & 15));
	ctx->floatB = ctx->floatA + wide_count;
	ctx->samples = (cs__m128i*)ctx->floatB + wide_count;
	ctx->running = 1;
	ctx->separate_thread = 0;
	ctx->sleep_milliseconds = 0;
	ctx->plugin_count = 0;
	ctx->mem_ctx = user_allocator_ctx;

	// Platform dependent assignments.
	ctx->fns = fns;
	ctx->pcm_handle = pcm_handle;
	ctx->alsa_so = alsa_so;
	pthread_mutex_init(&ctx->mutex, NULL);

	if (playing_pool_count)
	{
		ctx->playing_pool = (cs_playing_sound_t*)(ctx->samples + wide_count);
		for (int i = 0; i < playing_pool_count - 1; ++i)
			ctx->playing_pool[i].next = ctx->playing_pool + i + 1;
		ctx->playing_pool[playing_pool_count - 1].next = 0;
		ctx->playing_free = ctx->playing_pool;
	}

	else
	{
		ctx->playing_pool = 0;
		ctx->playing_free = 0;
	}

	return ctx;

ts_err:
	if (ctx) CUTE_SOUND_FREE(ctx, ctx->mem_ctx);
	return 0;
}

void cs_spawn_mix_thread(cs_context_t* ctx)
{
	if (ctx->separate_thread) return;
	ctx->separate_thread = 1;
	pthread_create(&ctx->thread, NULL, (void* (*)(void*))cs_ctx_thread, ctx);
}

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

void cs_sleep(int milliseconds)
{
	SDL_Delay(milliseconds);
}

struct cs_context_t
{
	unsigned latency_samples;
	unsigned index0; // read
	unsigned index1; // write
	unsigned samples_in_circular_buffer;
	int Hz;
	int bps;
	int buffer_size;
	int wide_count;
	int sample_count;
	cs_playing_sound_t* playing;
	cs__m128* floatA;
	cs__m128* floatB;
	cs__m128i* samples;
	cs_playing_sound_t* playing_pool;
	cs_playing_sound_t* playing_free;
	SDL_AudioDeviceID dev;

	// data for cs_mix thread, enable these with cs_spawn_mix_thread
	SDL_Thread* thread;
	SDL_mutex* mutex;
	int separate_thread;
	int running;
	int sleep_milliseconds;

	int plugin_count;
	cs_plugin_interface_t plugins[CUTE_SOUND_PLUGINS_MAX];

	void* mem_ctx;
};

static void cs_release_context(cs_context_t* ctx)
{
	SDL_DestroyMutex(ctx->mutex);
	cs_playing_sound_t* playing = ctx->playing;
	while (playing)
	{
		for (int i = 0; i < ctx->plugin_count; ++i)
		{
			cs_plugin_interface_t* plugin = ctx->plugins + i;
			plugin->on_free_playing_sound_fn(ctx, plugin->plugin_instance, playing->plugin_udata[i], playing);
		}
		playing = playing->next;
	}
	SDL_CloseAudioDevice(ctx->dev);

	CUTE_SOUND_FREE(ctx, ctx->mem_ctx);
}

int cs_ctx_thread(void* udata)
{
	cs_context_t* ctx = (cs_context_t*)udata;

	while (ctx->running)
	{
		cs_mix(ctx);
		if (ctx->sleep_milliseconds) cs_sleep(ctx->sleep_milliseconds);
		else cs_sleep(1);
	}

	ctx->separate_thread = 0;
	return 0;
}

void cs_lock(cs_context_t* ctx)
{
	SDL_LockMutex(ctx->mutex);
}

void cs_unlock(cs_context_t* ctx)
{
	SDL_UnlockMutex(ctx->mutex);
}

static void cs_sdl_audio_callback(void* udata, Uint8* stream, int len);

cs_context_t* cs_make_context(void* unused, unsigned play_frequency_in_Hz, int buffered_samples, int playing_pool_count, void* user_allocator_ctx)
{
	buffered_samples = buffered_samples < CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES ? CUTE_SOUND_MINIMUM_BUFFERED_SAMPLES : buffered_samples;
	(void)unused;
	int bps = sizeof(uint16_t) * 2;
	int sample_count = buffered_samples;
	int wide_count = (int)CUTE_SOUND_ALIGN(sample_count, 4) / 4;
	int pool_size = playing_pool_count * sizeof(cs_playing_sound_t);
	int mix_buffers_size = sizeof(cs__m128) * wide_count * 2;
	int sample_buffer_size = sizeof(cs__m128i) * wide_count;
	cs_context_t* ctx = 0;
	SDL_AudioSpec wanted, have;
	int ret = SDL_InitSubSystem(SDL_INIT_AUDIO);
	CUTE_SOUND_CHECK(ret >= 0, "Can't init SDL audio");

	ctx = (cs_context_t*)CUTE_SOUND_ALLOC(sizeof(cs_context_t) + mix_buffers_size + sample_buffer_size + 16 + pool_size, user_allocator_ctx);
	CUTE_SOUND_CHECK(ctx != NULL, "Can't create audio context");
	ctx->latency_samples = 4096;
	ctx->index0 = 0;
	ctx->index1 = 0;
	ctx->samples_in_circular_buffer = 0;
	ctx->Hz = play_frequency_in_Hz;
	ctx->bps = bps;
	ctx->wide_count = wide_count;
	ctx->sample_count = wide_count * 4;
	ctx->playing = 0;
	ctx->floatA = (cs__m128*)(ctx + 1);
	ctx->floatA = (cs__m128*)CUTE_SOUND_ALIGN(ctx->floatA, 16);
	CUTE_SOUND_ASSERT(!((size_t)ctx->floatA & 15));
	ctx->floatB = ctx->floatA + wide_count;
	ctx->samples = (cs__m128i*)ctx->floatB + wide_count;
	ctx->running = 1;
	ctx->separate_thread = 0;
	ctx->sleep_milliseconds = 0;
	ctx->plugin_count = 0;
	ctx->mem_ctx = user_allocator_ctx;

	SDL_memset(&wanted, 0, sizeof(wanted));
	SDL_memset(&have, 0, sizeof(have));
	wanted.freq = play_frequency_in_Hz;
	wanted.format = AUDIO_S16SYS;
	wanted.channels = 2; /* 1 = mono, 2 = stereo */
	wanted.samples = buffered_samples;
	wanted.callback = cs_sdl_audio_callback;
	wanted.userdata = ctx;
	ctx->dev = SDL_OpenAudioDevice(NULL, 0, &wanted, &have, 0);
	CUTE_SOUND_CHECK(ctx->dev >= 0, "Can't open SDL audio");
	SDL_PauseAudioDevice(ctx->dev, 0);
	ctx->mutex = SDL_CreateMutex();

	if (playing_pool_count)
	{
		ctx->playing_pool = (cs_playing_sound_t*)(ctx->samples + wide_count);
		for (int i = 0; i < playing_pool_count - 1; ++i)
			ctx->playing_pool[i].next = ctx->playing_pool + i + 1;
		ctx->playing_pool[playing_pool_count - 1].next = 0;
		ctx->playing_free = ctx->playing_pool;
	}

	else
	{
		ctx->playing_pool = 0;
		ctx->playing_free = 0;
	}

	return ctx;

ts_err:
	if (ctx) CUTE_SOUND_FREE(ctx, ctx->mem_ctx);
	return 0;
}

void cs_spawn_mix_thread(cs_context_t* ctx)
{
	if (ctx->separate_thread) return;
	ctx->separate_thread = 1;
	ctx->thread = SDL_CreateThread(&cs_ctx_thread, "CuteSoundThread", ctx);
}

#endif // CUTE_SOUND_PLATFORM == CUTE_SOUND_***

// Platform-agnostic functions that access cs_context_t members go here.

cs_playing_sound_t* cs_get_playing(cs_context_t* ctx)
{
	return ctx->playing;
}

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL || CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE

static int cs_samples_written(cs_context_t* ctx)
{
	return ctx->samples_in_circular_buffer;
}

static int cs_samples_unwritten(cs_context_t* ctx)
{
	return ctx->sample_count - ctx->samples_in_circular_buffer;
}

static int cs_samples_to_mix(cs_context_t* ctx)
{
	int lat = ctx->latency_samples;
	int written = cs_samples_written(ctx);
	int dif = lat - written;
	CUTE_SOUND_ASSERT(dif >= 0);
	if (dif)
	{
		int unwritten = cs_samples_unwritten(ctx);
		return dif < unwritten ? dif : unwritten;
	}
	return 0;
}

#define CUTE_SOUND_SAMPLES_TO_BYTES(interleaved_sample_count) ((interleaved_sample_count) * ctx->bps)
#define CUTE_SOUND_BYTES_TO_SAMPLES(byte_count) ((byte_count) / ctx->bps)

static void cs_push_bytes(cs_context_t* ctx, void* data, int size)
{
	int index1 = ctx->index1;
	int samples_to_write = CUTE_SOUND_BYTES_TO_SAMPLES(size);
	int sample_count = ctx->sample_count;

	int unwritten = cs_samples_unwritten(ctx);
	if (unwritten < samples_to_write) samples_to_write = unwritten;
	int samples_to_end = sample_count - index1;

	if (samples_to_write > samples_to_end)
	{
		memcpy((char*)ctx->samples + CUTE_SOUND_SAMPLES_TO_BYTES(index1), data, CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end));
		memcpy(ctx->samples, (char*)data + CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end), size - CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end));
		ctx->index1 = (samples_to_write - samples_to_end) % sample_count;
	}

	else
	{
		memcpy((char*)ctx->samples + CUTE_SOUND_SAMPLES_TO_BYTES(index1), data, size);
		ctx->index1 = (ctx->index1 + samples_to_write) % sample_count;
	}

	ctx->samples_in_circular_buffer += samples_to_write;
}

static int cs_pull_bytes(cs_context_t* ctx, void* dst, int size)
{
	int index0 = ctx->index0;
	int allowed_size = CUTE_SOUND_SAMPLES_TO_BYTES(cs_samples_written(ctx));
	int sample_count = ctx->sample_count;
	int zeros = 0;

	if (allowed_size < size)
	{
		zeros = size - allowed_size;
		size = allowed_size;
	}

	int samples_to_read = CUTE_SOUND_BYTES_TO_SAMPLES(size);
	int samples_to_end = sample_count - index0;

	if (samples_to_read > samples_to_end)
	{
		memcpy(dst, ((char*)ctx->samples) + CUTE_SOUND_SAMPLES_TO_BYTES(index0), CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end));
		memcpy(((char*)dst) + CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end), ctx->samples, size - CUTE_SOUND_SAMPLES_TO_BYTES(samples_to_end));
		ctx->index0 = (samples_to_read - samples_to_end) % sample_count;
	}

	else
	{
		memcpy(dst, ((char*)ctx->samples) + CUTE_SOUND_SAMPLES_TO_BYTES(index0), size);
		ctx->index0 = (ctx->index0 + samples_to_read) % sample_count;
	}

	ctx->samples_in_circular_buffer -= samples_to_read;

	return zeros;
}

#endif

void cs_shutdown_context(cs_context_t* ctx)
{
	if (ctx->separate_thread)
	{
		cs_lock(ctx);
		ctx->running = 0;
		cs_unlock(ctx);
	}

	while (ctx->separate_thread) cs_sleep(1);
	cs_release_context(ctx);
}

void cs_thread_sleep_delay(cs_context_t* ctx, int milliseconds)
{
	ctx->sleep_milliseconds = milliseconds;
}

static void s_on_make_playing(cs_context_t* ctx, cs_playing_sound_t* sound)
{
	for (int i = 0; i < ctx->plugin_count; ++i)
	{
		cs_plugin_interface_t* plugin = ctx->plugins + i;
		void* udata = NULL;
		plugin->on_make_playing_sound_fn(ctx, plugin->plugin_instance, &udata, sound);
		if (udata) sound->plugin_udata[i] = udata;
	}
}

int cs_insert_sound(cs_context_t* ctx, cs_playing_sound_t* sound)
{
	// Cannot use cs_playing_sound_t if cs_make_context was passed non-zero for playing_pool_count
	// since non-zero playing_pool_count means the context is doing some memory-management
	// for a playing sound pool. InsertSound assumes the pool does not exist, and is apart
	// of the lower-level API (see top of this header for documentation details).
	CUTE_SOUND_ASSERT(ctx->playing_pool == 0);

	if (sound->active) return 0;
	cs_lock(ctx);
	sound->next = ctx->playing;
	ctx->playing = sound;
	sound->loaded_sound->playing_count += 1;
	sound->active = 1;

	s_on_make_playing(ctx, sound);

	cs_unlock(ctx);

	return 1;
}

// NOTE: does not allow delay_in_seconds to be negative (clamps at 0)
void cs_set_delay(cs_context_t* ctx, cs_playing_sound_t* sound, float delay_in_seconds)
{
	if (delay_in_seconds < 0.0f) delay_in_seconds = 0.0f;
	sound->sample_index = (int)(delay_in_seconds * (float)ctx->Hz);
	sound->sample_index = -(int)CUTE_SOUND_ALIGN(sound->sample_index, 4);
}

cs_play_sound_def_t cs_make_def(cs_loaded_sound_t* sound)
{
	cs_play_sound_def_t def;
	def.paused = 0;
	def.looped = 0;
	def.volume_left = 1.0f;
	def.volume_right = 1.0f;
	def.pan = 0.5f;
	def.delay = 0.0f;
	def.loaded = sound;
	return def;
}

cs_playing_sound_t* cs_play_sound(cs_context_t* ctx, cs_play_sound_def_t def)
{
	cs_lock(ctx);

	cs_playing_sound_t* playing = ctx->playing_free;
	if (!playing) {
		cs_unlock(ctx);
		return 0;
	}
	ctx->playing_free = playing->next;
	*playing = cs_make_playing_sound(def.loaded);
	playing->active = 1;
	playing->paused = def.paused;
	playing->looped = def.looped;
	cs_set_volume(playing, def.volume_left, def.volume_right);
	cs_set_pan(playing, def.pan);
	cs_set_delay(ctx, playing, def.delay);
	playing->next = ctx->playing;
	ctx->playing = playing;
	playing->loaded_sound->playing_count += 1;

	s_on_make_playing(ctx, playing);

	cs_unlock(ctx);

	return playing;
}


void cs_stop_all_sounds(cs_context_t* ctx)
{
	// This is apart of the high level API, not the low level API.
	// If using the low level API you must write your own function to
	// stop playing all sounds.
	CUTE_SOUND_ASSERT(ctx->playing_pool != 0);

	cs_lock(ctx);
	cs_playing_sound_t* sound = ctx->playing;
	while (sound)
	{
		// let cs_mix() remove the sound
		sound->active = 0;
		sound = sound->next;
	}
	cs_unlock(ctx);
}

cs_plugin_id_t cs_add_plugin(cs_context_t* ctx, const cs_plugin_interface_t* plugin)
{
	CUTE_SOUND_ASSERT(ctx->plugin_count < CUTE_SOUND_PLUGINS_MAX);
	ctx->plugins[ctx->plugin_count++] = *plugin;
	return ctx->plugin_count - 1;
}

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

static void cs_position(cs_context_t* ctx, int* byte_to_lock, int* bytes_to_write)
{
	// compute bytes to be written to direct sound
	DWORD play_cursor;
	DWORD write_cursor;
#ifdef __cplusplus
	HRESULT hr = ctx->buffer->GetCurrentPosition(&play_cursor, &write_cursor);
#else
	HRESULT hr = ctx->buffer->lpVtbl->GetCurrentPosition(ctx->buffer, &play_cursor, &write_cursor);
#endif
	CUTE_SOUND_ASSERT(hr == DS_OK);

	DWORD lock = (ctx->running_index * ctx->bps) % ctx->buffer_size;
	DWORD target_cursor = (write_cursor + ctx->latency_samples * ctx->bps);
	if (target_cursor > (DWORD)ctx->buffer_size) target_cursor %= ctx->buffer_size;
	target_cursor = (DWORD)CUTE_SOUND_ALIGN(target_cursor, 16);
	DWORD write;

	if (lock > target_cursor)
	{
		write = (ctx->buffer_size - lock) + target_cursor;
	}

	else
	{
		write = target_cursor - lock;
	}

	*byte_to_lock = lock;
	*bytes_to_write = write;
}

static void cs_memcpy_to_directsound(cs_context_t* ctx, int16_t* samples, int byte_to_lock, int bytes_to_write)
{
	// copy mixer buffers to direct sound
	void* region1;
	DWORD size1;
	void* region2;
	DWORD size2;
#ifdef __cplusplus
	HRESULT hr = ctx->buffer->Lock(byte_to_lock, bytes_to_write, &region1, &size1, &region2, &size2, 0);

	if (hr == DSERR_BUFFERLOST)
	{
		ctx->buffer->Restore();
		hr = ctx->buffer->Lock(byte_to_lock, bytes_to_write, &region1, &size1, &region2, &size2, 0);
	}
#else
	HRESULT hr = ctx->buffer->lpVtbl->Lock(ctx->buffer, byte_to_lock, bytes_to_write, &region1, &size1, &region2, &size2, 0);

	if (hr == DSERR_BUFFERLOST)
	{
		ctx->buffer->lpVtbl->Restore(ctx->buffer);
		hr = ctx->buffer->lpVtbl->Lock(ctx->buffer, byte_to_lock, bytes_to_write, &region1, &size1, &region2, &size2, 0);
	}
#endif

	if (!SUCCEEDED(hr))
		return;

	unsigned running_index = ctx->running_index;
	INT16* sample1 = (INT16*)region1;
	DWORD sample1_count = size1 / ctx->bps;
	memcpy(sample1, samples, sample1_count * sizeof(INT16) * 2);
	samples += sample1_count * 2;
	running_index += sample1_count;

	INT16* sample2 = (INT16*)region2;
	DWORD sample2_count = size2 / ctx->bps;
	memcpy(sample2, samples, sample2_count * sizeof(INT16) * 2);
	samples += sample2_count * 2;
	running_index += sample2_count;

#ifdef __cplusplus
	ctx->buffer->Unlock(region1, size1, region2, size2);
#else
	ctx->buffer->lpVtbl->Unlock(ctx->buffer, region1, size1, region2, size2);
#endif
	ctx->running_index = running_index;

	// meager hack to fill out sound buffer before playing
	static int first;
	if (!first)
	{
#ifdef __cplusplus
		ctx->buffer->Play(0, 0, DSBPLAY_LOOPING);
#else
		ctx->buffer->lpVtbl->Play(ctx->buffer, 0, 0, DSBPLAY_LOOPING);
#endif
		first = 1;
	}
}

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE

static OSStatus cs_memcpy_to_coreaudio(void* udata, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData)
{
	cs_context_t* ctx = (cs_context_t*)udata;
	int bps = ctx->bps;
	int samples_requested_to_consume = inNumberFrames;
	AudioBuffer* buffer = ioData->mBuffers;

	CUTE_SOUND_ASSERT(ioData->mNumberBuffers == 1);
	CUTE_SOUND_ASSERT(buffer->mNumberChannels == 2);
	int byte_size = buffer->mDataByteSize;
	CUTE_SOUND_ASSERT(byte_size == samples_requested_to_consume * bps);

	int zero_bytes = cs_pull_bytes(ctx, buffer->mData, byte_size);
	memset(((char*)buffer->mData) + (byte_size - zero_bytes), 0, zero_bytes);

	return noErr;
}

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

static void cs_sdl_audio_callback(void* udata, Uint8* stream, int len)
{
	cs_context_t* ctx = (cs_context_t*)udata;
	int zero_bytes = cs_pull_bytes(ctx, stream, len);
	memset(stream + (len - zero_bytes), 0, zero_bytes);
}

#endif

void cs_mix(cs_context_t* ctx)
{
	// These variables have to be declared before any gotos to compile
	// as C++.
	cs_playing_sound_t** ptr;
	cs__m128i* samples;
	cs__m128* floatA;
	cs__m128* floatB;
	cs__m128 zero;
	int wide_count;
	int samples_to_write;

	cs_lock(ctx);

#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

	int byte_to_lock;
	int bytes_to_write;
	cs_position(ctx, &byte_to_lock, &bytes_to_write);

	if (!bytes_to_write) goto unlock;
	samples_to_write = bytes_to_write / ctx->bps;

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE || CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL

	int bytes_to_write;
	samples_to_write = cs_samples_to_mix(ctx);
	if (!samples_to_write) goto unlock;
	bytes_to_write = samples_to_write * ctx->bps;

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_LINUX

	int ret;
	snd_pcm_sframes_t frames = ctx->fns.snd_pcm_avail(ctx->pcm_handle);
	if (frames == -EAGAIN) goto unlock; // No data yet.
	else if (frames < 0) { /* Fatal error... How should this be handled? */ }
	else if (frames == 0) goto unlock;
	samples_to_write = (int)frames;
	if (samples_to_write > (int)ctx->latency_samples) samples_to_write = ctx->latency_samples;

#endif

	// clear mixer buffers
	wide_count = (int)CUTE_SOUND_ALIGN(samples_to_write, 4) / 4;

	floatA = ctx->floatA;
	floatB = ctx->floatB;
	zero = cs_mm_set1_ps(0.0f);

	for (int i = 0; i < wide_count; ++i)
	{
		floatA[i] = zero;
		floatB[i] = zero;
	}

	// mix all playing sounds into the mixer buffers
	ptr = &ctx->playing;
	while (*ptr)
	{
		cs_playing_sound_t* playing = *ptr;
		cs_loaded_sound_t* loaded = playing->loaded_sound;

		// immediately remove any inactive elements
		if (!playing->active || !ctx->running)
			goto remove;

		if (!loaded)
			goto remove;

		// skip all paused sounds
		if (playing->paused)
			goto get_next_playing_sound;

		{
			cs__m128* cA = (cs__m128*)loaded->channels[0];
			cs__m128* cB = (cs__m128*)loaded->channels[1];

			// Attempted to play a sound with no audio.
			// Make sure the audio file was loaded properly. Check for
			// error messages in cs_error_reason.
			CUTE_SOUND_ASSERT(cA);

			int mix_count = samples_to_write;
			int offset = playing->sample_index;
			int remaining = loaded->sample_count - offset;
			if (remaining < mix_count) mix_count = remaining;
			CUTE_SOUND_ASSERT(remaining > 0);

			float vA0 = playing->volume0 * playing->pan0;
			float vB0 = playing->volume1 * playing->pan1;
			cs__m128 vA = cs_mm_set1_ps(vA0);
			cs__m128 vB = cs_mm_set1_ps(vB0);

			// skip sound if it's delay is longer than mix_count and
			// handle various delay cases
			int delay_offset = 0;
			if (offset < 0)
			{
				int samples_till_positive = -offset;
				int mix_leftover = mix_count - samples_till_positive;

				if (mix_leftover <= 0)
				{
					playing->sample_index += mix_count;
					goto get_next_playing_sound;
				}

				else
				{
					offset = 0;
					delay_offset = samples_till_positive;
					mix_count = mix_leftover;
				}
			}
			CUTE_SOUND_ASSERT(!(delay_offset & 3));

			// SIMD offets
			int mix_wide = (int)CUTE_SOUND_ALIGN(mix_count, 4) / 4;
			int offset_wide = (int)CUTE_SOUND_TRUNC(offset, 4) / 4;
			int delay_wide = (int)CUTE_SOUND_ALIGN(delay_offset, 4) / 4;
			int sample_count = (mix_wide - 2 * delay_wide) * 4;

			// Give all plugins a chance to inject altered samples into the mix streams.
			for (int i = 0; i < ctx->plugin_count; ++i)
			{
				cs_plugin_interface_t* plugin = ctx->plugins + i;
				float* plugin_samples_in_channel_a = (float*)(cA + delay_wide + offset_wide);
				float* plugin_samples_in_channel_b = (float*)(cB + delay_wide + offset_wide);
				float* samples_out_channel_a = NULL;
				float* samples_out_channel_b = NULL;
				plugin->on_mix_fn(ctx, plugin->plugin_instance, 0, plugin_samples_in_channel_a, sample_count, &samples_out_channel_a, playing->plugin_udata[i], playing);
				if (loaded->channel_count == 2) plugin->on_mix_fn(ctx, plugin->plugin_instance, 1, plugin_samples_in_channel_b, sample_count, &samples_out_channel_b, playing->plugin_udata[i], playing);
				if (samples_out_channel_a) cA = (cs__m128*)samples_out_channel_a;
				if (samples_out_channel_b) cB = (cs__m128*)samples_out_channel_b;

				// Set offset_wide to cancel out delay_wide because cA and cB are now owned by the plugin,
				// this elimineating the need for the delay offset.
				if (!!samples_out_channel_a | !!samples_out_channel_b) offset_wide = -delay_wide;
			}

			// apply volume, load samples into float buffers
			switch (loaded->channel_count)
			{
			case 1:
				for (int i = delay_wide; i < mix_wide - delay_wide; ++i)
				{
					cs__m128 A = cA[i + offset_wide];
					cs__m128 B = cs_mm_mul_ps(A, vB);
					A = cs_mm_mul_ps(A, vA);
					floatA[i] = cs_mm_add_ps(floatA[i], A);
					floatB[i] = cs_mm_add_ps(floatB[i], B);
				}
				break;

			case 2:
			{
				for (int i = delay_wide; i < mix_wide - delay_wide; ++i)
				{
					cs__m128 A = cA[i + offset_wide];
					cs__m128 B = cB[i + offset_wide];

					A = cs_mm_mul_ps(A, vA);
					B = cs_mm_mul_ps(B, vB);
					floatA[i] = cs_mm_add_ps(floatA[i], A);
					floatB[i] = cs_mm_add_ps(floatB[i], B);
				}
			}	break;
			}

			// playing list logic
			playing->sample_index += mix_count;
			if (playing->sample_index == loaded->sample_count)
			{
				if (playing->looped)
				{
					playing->sample_index = 0;
					goto get_next_playing_sound;
				}

				goto remove;
			}
		}

	get_next_playing_sound:
		if (*ptr) ptr = &(*ptr)->next;
		else break;
		continue;

	remove:
		playing->sample_index = 0;
		*ptr = (*ptr)->next;
		playing->next = 0;
		playing->active = 0;

		if (playing->loaded_sound)
		{
			playing->loaded_sound->playing_count -= 1;
			CUTE_SOUND_ASSERT(playing->loaded_sound->playing_count >= 0);
		}

		// Notify plugins of this playing sound instance being released.
		for (int i = 0; i < ctx->plugin_count; ++i)
		{
			cs_plugin_interface_t* plugin = ctx->plugins + i;
			plugin->on_free_playing_sound_fn(ctx, plugin->plugin_instance, playing->plugin_udata[i], playing);
			playing->plugin_udata[i] = NULL;
		}

		// if using high-level API manage the cs_playing_sound_t memory ourselves
		if (ctx->playing_pool)
		{
			playing->next = ctx->playing_free;
			ctx->playing_free = playing;
		}

		continue;
	}

	// load all floats into 16 bit packed interleaved samples
#if CUTE_SOUND_PLATFORM == CUTE_SOUND_WINDOWS

	samples = ctx->samples;
	for (int i = 0; i < wide_count; ++i)
	{
		cs__m128i a = cs_mm_cvtps_epi32(floatA[i]);
		cs__m128i b = cs_mm_cvtps_epi32(floatB[i]);
		cs__m128i a0b0a1b1 = cs_mm_unpacklo_epi32(a, b);
		cs__m128i a2b2a3b3 = cs_mm_unpackhi_epi32(a, b);
		samples[i] = cs_mm_packs_epi32(a0b0a1b1, a2b2a3b3);
	}
	cs_memcpy_to_directsound(ctx, (int16_t*)samples, byte_to_lock, bytes_to_write);

#elif CUTE_SOUND_PLATFORM == CUTE_SOUND_APPLE || CUTE_SOUND_PLATFORM == CUTE_SOUND_SDL || CUTE_SOUND_PLATFORM == CUTE_SOUND_LINUX

	// Since the ctx->samples array is already in use as a ring buffer
	// reusing floatA to store output is a good way to temporarly store
	// the final samples. Then a single ring buffer push can be used
	// afterwards. Pretty hacky, but whatever :)
	samples = (cs__m128i*)floatA;
	for (int i = 0; i < wide_count; ++i)
	{
		cs__m128i a = cs_mm_cvtps_epi32(floatA[i]);
		cs__m128i b = cs_mm_cvtps_epi32(floatB[i]);
		cs__m128i a0b0a1b1 = cs_mm_unpacklo_epi32(a, b);
		cs__m128i a2b2a3b3 = cs_mm_unpackhi_epi32(a, b);
		samples[i] = cs_mm_packs_epi32(a0b0a1b1, a2b2a3b3);
	}

	// SDL/CoreAudio use a callback mechanism communicating with cute sound
	// over a ring buffer (accessed by cs_push_bytes and cs_pull_bytes), but
	// ALSA on Linux has their own memcpy-style function to use... So we don't
	// need a local ring buffer at all, and can directly hand over the samples.
	#if CUTE_SOUND_PLATFORM != CUTE_SOUND_LINUX
		cs_push_bytes(ctx, samples, bytes_to_write);
	#else
		ret = ctx->fns.snd_pcm_writei(ctx->pcm_handle, samples, (snd_pcm_sframes_t)samples_to_write);
		if (ret < 0) ret = ctx->fns.snd_pcm_recover(ctx->pcm_handle, ret, 0);
		if (ret < 0) {
			// A fatal error occured.
			ctx->separate_thread = 0;
		}
	#endif

#endif

	unlock:
	cs_unlock(ctx);
}

#endif // CUTE_SOUND_IMPLEMENTATION_ONCE
#endif // CUTE_SOUND_IMPLEMENTATION

/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2017 Randy Gaul http://www.randygaul.net
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
