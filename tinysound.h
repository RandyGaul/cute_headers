/*
	tinysound.h - v1.06

	Summary:
	tinysound is a C API for loading, playing, looping, panning and fading mono
	and stero sounds. This means tinysound imparts no external DLLs or large
	libraries that adversely effect shipping size. tinysound can also run on
	Windows XP since DirectSound ships with all recent versions of Windows.
	tinysound implements a custom SSE2 mixer by explicitly locking and unlocking
	portions of an internal. tinysound uses CoreAudio for Apple machines (like
	OSX and iOS).

	Revision history:
		1.0  (06/04/2016) initial release
		1.01 (06/06/2016) load WAV from memory
		                  separate portable and OS-specific code in tsMix
		                  fixed bug causing audio glitches when sounds ended
		                  added stb_vorbis loaders + demo example
		1.02 (06/08/2016) error checking + strings in vorbis loaders
		                  SSE2 implementation of mixer
		                  fix typos on docs/comments
		                  corrected volume bug introduced in 1.01
		1.03 (07/05/2016) size calculation helper (to know size of sound in
		                  bytes on the heap) tsSoundSize
		1.04 (12/06/2016) merged in Aaron Balint's contributions
		                  SFFT and pitch functions from Stephan M. Bernsee
		                  tsMix can run on its own thread with tsSpawnMixThread
		                  updated documentation, typo fixes
		                  fixed typo in malloc16 that caused heap corruption
		1.05 (12/08/2016) tsStopAllSounds, suggested by Aaron Balint
		1.06 (02/17/2017) port to CoreAudio for Apple machines
*/

/*
	Contributors:
		Aaron Balint      1.04 - real time pitch
		                  1.04 - separate thread for tsMix
		                  1.04 - bugfix, removed extra free16 call for second channel
*/

/*
	To create implementation (the function definitions)
		#define TS_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	DOCUMENTATION (very quick intro):
	1. create context
	2. load sounds from disk into memory
	3. play sounds
	4. free context

	1. tsContext* ctx = tsMakeContext( hwnd, frequency, latency, seconds, N );
	2. tsPlaySoundDef def = tsMakeDef( &tsLoadWAV( "path_to_file/filename.wav" ) );
	3. tsPlaySound( ctx, def );
	4. tsShutdownContext( ctx );

	DOCUMENTATION (longer introduction):
	tinysound consists of tsLoadedSounds, tsPlayingSounds and the tsContext.
	The tsContext encapsulates an OS sound API, as well as buffers + settings.
	tsLoadedSound holds raw samples of a sound. tsPlayingSound is an instance
	of a tsLoadedSound that represents a sound that can be played through the
	tsContext.

	There are two main versions of the API, the low-level and the high-level
	API. The low-level API does not manage any memory for tsPlayingSounds. The
	high level api holds a memory pool of playing sounds.

	High-level API:
		First create a context and pass in non-zero to the final parameter. This
		final parameter controls how large of a memory pool to use for tsPlayingSounds.
		Here's an example where N is the size of the internal pool:

		tsContext* ctx = tsMakeContext( hwnd, frequency, latency, seconds, N );

		We create tsPlayingSounds indirectly with tsPlayDef structs. tsPlayDef is a
		POD struct so feel free to make them straight on the stack. The tsPlayDef
		sets up initialization parameters. Here's an example to load a wav and
		play it:

		tsLoadedSound loaded = tsLoadWAV( "path_to_file/filename.wav" );
		tsPlaySoundDef def = tsMakeDef( &loaded );
		tsPlayingSound* sound = tsPlaySound( ctx, def );

		The same def can be used to play as many sounds as desired (even simultaneously)
		as long as the context playing sound pool is large enough.

	Low-level API:
		First create a context and pass 0 in the final parameter (0 here means
		the context will *not* allocate a tsPlayingSound memory pool):

		tsContext* ctx = tsMakeContext( hwnd, frequency, latency, seconds, 0 );

		parameters:
			hwnd           --  HWND, handle to window (on OSX just pass in 0)
			frequency      --  int, represents Hz frequency rate in which samples are played
			latency        --  int, estimated latency in Hz from PlaySound call to speaker output
			seconds        --  int, number of second of samples internal buffers can hold
			0 (last param) --  int, number of elements in tsPlayingSound pool

		We create a tsPlayingSound like so:
		tsLoadedSound loaded = tsLoadWAV( "path_to_file/filename.wav" );
		tsPlayingSound playing_sound = tsMakePlayingSound( &loaded );

		Then to play the sound we do:
		tsInsertSound( ctx, &playing_sound );

		The above tsInsertSound function call will place playing_sound into
		a singly-linked list inside the context. The context will remove
		the sound from its internal list when it finishes playing.

	WARNING: The high-level API cannot be mixed with the low-level API. If you
	try then the internal code will assert and crash. Pick one and stick with it.
	Usually he high-level API will be used, but if someone is *really* picky about
	their memory usage, or wants more control, the low-level API can be used.

	Here is the Low-Level API:
		tsPlayingSound tsMakePlayingSound( tsLoadedSound* loaded );
		void tsInsertSound( tsContext* ctx, tsPlayingSound* sound );

	Here is the High-Level API:
		tsPlayingSound* tsPlaySound( tsContext* ctx, tsPlaySoundDef def );
		tsPlaySoundDef tsMakeDef( tsLoadedSound* sound );
		void tsStopAllSounds( tsContext( ctx );

	Be sure to link against dsound.dll (or dsound.lib) on Windows.

	Read the rest of the header for specific details on all available functions
	and struct types.
*/

/*
	Known Limitations:

	* No Linux support -- only Windows/OSX/iOS. tinysound is port-ready! Please consider adding
		in a Linux port if you have the time to do so.
	* PCM mono/stereo format is the only formats the LoadWAV function supports. I don't
		guarantee it will work for all kinds of wav files, but it certainly does for the common
		kind (and can be changed fairly easily if someone wanted to extend it).
	* Only supports 16 bits per sample.
	* Mixer does not do any fancy clipping. The algorithm is to convert all 16 bit samples
		to float, mix all samples, and write back to audio API as 16 bit integers. In
		practice this works very well and clipping is not often a big problem.
	* I'm not super familiar with good ways to avoid the DirectSound play cursor from going
		past the write cursor. To mitigate this pass in a larger number to tsMakeContext's 4th
		parameter (buffer scale in seconds).
	* Pitch shifting uses some code from 1996, so it's super slow. This should probably be
		rewritten using SIMD intrinsics. Also for some reason the pitch shift code requires some
		dynamic memory in order to store intermediary data, so it can process small chunks
		of a sound at a time. This seems like code smells and dynamic memory probably should not
		be required at all. Also getting rid of the WOL license would be great.
*/

/*
	FAQ
	Q : Why DirectSound instead of (insert API here)?
	A : Casey Muratori documented DS on Handmade Hero, other APIs do not have such good docs. DS has
	shipped on Windows XP all the way through Windows 10 -- using this header effectively intro-
	duces zero dependencies for the foreseeable future. The DS API itself is sane enough to quickly
	implement needed features, and users won't hear the difference between various APIs. Latency is
	not that great with DS but it is shippable.

	Q : Why not include Linux support?
	A : I don't have time right now. I'm sure somewhere out there some great programmers already
	know all about how to do audio on Linux. tinysound is port-ready, so by all means, please
	contribute to the project and submit a pull request!

	Q : I would like to use my own memory management, how can I achieve this?
	A : This header makes a couple uses of malloc/free, and malloc16/free16. Simply find these bits
	and replace them with your own memory allocation routines. They can be wrapped up into a macro,
	or call your own functions directly -- it's up to you. Generally these functions allocate fairly
	large chunks of memory, and not very often (if at all), with one exception: tsSetPitch is a very
	expensive routine and requires frequent dynamic memory management.
*/

#if !defined( TINYSOUND_H )

#define TS_WINDOWS	1
#define TS_MAC		2
#define TS_UNIX		3

#if defined( _WIN32 )
	#define TS_PLATFORM TS_WINDOWS
#elif defined( __APPLE__ )
	#define TS_PLATFORM TS_MAC
#else
	#define TS_PLATFORM TS_UNIX
	#error unsupported platform
#endif

#include <stdint.h>

// read this in the event of tsLoadWAV/tsLoadOGG errors
// also read this in the event of certain errors from tsMakeContext
extern const char* g_tsErrorReason;

// stores a loaded sound in memory
typedef struct
{
	int sample_count;
	int channel_count;
	void* channels[ 2 ];
} tsLoadedSound;

struct tsPitchShift;
typedef struct tsPitchShift tsPitchShift;

// represents an instance of a tsLoadedSound, can be played through the tsContext
typedef struct tsPlayingSound
{
	int active;
	int paused;
	int looped;
	float volume0;
	float volume1;
	float pan0;
	float pan1;
	float pitch;
	tsPitchShift* pitch_filter[ 2 ];
	int sample_index;
	tsLoadedSound* loaded_sound;
	struct tsPlayingSound* next;
} tsPlayingSound;

// holds audio API info and other info
struct tsContext;
typedef struct tsContext tsContext;

// The returned struct will contain a null pointer in tsLoadedSound::channel[ 0 ]
// in the case of errors. Read g_tsErrorReason string for details on what happened.
// Calls tsReadMemWAV internally.
tsLoadedSound tsLoadWAV( const char* path );

// Reads a WAV file from memory. Still allocates memory for the tsLoadedSound since
// WAV format will interlace stereo, and we need separate data streams to do SIMD
// properly.
void tsReadMemWAV( const void* memory, tsLoadedSound* sound );

// If stb_vorbis was included *before* tinysound go ahead and create
// some functions for dealing with OGG files.
#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H
void tsReadMemOGG( const void* memory, int length, int* sample_rate, tsLoadedSound* sound );
tsLoadedSound tsLoadOGG( const char* path, int* sample_rate );
#endif

// Uses free16 (aligned free, implemented later in this file) to free up both of
// the channels stored within sound
void tsFreeSound( tsLoadedSound* sound );

// Returns the size, in bytes, of all heap-allocated memory for this particular
// loaded sound
int tsSoundSize( tsLoadedSound* sound );

// playing_pool_count -- 0 to setup low-level API, non-zero to size the internal
// memory pool for tsPlayingSound instances
tsContext* tsMakeContext( void* hwnd, unsigned play_frequency_in_Hz, int latency_factor_in_Hz, int num_buffered_seconds, int playing_pool_count );
void tsShutdownContext( tsContext* ctx );

// Call tsSpawnMixThread once to setup a separate thread for the context to run
// upon. The separate thread will continually call tsMix and perform mixing
// operations.
void tsSpawnMixThread( tsContext* ctx );

// Use tsThreadSleepDelay to specify a custom sleep delay time.
// A sleep will occur after each call to tsMix. By default YieldProcessor
// is used, and no sleep occurs. Use a sleep delay to conserve CPU bandwidth.
// A recommended sleep time is a little less than 1/2 your predicted 1/FPS.
// 60 fps is 16 ms, so about 1-5 should work well in most cases.
void tsThreadSleepDelay( tsContext* ctx, int milliseconds );

// Call this manually, once per game tick recommended, if you haven't ever
// called tsSpawnMixThread. Otherwise the thread will call tsMix itself.
// num_samples_to_write is not used on Windows. On Mac it is used to push
// samples into a circular buffer while CoreAudio simultaneously pulls samples
// off of the buffer. num_samples_to_write should be computed each update tick
// as delta_time * play_frequency_in_Hz + 1.
void tsMix( tsContext* ctx );

// All of the functions in this next section should only be called if tsIsActive
// returns true. Calling them otherwise probably won't do anything bad, but it
// won't do anything at all. If a sound is active it resides in the context's
// internal list of playing sounds.
int tsIsActive( tsPlayingSound* sound );

// Flags sound for removal. Upon next tsMix call will remove sound from playing
// list. If high-level API used sound is placed onto the internal free list.
void tsStopSound( tsPlayingSound* sound );

void tsLoopSound( tsPlayingSound* sound, int zero_for_no_loop );
void tsPauseSound( tsPlayingSound* sound, int one_for_paused );

// lerp from 0 to 1, 0 full left, 1 full right
void tsSetPan( tsPlayingSound* sound, float pan );

// explicitly set volume of each channel. Can be used as panning (but it's
// recommended to use the tsSetPan function for panning).
void tsSetVolume( tsPlayingSound* sound, float volume_left, float volume_right );

// Change pitch (not duration) of sound. pitch = 0.5f for one octave lower, pitch = 2.0f for one octave higher.
// pitch at 1.0f applies no change. pitch settings farther away from 1.0f create more distortion and lower
// the output sample quality. pitch can be adjusted in real-time for doppler effects and the like. Going beyond
// 0.5f and 2.0f may require some tweaking of the smbPitchShift function. See this link for more info:
// http://blogs.zynaptiq.com/bernsee/pitch-shifting-using-the-ft/

// Additional important information about performance: This function
// is quite expensive -- you have been warned! Try it out and be aware of how much CPU consumption it uses.
// To avoid destroying the originally loaded sound samples, tsSetPitch will do a one-time allocation to copy
// sound samples into a new buffer. The new buffer contains the pitch adjusted samples, and these will be played
// through tsMix. This lets the pitch be modulated at run-time, but requires dynamically allocated memory. The
// memory is freed once the sound finishes playing. If a one-time pitch adjustment is desired, for performance
// reasons please consider doing an off-line pitch adjustment manually as a pre-processing step for your sounds.
// Also, consider changing malloc16 and free16 to match your custom memory allocation needs. Try adjusting
// TS_PITCH_QUALITY and see how this affects your performance.
void tsSetPitch( tsPlayingSound* sound, float pitch );

// Delays sound before actually playing it. Requires context to be passed in
// since there's a conversion from seconds to samples per second.
// If one were so inclined another version could be implemented like:
// void tsSetDelay( tsPlayingSound* sound, float delay, int samples_per_second )
void tsSetDelay( tsContext* ctx, tsPlayingSound* sound, float delay_in_seconds );

// Portable sleep function
void tsSleep( int milliseconds );

// LOW-LEVEL API
tsPlayingSound tsMakePlayingSound( tsLoadedSound* loaded );
void tsInsertSound( tsContext* ctx, tsPlayingSound* sound );

// HIGH-LEVEL API
typedef struct
{
	int paused;
	int looped;
	float volume_left;
	float volume_right;
	float pan;
	float pitch;
	float delay;
	tsLoadedSound* loaded;
} tsPlaySoundDef;

tsPlayingSound* tsPlaySound( tsContext* ctx, tsPlaySoundDef def );
tsPlaySoundDef tsMakeDef( tsLoadedSound* sound );
void tsStopAllSounds( tsContext* ctx );

#define TINYSOUND_H
#endif

#ifdef TS_IMPLEMENTATION

#define _CRT_SECURE_NO_WARNINGS FUCK_YOU
#include <stdlib.h>	// malloc, free
#include <stdio.h>	// fopen, fclose
#include <string.h>	// memcmp, memset, memcpy
#include <xmmintrin.h>
#include <emmintrin.h>

#if TS_PLATFORM == TS_WINDOWS

	#include <dsound.h>
	#undef PlaySound
	#pragma comment( lib, "dsound.lib" )

#elif TS_PLATFORM == TS_MAC

	#include <CoreAudio/CoreAudio.h>
	#include <AudioUnit/AudioUnit.h>
	#include <pthread.h>
	#include <mach/mach_time.h>

#else
#endif

#define TS_CHECK( X, Y ) do { if ( !(X) ) { g_tsErrorReason = Y; goto err; } } while ( 0 )
#if TS_PLATFORM == TS_MAC && defined( __clang__ )
    #define TS_ASSERT_INTERNAL __builtin_trap( )
#else
    #define TS_ASSERT_INTERNAL *(int*)0 = 0
#endif
#define TS_ASSERT( X ) do { if ( !(X) ) TS_ASSERT_INTERNAL; } while ( 0 )
#define TS_ALIGN( X, Y ) ((((size_t)X) + ((Y) - 1)) & ~((Y) - 1))
#define TS_TRUNC( X, Y ) ((size_t)(X) & ~((Y) - 1))

const char* g_tsErrorReason;

static void* tsReadFileToMemory( const char* path, int* size )
{
	void* data = 0;
	FILE* fp = fopen( path, "rb" );
	int sizeNum = 0;

	if ( fp )
	{
		fseek( fp, 0, SEEK_END );
		sizeNum = (int)ftell( fp );
		fseek( fp, 0, SEEK_SET );
		data = malloc( sizeNum );
		fread( data, sizeNum, 1, fp );
		fclose( fp );
	}

	if ( size ) *size = sizeNum;
	return data;
}

static int tsFourCC( const char* CC, void* memory )
{
	if ( !memcmp( CC, memory, 4 ) ) return 1;
	return 0;
}

static char* tsNext( char* data )
{
	uint32_t size = *(uint32_t*)(data + 4);
	size = (size + 1) & ~1;
	return data + 8 + size;
}

static void* malloc16( size_t size )
{
	void* p = malloc( size + 16 );
	if ( !p ) return 0;
	unsigned char offset = (size_t)p & 15;
	p = (void*)TS_ALIGN( p + 1, 16 );
	*((char*)p - 1) = 16 - offset;
	TS_ASSERT( !((size_t)p & 15) );
	return p;
}

static void free16( void* p )
{
	if ( !p ) return;
	free( (char*)p - (size_t)*((char*)p - 1) );
}

static void tsLastElement( __m128* a, int i, int j, int16_t* samples, int offset )
{
	switch ( offset )
	{
	case 1:
		a[ i ] = _mm_set_ps( samples[ j ], 0.0f, 0.0f, 0.0f );
		break;

	case 2:
		a[ i ] = _mm_set_ps( samples[ j ], samples[ j + 1 ], 0.0f, 0.0f );
		break;

	case 3:
		a[ i ] = _mm_set_ps( samples[ j ], samples[ j + 1 ], samples[ j + 2 ], 0.0f );
		break;

	case 0:
		a[ i ] = _mm_set_ps( samples[ j ], samples[ j + 1 ], samples[ j + 2 ], samples[ j + 3 ] );
		break;
	}
}

void tsReadMemWAV( const void* memory, tsLoadedSound* sound )
{
	#pragma pack( push, 1 )
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
		uint8_t SubFormat[ 18 ];
	} Fmt;
	#pragma pack( pop )

	char* data = (char*)memory;
	TS_CHECK( data, "Unable to read input file (file doesn't exist, or could not allocate heap memory." );
	TS_CHECK( tsFourCC( "RIFF", data ), "Incorrect file header; is this a WAV file?" );
	TS_CHECK( tsFourCC( "WAVE", data + 8 ), "Incorrect file header; is this a WAV file?" );

	data += 12;

	TS_CHECK( tsFourCC( "fmt ", data ), "fmt chunk not found." );
	Fmt fmt;
	fmt = *(Fmt*)(data + 8);
	TS_CHECK( fmt.wFormatTag == 1, "Only PCM WAV files are supported." );
	TS_CHECK( fmt.nChannels == 1 || fmt.nChannels == 2, "Only mono or stereo supported (too many channels detected)." );
	TS_CHECK( fmt.wBitsPerSample == 16, "Only 16 bits per sample supported." );
	TS_CHECK( fmt.nBlockAlign == fmt.nChannels * 2, "implementation error" );

	data = tsNext( data );
	TS_CHECK( tsFourCC( "data", data ), "data chunk not found." );
	int sample_size = *((uint32_t*)(data + 4));
	int sample_count = sample_size / (fmt.nChannels * sizeof( uint16_t ));
	sound->sample_count = sample_count;
	sound->channel_count = fmt.nChannels;

	int wide_count = (int)TS_ALIGN( sample_count, 4 );
	wide_count /= 4;
	int wide_offset = sample_count & 3;
	int16_t* samples = (int16_t*)(data + 8);
	float* sample = (float*)alloca( sizeof( float ) * 4 + 16 );
	sample = (float*)TS_ALIGN( sample, 16 );

	switch ( sound->channel_count )
	{
	case 1:
	{
		sound->channels[ 0 ] = malloc16( wide_count * sizeof( __m128 ) );
		sound->channels[ 1 ] = 0;
		__m128* a = (__m128*)sound->channels[ 0 ];

		for ( int i = 0, j = 0; i < wide_count - 1; ++i, j += 4 )
		{
			sample[ 0 ] = (float)samples[ j ];
			sample[ 1 ] = (float)samples[ j + 1 ];
			sample[ 2 ] = (float)samples[ j + 2 ];
			sample[ 3 ] = (float)samples[ j + 3 ];
			a[ i ] = _mm_load_ps( sample );
		}

		tsLastElement( a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset );
	}	break;

	case 2:
	{
		__m128* a = (__m128*)malloc16( wide_count * sizeof( __m128 ) * 2 );
		__m128* b = a + wide_count;

		for ( int i = 0, j = 0; i < wide_count - 1; ++i, j += 8 )
		{
			sample[ 0 ] = (float)samples[ j ];
			sample[ 1 ] = (float)samples[ j + 2 ];
			sample[ 2 ] = (float)samples[ j + 4 ];
			sample[ 3 ] = (float)samples[ j + 6 ];
			a[ i ] = _mm_load_ps( sample );

			sample[ 0 ] = (float)samples[ j + 1 ];
			sample[ 1 ] = (float)samples[ j + 3 ];
			sample[ 2 ] = (float)samples[ j + 5 ];
			sample[ 3 ] = (float)samples[ j + 7 ];
			b[ i ] = _mm_load_ps( sample );
		}

		tsLastElement( a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset );
		tsLastElement( b, wide_count - 1, (wide_count - 1) * 4 + 4, samples, wide_offset );
		sound->channels[ 0 ] = a;
		sound->channels[ 1 ] = b;
	}	break;

	default:
		TS_CHECK( 0, "unsupported channel count (only support mono and stereo)." );
	}

	return;

err:
	memset( &sound, 0, sizeof( sound ) );
}

tsLoadedSound tsLoadWAV( const char* path )
{
	tsLoadedSound sound = { 0 };
	char* wav = (char*)tsReadFileToMemory( path, 0 );
	tsReadMemWAV( wav, &sound );
	free( wav );
	return sound;
}

// If stb_vorbis was included *before* tinysound go ahead and create
// some functions for dealing with OGG files.
#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H
void tsReadMemOGG( const void* memory, int length, int* sample_rate, tsLoadedSound* sound )
{
	int16_t* samples = 0;
	int channel_count;
	int sample_count = stb_vorbis_decode_memory( (const unsigned char*)memory, length, &channel_count, sample_rate, &samples );

	TS_CHECK( sample_count > 0, "stb_vorbis_decode_memory failed. Make sure your file exists and is a valid OGG file." );

	int wide_count = (int)TS_ALIGN( sample_count, 4 ) / 4;
	int wide_offset = sample_count & 3;
	float* sample = (float*)alloca( sizeof( float ) * 4 + 16 );
	sample = (float*)TS_ALIGN( sample, 16 );
	__m128* a;
	__m128* b;

	switch ( channel_count )
	{
	case 1:
	{
		a = malloc16( wide_count * sizeof( __m128 ) );
		b = 0;

		for ( int i = 0, j = 0; i < wide_count - 1; ++i, j += 4 )
		{
			sample[ 0 ] = (float)samples[ j ];
			sample[ 1 ] = (float)samples[ j + 1 ];
			sample[ 2 ] = (float)samples[ j + 2 ];
			sample[ 3 ] = (float)samples[ j + 3 ];
			a[ i ] = _mm_load_ps( sample );
		}

		tsLastElement( a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset );
	}	break;

	case 2:
		a = (__m128*)malloc16( wide_count * sizeof( __m128 ) * 2 );
		b = a + wide_count;

		for ( int i = 0, j = 0; i < wide_count - 1; ++i, j += 8 )
		{
			sample[ 0 ] = (float)samples[ j ];
			sample[ 1 ] = (float)samples[ j + 2 ];
			sample[ 2 ] = (float)samples[ j + 4 ];
			sample[ 3 ] = (float)samples[ j + 6 ];
			a[ i ] = _mm_load_ps( sample );

			sample[ 0 ] = (float)samples[ j + 1 ];
			sample[ 1 ] = (float)samples[ j + 3 ];
			sample[ 2 ] = (float)samples[ j + 5 ];
			sample[ 3 ] = (float)samples[ j + 7 ];
			b[ i ] = _mm_load_ps( sample );
		}

		tsLastElement( a, wide_count - 1, (wide_count - 1) * 4, samples, wide_offset );
		tsLastElement( b, wide_count - 1, (wide_count - 1) * 4 + 4, samples, wide_offset );
		break;

	default:
		TS_CHECK( 0, "Unsupported channel count." );
	}

	sound->sample_count = sample_count;
	sound->channel_count = channel_count;
	sound->channels[ 0 ] = a;
	sound->channels[ 1 ] = b;
	free( samples );
	return;

	err:
	free( samples );
	memset( sound, 0, sizeof( tsLoadedSound ) );
}

tsLoadedSound tsLoadOGG( const char* path, int* sample_rate )
{
	int length;
	void* memory = tsReadFileToMemory( path, &length );
	tsLoadedSound sound;
	tsReadMemOGG( memory, length, sample_rate, &sound );
	free( memory );

	return sound;
}
#endif

void tsFreeSound( tsLoadedSound* sound )
{
	free16( sound->channels[ 0 ] );
	memset( sound, 0, sizeof( tsLoadedSound ) );
}

int tsSoundSize( tsLoadedSound* sound )
{
	return sound->sample_count * sound->channel_count * sizeof( uint16_t );
}

tsPlayingSound tsMakePlayingSound( tsLoadedSound* loaded )
{
	tsPlayingSound playing;
	playing.active = 0;
	playing.paused = 0;
	playing.looped = 0;
	playing.volume0 = 1.0f;
	playing.volume1 = 1.0f;
	playing.pan0 = 0.5f;
	playing.pan1 = 0.5f;
	playing.pitch = 1.0f;
	playing.pitch_filter[ 0 ] = 0;
	playing.pitch_filter[ 1 ] = 0;
	playing.sample_index = 0;
	playing.loaded_sound = loaded;
	playing.next = 0;
	return playing;
}

int tsIsActive( tsPlayingSound* sound )
{
	return sound->active;
}

void tsStopSound( tsPlayingSound* sound )
{
	sound->active = 0;
}

void tsLoopSound( tsPlayingSound* sound, int zero_for_no_loop )
{
	sound->looped = zero_for_no_loop;
}

void tsPauseSound( tsPlayingSound* sound, int one_for_paused )
{
	sound->paused = one_for_paused;
}

void tsSetPan( tsPlayingSound* sound, float pan )
{
	if ( pan > 1.0f ) pan = 1.0f;
	else if ( pan < 0.0f ) pan = 0.0f;
	float left = 1.0f - pan;
	float right = pan;
	sound->pan0 = left;
	sound->pan1 = right;
}

void tsSetPitch( tsPlayingSound* sound, float pitch )
{
	sound->pitch = pitch;
}

void tsSetVolume( tsPlayingSound* sound, float volume_left, float volume_right )
{
	if ( volume_left < 0.0f ) volume_left = 0.0f;
	if ( volume_right < 0.0f ) volume_right = 0.0f;
	sound->volume0 = volume_left;
	sound->volume1 = volume_right;
}

static void tsRemoveFilter( tsPlayingSound* playing );

#if TS_PLATFORM == TS_WINDOWS

    void tsSleep( int milliseconds )
    {
        Sleep( milliseconds );
    }

	struct tsContext
	{
		unsigned latency_samples;
		unsigned running_index;
		int Hz;
		int bps;
		int buffer_size;
		int wide_count;
		tsPlayingSound* playing;
		__m128* floatA;
		__m128* floatB;
		__m128i* samples;
		tsPlayingSound* playing_pool;
		tsPlayingSound* playing_free;

		// platform specific stuff
		LPDIRECTSOUND dsound;
		LPDIRECTSOUNDBUFFER buffer;
		LPDIRECTSOUNDBUFFER primary;

		// data for tsMix thread, enable these with tsSpawnMixThread
		CRITICAL_SECTION critical_section;
		int separate_thread;
		int running;
		int sleep_milliseconds;
	};

	static void tsReleaseContext( tsContext* ctx )
	{
		if ( ctx->separate_thread )	DeleteCriticalSection( &ctx->critical_section );
		ctx->buffer->lpVtbl->Release( ctx->buffer );
		ctx->primary->lpVtbl->Release( ctx->primary );
		ctx->dsound->lpVtbl->Release( ctx->dsound );
		tsPlayingSound* playing = ctx->playing;
		while ( playing )
		{
			tsRemoveFilter( playing );
			playing = playing->next;
		}
		free( ctx );
	}

	static DWORD WINAPI tsCtxThread( LPVOID lpParameter )
	{
		tsContext* ctx = (tsContext*)lpParameter;

		while ( ctx->running )
		{
			tsMix( ctx );
			if ( ctx->sleep_milliseconds ) tsSleep( ctx->sleep_milliseconds );
			else YieldProcessor( );
		}

		ctx->separate_thread = 0;
		return 0;
	}

	static void tsLock( tsContext* ctx )
	{
		if ( ctx->separate_thread ) EnterCriticalSection( &ctx->critical_section );
	}

	static void tsUnlock( tsContext* ctx )
	{
		if ( ctx->separate_thread ) LeaveCriticalSection( &ctx->critical_section );
	}

	tsContext* tsMakeContext( void* hwnd, unsigned play_frequency_in_Hz, int latency_factor_in_Hz, int num_buffered_seconds, int playing_pool_count )
	{
		int bps = sizeof( INT16 ) * 2;
		int buffer_size = play_frequency_in_Hz * bps * num_buffered_seconds;

		LPDIRECTSOUND dsound;
		HRESULT res = DirectSoundCreate( 0, &dsound, 0 );
		dsound->lpVtbl->SetCooperativeLevel( dsound, (HWND)hwnd, DSSCL_PRIORITY );
		DSBUFFERDESC bufdesc = { 0 };
		bufdesc.dwSize = sizeof( bufdesc );
		bufdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

		LPDIRECTSOUNDBUFFER primary_buffer;
		res = dsound->lpVtbl->CreateSoundBuffer( dsound, &bufdesc, &primary_buffer, 0 );

		WAVEFORMATEX format = { 0 };
		format.wFormatTag = WAVE_FORMAT_PCM;
		format.nChannels = 2;
		format.nSamplesPerSec = play_frequency_in_Hz;
		format.wBitsPerSample = 16;
		format.nBlockAlign = (format.nChannels * format.wBitsPerSample) / 8;
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
		format.cbSize = 0;
		res = primary_buffer->lpVtbl->SetFormat( primary_buffer, &format );

		LPDIRECTSOUNDBUFFER secondary_buffer;
		bufdesc.dwSize = sizeof( bufdesc );
		bufdesc.dwFlags = 0;
		bufdesc.dwBufferBytes = buffer_size;
		bufdesc.lpwfxFormat = &format;
		res = dsound->lpVtbl->CreateSoundBuffer( dsound, &bufdesc, &secondary_buffer, 0 );

		int sample_count = play_frequency_in_Hz * num_buffered_seconds;
		int wide_count = (int)TS_ALIGN( sample_count, 4 );
		int pool_size = playing_pool_count * sizeof( tsPlayingSound );
		int mix_buffers_size = sizeof( __m128 ) * wide_count * 2;
		int sample_buffer_size = sizeof( __m128i ) * wide_count;
		tsContext* ctx = (tsContext*)malloc( sizeof( tsContext ) + mix_buffers_size + sample_buffer_size + 16 + pool_size );
		ctx->latency_samples = (unsigned)TS_ALIGN( play_frequency_in_Hz / latency_factor_in_Hz, 4 );
		ctx->running_index = 0;
		ctx->Hz = play_frequency_in_Hz;
		ctx->bps = bps;
		ctx->buffer_size = buffer_size;
		ctx->wide_count = wide_count;
		ctx->dsound = dsound;
		ctx->buffer = secondary_buffer;
		ctx->primary = primary_buffer;
		ctx->playing = 0;
		ctx->floatA = (__m128*)(ctx + 1);
		ctx->floatA = (__m128*)TS_ALIGN( ctx->floatA, 16 );
		TS_ASSERT( !((size_t)ctx->floatA & 15) );
		ctx->floatB = ctx->floatA + wide_count;
		ctx->samples = (__m128i*)ctx->floatB + wide_count;
		ctx->running = 1;
		ctx->separate_thread = 0;
		ctx->sleep_milliseconds = 0;

		if ( playing_pool_count )
		{
			ctx->playing_pool = (tsPlayingSound*)(ctx->samples + wide_count);
			for ( int i = 0; i < playing_pool_count - 1; ++i )
				ctx->playing_pool[ i ].next = ctx->playing_pool + i + 1;
			ctx->playing_pool[ playing_pool_count - 1 ].next = 0;
			ctx->playing_free = ctx->playing_pool;
		}

		else
		{
			ctx->playing_pool = 0;
			ctx->playing_free = 0;
		}

		return ctx;
	}

	void tsSpawnMixThread( tsContext* ctx )
	{
		if ( ctx->separate_thread ) return;
		InitializeCriticalSectionAndSpinCount( &ctx->critical_section, 0x00000400 );
		ctx->separate_thread = 1;
		CreateThread( 0, 0, tsCtxThread, ctx, 0, 0 );
	}

#elif TS_PLATFORM == TS_MAC

	void tsSleep( int milliseconds )
	{
		usleep( milliseconds * 1000 );
	}

	struct tsContext
	{
		unsigned latency_samples;
		unsigned index0; // read
		unsigned index1; // write
		int Hz;
		int bps;
		int wide_count;
		int sample_count;
		tsPlayingSound* playing;
		__m128* floatA;
		__m128* floatB;
		__m128i* samples;
		tsPlayingSound* playing_pool;
		tsPlayingSound* playing_free;

		// platform specific stuff
		AudioComponentInstance inst;

		// data for tsMix thread, enable these with tsSpawnMixThread
		pthread_t thread;
		pthread_mutex_t mutex;
		int separate_thread;
		int running;
		int sleep_milliseconds;
	};

	static void tsReleaseContext( tsContext* ctx )
	{
		if ( ctx->separate_thread )	pthread_mutex_destroy( &ctx->mutex );
		AudioOutputUnitStop( ctx->inst );
		AudioUnitUninitialize( ctx->inst );
		AudioComponentInstanceDispose( ctx->inst );
		tsPlayingSound* playing = ctx->playing;
		while ( playing )
		{
			tsRemoveFilter( playing );
			playing = playing->next;
		}
		free( ctx );
	}

	static int tsSamplesWritten( tsContext* ctx )
	{
		int index0 = ctx->index0;
		int index1 = ctx->index1;
		if ( index0 <= index1 ) return index1 - index0;
		else return ctx->sample_count - index0 + index1;
	}

	static int tsSamplesUnwritten( tsContext* ctx )
	{
		int index0 = ctx->index0;
		int index1 = ctx->index1;
		if ( index0 <= index1 ) return ctx->sample_count - index1 + index0;
		else return index0 - index1;
	}

	static int tsSamplesToMix( tsContext* ctx )
	{
		int lat = ctx->latency_samples;
		int written = tsSamplesWritten( ctx );
		int dif = lat - written;
		TS_ASSERT( dif >= 0 );
		if ( dif )
		{
			int unwritten = tsSamplesUnwritten( ctx );
			return dif < unwritten ? dif : unwritten;
		}
		return 0;
	}

#define TS_SAMPLES_TO_BYTES( interleaved_sample_count ) ((interleaved_sample_count) * ctx->bps)
#define TS_BYTES_TO_SAMPLES( byte_count ) ((byte_count) / ctx->bps)

	static void tsPushBytes( tsContext* ctx, void* data, int size )
	{
		int index0 = ctx->index0;
		int index1 = ctx->index1;
		int samples = TS_BYTES_TO_SAMPLES( size );
		int sample_count = ctx->sample_count;

		int unwritten = tsSamplesUnwritten( ctx );
		if ( unwritten < samples ) samples = unwritten;
		int can_overflow = index0 <= index1;
		int would_overflow = index1 + samples > sample_count;

		if ( can_overflow && would_overflow )
		{
			int first_size = TS_SAMPLES_TO_BYTES( sample_count - index1 );
			int second_size = size - first_size;
			memcpy( (char*)ctx->samples + TS_SAMPLES_TO_BYTES( index1 ), data, first_size );
			memcpy( ctx->samples, (char*)data + first_size, second_size );
			ctx->index1 = TS_BYTES_TO_SAMPLES( second_size );
		}

		else
		{
			memcpy( (char*)ctx->samples + TS_SAMPLES_TO_BYTES( index1 ), data, size );
			ctx->index1 += TS_BYTES_TO_SAMPLES( size );
		}
	}

	static int tsPullBytes( tsContext* ctx, void* dst, int size )
	{
		int index0 = ctx->index0;
		int index1 = ctx->index1;
		int allowed_size = TS_SAMPLES_TO_BYTES( tsSamplesWritten( ctx ) );
		int zeros = 0;

		if ( allowed_size < size )
		{
			zeros = size - allowed_size;
			size = allowed_size;
		}

		if ( index1 >= index0 )
		{
			memcpy( dst, ((char*)ctx->samples) + TS_SAMPLES_TO_BYTES( index0 ), size );
			ctx->index0 += TS_BYTES_TO_SAMPLES( size );
		}

		else
		{
			int first_size = TS_SAMPLES_TO_BYTES( ctx->sample_count ) - TS_SAMPLES_TO_BYTES( index0 );
			if ( first_size > size ) first_size = size;
			int second_size = size - first_size;
			memcpy( dst, ((char*)ctx->samples) + TS_SAMPLES_TO_BYTES( index0 ), first_size );
			memcpy( ((char*)dst) + first_size, ctx->samples, second_size );
			if ( second_size ) ctx->index0 = TS_BYTES_TO_SAMPLES( second_size );
			else ctx->index0 += TS_BYTES_TO_SAMPLES( first_size );
		}

		return zeros;
	}

	static void* tsCtxThread( void* udata )
	{
		tsContext* ctx = (tsContext*)udata;

		while ( ctx->running )
		{
			tsMix( ctx );
			if ( ctx->sleep_milliseconds ) tsSleep( ctx->sleep_milliseconds );
			else pthread_yield_np( );
		}

		ctx->separate_thread = 0;
		pthread_exit( 0 );
		return 0;
	}

	static void tsLock( tsContext* ctx )
	{
		if ( ctx->separate_thread ) pthread_mutex_lock( &ctx->mutex );
	}

	static void tsUnlock( tsContext* ctx )
	{
		if ( ctx->separate_thread ) pthread_mutex_unlock( &ctx->mutex );
	}

	static OSStatus tsMemcpyToCA( void* udata, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData );

	tsContext* tsMakeContext( void* unused, unsigned play_frequency_in_Hz, int latency_factor_in_Hz, int num_buffered_seconds, int playing_pool_count )
	{
		int bps = sizeof( uint16_t ) * 2;

		AudioComponentDescription comp_desc = { 0 };
		comp_desc.componentType = kAudioUnitType_Output;
		comp_desc.componentSubType = kAudioUnitSubType_DefaultOutput;
		comp_desc.componentFlags = 0;
		comp_desc.componentFlagsMask = 0;
		comp_desc.componentManufacturer = kAudioUnitManufacturer_Apple;

		AudioComponent comp = AudioComponentFindNext( NULL, &comp_desc );
		if ( !comp )
		{
			g_tsErrorReason = "Failed to create output unit from AudioComponentFindNext.";
			return 0;
		}

		AudioStreamBasicDescription stream_desc = { 0 };
		stream_desc.mSampleRate = (double)play_frequency_in_Hz;
		stream_desc.mFormatID = kAudioFormatLinearPCM;
		stream_desc.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagsNativeEndian | kAudioFormatFlagIsPacked;
		stream_desc.mFramesPerPacket = 1;
		stream_desc.mChannelsPerFrame = 2;
		stream_desc.mBitsPerChannel = sizeof( uint16_t ) * 8;
		stream_desc.mBytesPerPacket = bps;
		stream_desc.mBytesPerFrame = bps;
		stream_desc.mReserved = 0;

		AudioComponentInstance inst;
		OSStatus ret;
		AURenderCallbackStruct input;

		ret = AudioComponentInstanceNew( comp, &inst );

		int sample_count = play_frequency_in_Hz * num_buffered_seconds;
		int latency_count = (unsigned)TS_ALIGN( play_frequency_in_Hz / latency_factor_in_Hz, 4 );
		TS_ASSERT( sample_count > latency_count );
		int wide_count = (int)TS_ALIGN( sample_count, 4 ) / 4;
		int pool_size = playing_pool_count * sizeof( tsPlayingSound );
		int mix_buffers_size = sizeof( __m128 ) * wide_count * 2;
		int sample_buffer_size = sizeof( __m128i ) * wide_count;
		tsContext* ctx = (tsContext*)malloc( sizeof( tsContext ) + mix_buffers_size + sample_buffer_size + 16 + pool_size );
		TS_CHECK( ret == noErr, "AudioComponentInstanceNew failed" );
		ctx->latency_samples = latency_count;
		ctx->index0 = 0;
		ctx->index1 = 0;
		ctx->Hz = play_frequency_in_Hz;
		ctx->bps = bps;
		ctx->wide_count = wide_count;
		ctx->sample_count = wide_count * 4;
		ctx->inst = inst;
		ctx->playing = 0;
		ctx->floatA = (__m128*)(ctx + 1);
		ctx->floatA = (__m128*)TS_ALIGN( ctx->floatA, 16 );
		TS_ASSERT( !((size_t)ctx->floatA & 15) );
		ctx->floatB = ctx->floatA + wide_count;
		ctx->samples = (__m128i*)ctx->floatB + wide_count;
		ctx->running = 1;
		ctx->separate_thread = 0;
		ctx->sleep_milliseconds = 0;

		ret = AudioUnitSetProperty( inst, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &stream_desc, sizeof( stream_desc ) );
		TS_CHECK( ret == noErr, "Failed to set stream forat" );

		input.inputProc = tsMemcpyToCA;
		input.inputProcRefCon = ctx;
		ret = AudioUnitSetProperty( inst, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &input, sizeof( input ) );
		TS_CHECK( ret == noErr, "AudioUnitSetProperty failed" );

		ret = AudioUnitInitialize( inst );
		TS_CHECK( ret == noErr, "Couldn't initialize output unit" );

		ret = AudioOutputUnitStart( inst );
		TS_CHECK( ret == noErr, "Couldn't start output unit" );

		if ( playing_pool_count )
		{
			ctx->playing_pool = (tsPlayingSound*)(ctx->samples + wide_count);
			for ( int i = 0; i < playing_pool_count - 1; ++i )
				ctx->playing_pool[ i ].next = ctx->playing_pool + i + 1;
			ctx->playing_pool[ playing_pool_count - 1 ].next = 0;
			ctx->playing_free = ctx->playing_pool;
		}

		else
		{
			ctx->playing_pool = 0;
			ctx->playing_free = 0;
		}

		return ctx;

	err:
		free( ctx );
		return 0;
	}

	void tsSpawnMixThread( tsContext* ctx )
	{
		if ( ctx->separate_thread ) return;
		pthread_mutex_init( &ctx->mutex, 0 );
		ctx->separate_thread = 1;
		pthread_create( &ctx->thread, 0, tsCtxThread, ctx );
	}

#else
#endif

void tsShutdownContext( tsContext* ctx )
{
	if ( ctx->separate_thread )
	{
		tsLock( ctx );
		ctx->running = 0;
		tsUnlock( ctx );
	}

	while ( ctx->separate_thread ) tsSleep( 1 );
	tsReleaseContext( ctx );
}

void tsThreadSleepDelay( tsContext* ctx, int milliseconds )
{
	ctx->sleep_milliseconds = milliseconds;
}

void tsInsertSound( tsContext* ctx, tsPlayingSound* sound )
{
	// Cannot use tsPlayingSound if tsMakeContext was passed non-zero for playing_pool_count
	// since non-zero playing_pool_count means the context is doing some memory-management
	// for a playing sound pool. InsertSound assumes the pool does not exist, and is apart
	// of the lower-level API (see top of this header for documentation details).
	TS_ASSERT( ctx->playing_pool == 0 );

	if ( sound->active ) return;
	tsLock( ctx );
	sound->next = ctx->playing;
	ctx->playing = sound;
	sound->active = 1;
	tsUnlock( ctx );
}

// NOTE: does not allow delay_in_seconds to be negative (clamps at 0)
void tsSetDelay( tsContext* ctx, tsPlayingSound* sound, float delay_in_seconds )
{
	if ( delay_in_seconds < 0.0f ) delay_in_seconds = 0.0f;
	sound->sample_index = (int)(delay_in_seconds * (float)ctx->Hz);
	sound->sample_index = -(int)TS_ALIGN( sound->sample_index, 4 );
}

tsPlaySoundDef tsMakeDef( tsLoadedSound* sound )
{
	tsPlaySoundDef def;
	def.paused = 0;
	def.looped = 0;
	def.volume_left = 1.0f;
	def.volume_right = 1.0f;
	def.pan = 0.5f;
	def.pitch = 1.0f;
	def.delay = 0.0f;
	def.loaded = sound;
	return def;
}

tsPlayingSound* tsPlaySound( tsContext* ctx, tsPlaySoundDef def )
{
	tsLock( ctx );

	tsPlayingSound* playing = ctx->playing_free;
	if ( !playing ) return 0;
	ctx->playing_free = playing->next;
	*playing = tsMakePlayingSound( def.loaded );
	playing->active = 1;
	playing->paused = def.paused;
	playing->looped = def.looped;
	tsSetVolume( playing, def.volume_left, def.volume_right );
	tsSetPan( playing, def.pan );
	tsSetPitch( playing, def.pitch );
	tsSetDelay( ctx, playing, def.delay );
	playing->next = ctx->playing;
	ctx->playing = playing;

	tsUnlock( ctx );

	return playing;
}

void tsStopAllSounds( tsContext* ctx )
{
	// This is apart of the high level API, not the low level API.
	// If using the low level API you must write your own function to
	// stop playing all sounds.
	TS_ASSERT( ctx->playing_pool == 0 );

	tsPlayingSound* sound = ctx->playing;
	ctx->playing = 0;

	while ( sound )
	{
		tsPlayingSound* next = sound->next;
		sound->next = ctx->playing_free;
		ctx->playing_free = sound;
		sound = next;
	}
}

#if TS_PLATFORM == TS_WINDOWS

	static void tsPosition( tsContext* ctx, int* byte_to_lock, int* bytes_to_write )
	{
		// compute bytes to be written to direct sound
		DWORD play_cursor;
		DWORD write_cursor;
		HRESULT hr = ctx->buffer->lpVtbl->GetCurrentPosition( ctx->buffer, &play_cursor, &write_cursor );
		TS_ASSERT( hr == DS_OK );

		DWORD lock = (ctx->running_index * ctx->bps) % ctx->buffer_size;
		DWORD target_cursor = (write_cursor + ctx->latency_samples * ctx->bps) % ctx->buffer_size;
		target_cursor = (DWORD)TS_ALIGN( target_cursor, 16 );
		DWORD write;

		if ( lock > target_cursor )
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

	static void tsMemcpyToDS( tsContext* ctx, int16_t* samples, int byte_to_lock, int bytes_to_write )
	{
		// copy mixer buffers to direct sound
		void* region1;
		DWORD size1;
		void* region2;
		DWORD size2;
		HRESULT hr = ctx->buffer->lpVtbl->Lock( ctx->buffer, byte_to_lock, bytes_to_write, &region1, &size1, &region2, &size2, 0 );

		if ( hr == DSERR_BUFFERLOST )
		{
			ctx->buffer->lpVtbl->Restore( ctx->buffer );
			hr = ctx->buffer->lpVtbl->Lock( ctx->buffer, byte_to_lock, bytes_to_write, &region1, &size1, &region2, &size2, 0 );
		}

		if ( !SUCCEEDED( hr ) )
			return;

		unsigned running_index = ctx->running_index;
		INT16* sample1 = (INT16*)region1;
		DWORD sample1_count = size1 / ctx->bps;
		memcpy( sample1, samples, sample1_count * sizeof( INT16 ) * 2 );
		samples += sample1_count * 2;
		running_index += sample1_count;

		INT16* sample2 = (INT16*)region2;
		DWORD sample2_count = size2 / ctx->bps;
		memcpy( sample2, samples, sample2_count * sizeof( INT16 ) * 2 );
		samples += sample2_count * 2;
		running_index += sample2_count;

		ctx->buffer->lpVtbl->Unlock( ctx->buffer, region1, size1, region2, size2 );
		ctx->running_index = running_index;

		// meager hack to fill out sound buffer before playing
		static int first;
		if ( !first )
		{
			ctx->buffer->lpVtbl->Play( ctx->buffer, 0, 0, DSBPLAY_LOOPING );
			first = 1;
		}
	}

#elif TS_PLATFORM == TS_MAC

	static OSStatus tsMemcpyToCA( void* udata, AudioUnitRenderActionFlags* ioActionFlags, const AudioTimeStamp* inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList* ioData )
	{
		tsContext* ctx = (tsContext*)udata;
		int bps = ctx->bps;
		int samples_requested_to_consume = inNumberFrames;
		AudioBuffer* buffer = ioData->mBuffers;

		TS_ASSERT( ioData->mNumberBuffers == 1 );
		TS_ASSERT( buffer->mNumberChannels == 2 );
		int byte_size = buffer->mDataByteSize;
		TS_ASSERT( byte_size == samples_requested_to_consume * bps );

		int zero_bytes = tsPullBytes( ctx, buffer->mData, byte_size );
		memset( ((char*)buffer->mData) + (byte_size - zero_bytes), 0, zero_bytes );

		return noErr;
	}

#else
#endif

static void smbPitchShift( float pitchShift, long numSampsToProcess, float sampleRate, float* indata, tsPitchShift** pitch_filter );

// Pitch processing
#define TS_PI                  3.14159265358979323846
#define TS_MAX_FRAME_LENGTH    8192
#define TS_PITCH_FRAME_SIZE    512
#define TS_PITCH_QUALITY       4

// the g members of tsPitchShift contain intermediary data for the
// smbPitchShift function. This struct is only used if tsSetPitch is
// called with a parameter other than 1.0f.
typedef struct tsPitchShift
{
	float* outdata;
	float gInFIFO[ TS_MAX_FRAME_LENGTH ];
	float gOutFIFO[ TS_MAX_FRAME_LENGTH ];
	float gFFTworksp[ 2 * TS_MAX_FRAME_LENGTH ];
	float gLastPhase[ TS_MAX_FRAME_LENGTH / 2 + 1 ];
	float gSumPhase[ TS_MAX_FRAME_LENGTH / 2 + 1 ];
	float gOutputAccum[ 2 * TS_MAX_FRAME_LENGTH ];
	float gAnaFreq[ TS_MAX_FRAME_LENGTH ];
	float gAnaMagn[ TS_MAX_FRAME_LENGTH ];
	float gSynFreq[ TS_MAX_FRAME_LENGTH ];
	float gSynMagn[ TS_MAX_FRAME_LENGTH ];
	long  gRover;
} tsPitchShift;

static void tsRemoveFilter( tsPlayingSound* playing )
{
	for ( int i = 0; i < 2; i++ )
	{
		if ( playing->pitch_filter[ i ] )
		{
			free16( playing->pitch_filter[ i ]->outdata );
			free( playing->pitch_filter[ i ] );
			playing->pitch_filter[ i ] = 0;
		}
	}
}

void tsMix( tsContext* ctx )
{
	tsLock( ctx );

#if TS_PLATFORM == TS_WINDOWS

	int byte_to_lock;
	int bytes_to_write;
	tsPosition( ctx, &byte_to_lock, &bytes_to_write );

	if ( !bytes_to_write ) goto unlock;
	int samples_to_write = bytes_to_write / ctx->bps;

#elif TS_PLATFORM == TS_MAC

	int samples_to_write = tsSamplesToMix( ctx );
	if ( !samples_to_write ) goto unlock;
	int bytes_to_write = samples_to_write * ctx->bps;

#else
#endif

	// clear mixer buffers
	int wide_count = samples_to_write / 4;
	TS_ASSERT( !(samples_to_write & 3) );

	__m128* floatA = ctx->floatA;
	__m128* floatB = ctx->floatB;
	__m128 zero = _mm_set1_ps( 0.0f );

	for ( int i = 0; i < wide_count; ++i )
	{
		floatA[ i ] = zero;
		floatB[ i ] = zero;
	}

	// mix all playing sounds into the mixer buffers
	tsPlayingSound** ptr = &ctx->playing;
	while ( *ptr )
	{
		tsPlayingSound* playing = *ptr;
		tsLoadedSound* loaded = playing->loaded_sound;
		__m128* cA = (__m128*)loaded->channels[ 0 ];
		__m128* cB = (__m128*)loaded->channels[ 1 ];
		TS_ASSERT( cA ); // attempted to play a sound with no audio

		int mix_count = samples_to_write;
		int offset = playing->sample_index;
		int remaining = loaded->sample_count - offset;
		if ( remaining < mix_count ) mix_count = remaining;
		TS_ASSERT( remaining > 0 );

		float vA0 = playing->volume0 * playing->pan0;
		float vB0 = playing->volume1 * playing->pan1;
		__m128 vA = _mm_set1_ps( vA0 );
		__m128 vB = _mm_set1_ps( vB0 );

		// skip sound if it's delay is longer than mix_count and
		// handle various delay cases
		int delay_offset = 0;
		if ( offset < 0 )
		{
			int samples_till_positive = -offset;
			int mix_leftover = mix_count - samples_till_positive;

			if ( mix_leftover <= 0 )
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
		TS_ASSERT( !(delay_offset & 3) );

		// immediately remove any inactive elements
		if ( !playing->active || !ctx->running )
			goto remove;

		// skip all paused sounds
		if ( playing->paused )
			goto get_next_playing_sound;

		// SIMD offets
		int mix_wide = (int)TS_ALIGN( mix_count, 4 ) / 4;
		int offset_wide = (int)TS_TRUNC( offset, 4 ) / 4;
		int delay_wide = (int)TS_ALIGN( delay_offset, 4 ) / 4;

		// use smbPitchShift to on-the-fly pitch shift some samples
		// only call this function if the user set a custom pitch value
		if ( playing->pitch != 1.0f )
		{
			int sample_count = (mix_wide - 2 * delay_wide) * 4;
			int falling_behind = sample_count > TS_MAX_FRAME_LENGTH;

			// TS_MAX_FRAME_LENGTH represents max samples we can pitch shift in one go,
			// it should be large enough to never fall behind, but we need this if just in case...
			if ( !falling_behind )
			{
				smbPitchShift( playing->pitch, sample_count, (float)ctx->Hz, (float*)(cA + delay_wide + offset_wide), playing->pitch_filter );
				cA = (__m128 *)playing->pitch_filter[ 0 ]->outdata;

				if ( loaded->channel_count == 2 )
				{
					smbPitchShift( playing->pitch, sample_count, (float)ctx->Hz, (float*)(cB + delay_wide + offset_wide), playing->pitch_filter + 1 );
					cB = (__m128 *)playing->pitch_filter[ 1 ]->outdata;
				}

				offset_wide = -delay_wide;
			}
		}

		// apply volume, load samples into float buffers
		switch ( loaded->channel_count )
		{
		case 1:
			for ( int i = delay_wide; i < mix_wide - delay_wide; ++i )
			{
				__m128 A = cA[ i + offset_wide ];
				__m128 B = _mm_mul_ps( A, vB );
				A = _mm_mul_ps( A, vA );
				floatA[ i ] = _mm_add_ps( floatA[ i ], A );
				floatB[ i ] = _mm_add_ps( floatB[ i ], B );
			}
			break;

		case 2:
		{
			for ( int i = delay_wide; i < mix_wide - delay_wide; ++i )
			{
				__m128 A = cA[ i + offset_wide ];
				__m128 B = cB[ i + offset_wide ];

				A = _mm_mul_ps( A, vA );
				B = _mm_mul_ps( B, vB );
				floatA[ i ] = _mm_add_ps( floatA[ i ], A );
				floatB[ i ] = _mm_add_ps( floatB[ i ], B );
			}
		}	break;
		}

		// playing list logic
		playing->sample_index += mix_count;
		if ( playing->sample_index == loaded->sample_count )
		{
			if ( playing->looped )
			{
				playing->sample_index = 0;
				goto get_next_playing_sound;
			}

			remove:
			playing->sample_index = 0;
			*ptr = (*ptr)->next;
			playing->next = 0;
			playing->active = 0;

			tsRemoveFilter( playing );

			// if using high-level API manage the tsPlayingSound memory ourselves
			if ( ctx->playing_pool )
			{
				playing->next = ctx->playing_free;
				ctx->playing_free = playing;
			}

			// we already incremented next pointer, so don't do it again
			continue;
		}

	get_next_playing_sound:
		if ( *ptr ) ptr = &(*ptr)->next;
		else break;
	}

	// load all floats into 16 bit packed interleaved samples
#if TS_PLATFORM == TS_WINDOWS

	__m128i* samples = ctx->samples;
	for ( int i = 0; i < wide_count; ++i )
	{
		__m128i a = _mm_cvtps_epi32( floatA[ i ] );
		__m128i b = _mm_cvtps_epi32( floatB[ i ] );
		__m128i a0b0a1b1 = _mm_unpacklo_epi32( a, b );
		__m128i a2b2a3b3 = _mm_unpackhi_epi32( a, b );
		samples[ i ] = _mm_packs_epi32( a0b0a1b1, a2b2a3b3 );
	}
	tsMemcpyToDS( ctx, (int16_t*)samples, byte_to_lock, bytes_to_write );

#elif TS_PLATFORM == TS_MAC

	// Since the ctx->samples array is already in use as a ring buffer
	// reusing floatA to store output is a good way to temporarly store
	// the final samples. Then a single ring buffer push can be used
	// afterwards. Pretty hacky, but whatever :)
	__m128i* samples = (__m128i*)floatA;
	memset( samples, 0, sizeof( __m128i ) * wide_count );
	for ( int i = 0; i < wide_count; ++i )
	{
		__m128i a = _mm_cvtps_epi32( floatA[ i ] );
		__m128i b = _mm_cvtps_epi32( floatB[ i ] );
		__m128i a0b0a1b1 = _mm_unpacklo_epi32( a, b );
		__m128i a2b2a3b3 = _mm_unpackhi_epi32( a, b );
		samples[ i ] = _mm_packs_epi32( a0b0a1b1, a2b2a3b3 );
	}
	tsPushBytes( ctx, samples, bytes_to_write );

#else
#endif

unlock:
	tsUnlock( ctx );
}

#endif

/*
	zlib license:

	Copyright (c) 2016 Randy Gaul http://www.randygaul.net

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
*/

// NOTE:
// See FAQ and see docs for tsPitchShift for more information on the below code.
// Feel free to remove the below code in order to get rid of the WOL license,
// if desired.

/****************************************************************************
*
* NAME: smbPitchShift.cpp
* VERSION: 1.2
* HOME URL: http://blogs.zynaptiq.com/bernsee
* KNOWN BUGS: none
*
* SYNOPSIS: Routine for doing pitch shifting while maintaining
* duration using the Short Time Fourier Transform.
*
* DESCRIPTION: The routine takes a pitchShift factor value which is between 0.5
* (one octave down) and 2. (one octave up). A value of exactly 1 does not change
* the pitch. numSampsToProcess tells the routine how many samples in indata[0...
* numSampsToProcess-1] should be pitch shifted and moved to outdata[0 ...
* numSampsToProcess-1]. The two buffers can be identical (ie. it can process the
* data in-place). fftFrameSize defines the FFT frame size used for the
* processing. Typical values are 1024, 2048 and 4096. It may be any value <=
* TS_MAX_FRAME_LENGTH but it MUST be a power of 2. osamp is the STFT
* oversampling factor which also determines the overlap between adjacent STFT
* frames. It should at least be 4 for moderate scaling ratios. A value of 32 is
* recommended for best quality. sampleRate takes the sample rate for the signal
* in unit Hz, ie. 44100 for 44.1 kHz audio. The data passed to the routine in
* indata[] should be in the range [-1.0, 1.0), which is also the output range
* for the data, make sure you scale the data accordingly (for 16bit signed integers
* you would have to divide (and multiply) by 32768).
*
* COPYRIGHT 1999-2015 Stephan M. Bernsee <s.bernsee [AT] zynaptiq [DOT] com>
*
* 						The Wide Open License (WOL)
*
* Permission to use, copy, modify, distribute and sell this software and its
* documentation for any purpose is hereby granted without fee, provided that
* the above copyright notice and this license appear in all source copies.
* THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF
* ANY KIND. See http://www.dspguru.com/wol.htm for more information.
*
*****************************************************************************/

#include <math.h>

static double smbAtan2(double x, double y)
{
	double signx;
	if (x > 0.) signx = 1.;
	else signx = -1.;

	if (x == 0.) return 0.;
	if (y == 0.) return signx * TS_PI / 2.;

	return atan2(x, y);
}

static void smbFft(float *fftBuffer, long fftFrameSize, long sign)
/*
	FFT routine, (C)1996 S.M.Bernsee. Sign = -1 is FFT, 1 is iFFT (inverse)
	Fills fftBuffer[0...2*fftFrameSize-1] with the Fourier transform of the
	time domain data in fftBuffer[0...2*fftFrameSize-1]. The FFT array takes
	and returns the cosine and sine parts in an interleaved manner, ie.
	fftBuffer[0] = cosPart[0], fftBuffer[1] = sinPart[0], asf. fftFrameSize
	must be a power of 2. It expects a complex input signal (see footnote 2),
	ie. when working with 'common' audio signals our input signal has to be
	passed as {in[0],0.,in[1],0.,in[2],0.,...} asf. In that case, the transform
	of the frequencies of interest is in fftBuffer[0...fftFrameSize].
*/
{
	float wr, wi, arg, *p1, *p2, temp;
	float tr, ti, ur, ui, *p1r, *p1i, *p2r, *p2i;
	long i, bitm, j, le, le2, k;

	for (i = 2; i < 2*fftFrameSize-2; i += 2) {
		for (bitm = 2, j = 0; bitm < 2*fftFrameSize; bitm <<= 1) {
			if (i & bitm) j++;
			j <<= 1;
		}
		if (i < j) {
			p1 = fftBuffer+i; p2 = fftBuffer+j;
			temp = *p1; *(p1++) = *p2;
			*(p2++) = temp; temp = *p1;
			*p1 = *p2; *p2 = temp;
		}
	}
	for (k = 0, le = 2; k < (long)(log(fftFrameSize)/log(2.)+.5); k++) {
		le <<= 1;
		le2 = le>>1;
		ur = 1.0;
		ui = 0.0;
		arg = (float)(TS_PI / (le2>>1));
		wr = (float)cos(arg);
		wi = (float)(sign*sin(arg));
		for (j = 0; j < le2; j += 2) {
			p1r = fftBuffer+j; p1i = p1r+1;
			p2r = p1r+le2; p2i = p2r+1;
			for (i = j; i < 2*fftFrameSize; i += le) {
				tr = *p2r * ur - *p2i * ui;
				ti = *p2r * ui + *p2i * ur;
				*p2r = *p1r - tr; *p2i = *p1i - ti;
				*p1r += tr; *p1i += ti;
				p1r += le; p1i += le;
				p2r += le; p2i += le;
			}
			tr = ur*wr - ui*wi;
			ui = ur*wi + ui*wr;
			ur = tr;
		}
	}
}

/*
	Purpose: doing pitch shifting while maintaining duration using the Short Time Fourier Transform.
	Author: (c)1999-2015 Stephan M. Bernsee <s.bernsee [AT] zynaptiq [DOT] com>
*/
static void smbPitchShift(float pitchShift, long numSampsToProcess, float sampleRate, float* indata, tsPitchShift** pitch_filter)
{
	tsPitchShift* pf;

	if (*pitch_filter == NULL)
	{
		pf = (tsPitchShift*)malloc(sizeof(tsPitchShift));
		pf->outdata = malloc16(TS_MAX_FRAME_LENGTH*sizeof(float));
		pf->gRover = 0;
		memset(pf->gInFIFO, 0, TS_MAX_FRAME_LENGTH*sizeof(float));
		memset(pf->gOutFIFO, 0, TS_MAX_FRAME_LENGTH*sizeof(float));
		memset(pf->gFFTworksp, 0, 2*TS_MAX_FRAME_LENGTH*sizeof(float));
		memset(pf->gLastPhase, 0, (TS_MAX_FRAME_LENGTH/2+1)*sizeof(float));
		memset(pf->gSumPhase, 0, (TS_MAX_FRAME_LENGTH/2+1)*sizeof(float));
		memset(pf->gOutputAccum, 0, 2*TS_MAX_FRAME_LENGTH*sizeof(float));
		memset(pf->gAnaFreq, 0, TS_MAX_FRAME_LENGTH*sizeof(float));
		memset(pf->gAnaMagn, 0, TS_MAX_FRAME_LENGTH*sizeof(float));
		*pitch_filter = pf;
	}
	else
	{
		pf = *pitch_filter;
	}

	double magn, phase, tmp, window, real, imag;
	long i, k, qpd, index;

	long fftFrameSize = TS_PITCH_FRAME_SIZE;
	long osamp = TS_PITCH_QUALITY;

		/* set up some handy variables */
	long fftFrameSize2 = fftFrameSize / 2;
	long stepSize = fftFrameSize / osamp;
	double expct = 2.*TS_PI*(double)stepSize / (double)fftFrameSize;
	double freqPerBin = sampleRate / (double)fftFrameSize;
	long inFifoLatency = fftFrameSize - stepSize;

	if (pf->gRover == 0) pf->gRover = inFifoLatency;

	/* main processing loop */
	for (i = 0; i < numSampsToProcess; i++){

		/* As long as we have not yet collected enough data just read in */
		pf->gInFIFO[pf->gRover] = indata[i] / 32768.0f;
		pf->outdata[i] = pf->gOutFIFO[pf->gRover-inFifoLatency] * 32768.0f;
		pf->gRover++;

		/* now we have enough data for processing */
		if (pf->gRover >= fftFrameSize) {
			pf->gRover = inFifoLatency;

			/* do windowing and re,im interleave */
			for (k = 0; k < fftFrameSize;k++) {
				window = -.5*cos(2.*TS_PI*(double)k/(double)fftFrameSize)+.5;
				pf->gFFTworksp[2*k] = (float)(pf->gInFIFO[k] * window);
				pf->gFFTworksp[2*k+1] = 0.;
			}


			/* ***************** ANALYSIS ******************* */
			/* do transform */
			smbFft(pf->gFFTworksp, fftFrameSize, -1);

			/* this is the analysis step */
			for (k = 0; k <= fftFrameSize2; k++) {

				/* de-interlace FFT buffer */
				real = pf->gFFTworksp[2*k];
				imag = pf->gFFTworksp[2*k+1];

				/* compute magnitude and phase */
				magn = 2.*sqrt(real*real + imag*imag);
				phase = smbAtan2(imag,real);

				/* compute phase difference */
				tmp = phase - pf->gLastPhase[k];
				pf->gLastPhase[k] = (float)phase;

				/* subtract expected phase difference */
				tmp -= (double)k*expct;

				/* map delta phase into +/- Pi interval */
				qpd = (long)(tmp/TS_PI);
				if (qpd >= 0) qpd += qpd&1;
				else qpd -= qpd&1;
				tmp -= TS_PI*(double)qpd;

				/* get deviation from bin frequency from the +/- Pi interval */
				tmp = osamp*tmp/(2.*TS_PI);

				/* compute the k-th partials' true frequency */
				tmp = (double)k*freqPerBin + tmp*freqPerBin;

				/* store magnitude and true frequency in analysis arrays */
				pf->gAnaMagn[k] = (float)magn;
				pf->gAnaFreq[k] = (float)tmp;

			}

			/* ***************** PROCESSING ******************* */
			/* this does the actual pitch shifting */
			memset(pf->gSynMagn, 0, fftFrameSize*sizeof(float));
			memset(pf->gSynFreq, 0, fftFrameSize*sizeof(float));
			for (k = 0; k <= fftFrameSize2; k++) {
				index = (long)(k*pitchShift);
				if (index <= fftFrameSize2) {
					pf->gSynMagn[index] += pf->gAnaMagn[k];
					pf->gSynFreq[index] = pf->gAnaFreq[k] * pitchShift;
				}
			}

			/* ***************** SYNTHESIS ******************* */
			/* this is the synthesis step */
			for (k = 0; k <= fftFrameSize2; k++) {

				/* get magnitude and true frequency from synthesis arrays */
				magn = pf->gSynMagn[k];
				tmp = pf->gSynFreq[k];

				/* subtract bin mid frequency */
				tmp -= (double)k*freqPerBin;

				/* get bin deviation from freq deviation */
				tmp /= freqPerBin;

				/* take osamp into account */
				tmp = 2.*TS_PI*tmp/osamp;

				/* add the overlap phase advance back in */
				tmp += (double)k*expct;

				/* accumulate delta phase to get bin phase */
				pf->gSumPhase[k] += (float)tmp;
				phase = pf->gSumPhase[k];

				/* get real and imag part and re-interleave */
				pf->gFFTworksp[2*k] = (float)(magn*cos(phase));
				pf->gFFTworksp[2*k+1] = (float)(magn*sin(phase));
			}

			/* zero negative frequencies */
			for (k = fftFrameSize+2; k < 2*fftFrameSize; k++) pf->gFFTworksp[k] = 0.;

			/* do inverse transform */
			smbFft(pf->gFFTworksp, fftFrameSize, 1);

			/* do windowing and add to output accumulator */
			for(k=0; k < fftFrameSize; k++) {
				window = -.5*cos(2.*TS_PI*(double)k/(double)fftFrameSize)+.5;
				pf->gOutputAccum[k] += (float)(2.*window*pf->gFFTworksp[2*k]/(fftFrameSize2*osamp));
			}
			for (k = 0; k < stepSize; k++) pf->gOutFIFO[k] = pf->gOutputAccum[k];

			/* shift accumulator */
			memmove(pf->gOutputAccum, pf->gOutputAccum+stepSize, fftFrameSize*sizeof(float));

			/* move input FIFO */
			for (k = 0; k < inFifoLatency; k++) pf->gInFIFO[k] = pf->gInFIFO[k+stepSize];
		}
	}
}
