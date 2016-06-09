/*
	tinysound.h - v1.02
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
	The tsContext encapsulates Direct Sound, as well as buffers + settings.
	tsLoadedSound holds raw samples of a sound. tsPlayingSound is an instance
	of a tsLoadedSound that represents a sound that can be played through the
	tsContext.

	There are two main versions of the API, the low-level and the high-level
	API. The low-level API does not manage any memory for tsPlayingSounds.

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
			hwnd           --  HWND, handle to window
			frequency      --  int, represents Hz frequency rate in which samples are played
			latency        --  int, estimated latency in Hz from PlaySound call to speaker output
			seconds        --  int, number of second of samples internal buffers can hold
			0 (last param) --  int, number of elements in tsPlayingSound pool
	
		We create a tsPlayingSound like so:
		tsLoadedSound loaded = tsLoadWAV( "path_to_file/filename.wav" );
		tsPlayingSound playing_sound = tsMakePlayingSound( &loaded );
	
		Mess around with various settings with the variou ts*** functions. Here's
		an example for setting volume and looping:
	
		tsSetLoop( &playing_sound, 1 );
		tsSetPan( &playing_sound, 0.2f );
	
		Then to play the sound we do:
		tsInsertSound( ctx, &playing_sound );
	
		The above tsInsertSound function call will place playing_sound into
		a singly-linked list inside the context. The context will remove
		the sound from its internal list when it finishes playing.

	WARNING: The high-level API cannot be mixed with the low-level API. If you
	try then the internal code will assert and crash. Pick one and stick with it.
	Usually he high-level API will be used, but if someone is *really* picky about
	their memory usage, or wants more control, the low-level API can be used.

	Be sure to link against dsound.dll (or dsound.lib).

	Read the rest of the header for specific details on all available functions
	and struct types.
*/

/*
	Known Limitations:

	* Windows only. Since I last checked the Steam survey over 95% of users ran Windows.
		Since tinysound is for games there's just not a good reason me to personally spend
		time on other platforms.
	* PCM mono/stereo format is the only formats the LoadWAV function supports. I don't
		guarantee it will work for all kinds of wav files, but it certainly does for the common
		kind (and can be changed fairly easily if someone wanted to extend it).
	* Only supports 16 bits per sample.
	* Mixer does not do any fancy clipping. The algorithm is to convert all 16 bit samples
		to float, mix all samples, and write back to DirectSound as 16 bit integers. In
		practice this works very well and clipping is not often a big problem.
	* I'm not super familiar with good ways to avoid the DirectSound play cursor from going
		past the write cursor. To mitigate this pass in a larger number to tsMakeContext's 4rd
		parameter (buffer scale in seconds).
*/

#if !defined( TINYSOUND_H )

#include <stdint.h>

// read this in the event of LoadWAV errors
extern const char* g_tsErrorReason;

// stores a loaded sound in memory
typedef struct
{
	int sample_count;
	int channel_count;
	void* channels[ 2 ];
} tsLoadedSound;

// represents an instance of a tsLoadedSound, can be played through
// the tsContext
typedef struct tsPlayingSound
{
	int active;
	int paused;
	int looped;
	float volume0;
	float volume1;
	float pan0;
	float pan1;
	int sample_index;
	tsLoadedSound* loaded_sound;
	struct tsPlayingSound* next;
} tsPlayingSound;

// holds direct sound and other info
struct tsContext;
typedef struct tsContext tsContext;

// The returned struct will contain a null pointer in tsLoadedSound::channel[ 0 ]
// in the case of errors. Read g_tsErrorReason string for details on what happened.
// Calls tsReadMemWAV internally.
tsLoadedSound tsLoadWAV( const char* path );

// Reads a WAV file from memory. Still allocates memory for the tsLoadedSound since
// WAV format will interlace stereo, and we need separate data streams to do SIMD
// properly.
void tsReadMemWAV( void* memory, tsLoadedSound* sound );

// If stb_vorbis was included *before* tinysound go ahead and create
// some functions for dealing with OGG files.
#ifdef STB_VORBIS_INCLUDE_STB_VORBIS_H
void tsReadMemOGG( void* memory, int length, int* sample_rate, tsLoadedSound* sound );
tsLoadedSound tsLoadOGG( const char* path, int* sample_rate );
#endif

// Uses free16 (aligned free, implemented later in this file) to free up both of
// the channels stored within sound
void tsFreeSound( tsLoadedSound* sound );

// playing_pool_count -- 0 to setup low-level API, non-zero to size the internal
// memory pool for tsPlayingSound instances
tsContext* tsMakeContext( void* hwnd, unsigned play_frequency_in_Hz, int latency_factor_in_Hz, int num_buffered_seconds, int playing_pool_count );
void tsShutdownContext( tsContext* ctx );
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

// delays sound before actually playing it. Requires context to be passed in
// since there's a conversion from seconds to samples per second.
// If one were so inclined another version could be implemented like:
// void tsSetDelay( tsContext* ctx, tsPlayingSound* sound, float delay, int samples_per_second )
void tsSetDelay( tsContext* ctx, tsPlayingSound* sound, float delay_in_seconds );

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
	float delay;
	tsLoadedSound* loaded;
} tsPlaySoundDef;

tsPlayingSound* tsPlaySound( tsContext* ctx, tsPlaySoundDef def );
tsPlaySoundDef tsMakeDef( tsLoadedSound* sound );

#define TINYSOUND_H
#endif

#ifdef TS_IMPLEMENTATION

#include <stdlib.h>	// malloc, free
#include <stdio.h>	// fopen, fclose
#include <string.h>	// memcmp, memset, memcpy
#include <xmmintrin.h>
#include <emmintrin.h>

// 1 : include dsound.h
// 0 : use custom symbol definitions to remove dsound.h dependency
#define TS_USE_DSOUND_HEADER 1

#if TS_USE_DSOUND_HEADER
	#include <dsound.h>
	#undef PlaySound
#else
#ifdef __cplusplus
extern "C" {
#endif
typedef struct IDirectSound *LPDIRECTSOUND;
typedef struct IDirectSoundBuffer *LPDIRECTSOUNDBUFFER;
typedef unsigned long DWORD;
typedef unsigned short WORD;
typedef long LONG;
typedef unsigned long ULONG;
typedef long *LPLONG;
typedef LONG HRESULT;
typedef signed short INT16;
typedef void *LPVOID;
typedef DWORD *LPDWORD;
typedef struct
{
	struct IUnknownVtbl *lpVtbl;
} IUnknown;
typedef  IUnknown *LPUNKNOWN;

#define DECLARE_HANDLE(name) struct name##__{int unused;}; typedef struct name##__ *name
DECLARE_HANDLE(HWND);
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define _FACDS  0x878 /* DirectSound's facility code */
#define MAKE_DSHRESULT(code)  MAKE_HRESULT(1, _FACDS, code)
#define MAKE_HRESULT(sev,fac,code) ((HRESULT) (((unsigned long)(sev)<<31) | ((unsigned long)(fac)<<16) | ((unsigned long)(code))) )
#define DSERR_BUFFERLOST MAKE_DSHRESULT(150)
#define DSBPLAY_LOOPING 0x00000001
#define DSSCL_PRIORITY 0x00000002
#define WINAPI __stdcall
#define DSBCAPS_PRIMARYBUFFER 0x00000001
#define WAVE_FORMAT_PCM 1

typedef struct _GUID {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[ 8 ];
} GUID;
typedef const GUID *LPCGUID;
typedef GUID IID;

typedef struct _DSBCAPS
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwBufferBytes;
    DWORD           dwUnlockTransferRate;
    DWORD           dwPlayCpuOverhead;
} DSBCAPS, *LPDSBCAPS;

typedef struct _DSCAPS
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwMinSecondarySampleRate;
    DWORD           dwMaxSecondarySampleRate;
    DWORD           dwPrimaryBuffers;
    DWORD           dwMaxHwMixingAllBuffers;
    DWORD           dwMaxHwMixingStaticBuffers;
    DWORD           dwMaxHwMixingStreamingBuffers;
    DWORD           dwFreeHwMixingAllBuffers;
    DWORD           dwFreeHwMixingStaticBuffers;
    DWORD           dwFreeHwMixingStreamingBuffers;
    DWORD           dwMaxHw3DAllBuffers;
    DWORD           dwMaxHw3DStaticBuffers;
    DWORD           dwMaxHw3DStreamingBuffers;
    DWORD           dwFreeHw3DAllBuffers;
    DWORD           dwFreeHw3DStaticBuffers;
    DWORD           dwFreeHw3DStreamingBuffers;
    DWORD           dwTotalHwMemBytes;
    DWORD           dwFreeHwMemBytes;
    DWORD           dwMaxContigFreeHwMemBytes;
    DWORD           dwUnlockTransferRateHwBuffers;
    DWORD           dwPlayCpuOverheadSwBuffers;
    DWORD           dwReserved1;
    DWORD           dwReserved2;
} DSCAPS, *LPDSCAPS;

typedef struct tWAVEFORMATEX
{
    WORD        wFormatTag;
    WORD        nChannels;
    DWORD       nSamplesPerSec;
    DWORD       nAvgBytesPerSec;
    WORD        nBlockAlign;
    WORD        wBitsPerSample;
    WORD        cbSize;
} WAVEFORMATEX, *PWAVEFORMATEX,  *NPWAVEFORMATEX,  *LPWAVEFORMATEX;

typedef struct _DSBUFFERDESC
{
    DWORD           dwSize;
    DWORD           dwFlags;
    DWORD           dwBufferBytes;
    DWORD           dwReserved;
    LPWAVEFORMATEX  lpwfxFormat;

    GUID            guid3DAlgorithm;
} DSBUFFERDESC, *LPDSBUFFERDESC;

typedef const DSBUFFERDESC *LPCDSBUFFERDESC;
typedef const WAVEFORMATEX  *LPCWAVEFORMATEX;

extern HRESULT WINAPI DirectSoundCreate(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, void* pUnkOuter);

typedef struct IDirectSound { struct IDirectSoundVtbl * lpVtbl; }
IDirectSound; typedef struct IDirectSoundVtbl IDirectSoundVtbl; struct IDirectSoundVtbl
{
    HRESULT (__stdcall * QueryInterface) (IDirectSound * This,   const IID * const,  LPVOID*);
    ULONG (__stdcall * AddRef) (IDirectSound * This);
    ULONG (__stdcall * Release) (IDirectSound * This);

    HRESULT (__stdcall * CreateSoundBuffer)    (IDirectSound * This, LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER *ppDSBuffer, LPUNKNOWN pUnkOuter);
    HRESULT (__stdcall * GetCaps)              (IDirectSound * This, LPDSCAPS pDSCaps);
    HRESULT (__stdcall * DuplicateSoundBuffer) (IDirectSound * This, LPDIRECTSOUNDBUFFER pDSBufferOriginal, LPDIRECTSOUNDBUFFER *ppDSBufferDuplicate);
    HRESULT (__stdcall * SetCooperativeLevel)  (IDirectSound * This, HWND hwnd, DWORD dwLevel);
    HRESULT (__stdcall * Compact)              (IDirectSound * This);
    HRESULT (__stdcall * GetSpeakerConfig)     (IDirectSound * This, LPDWORD pdwSpeakerConfig);
    HRESULT (__stdcall * SetSpeakerConfig)     (IDirectSound * This, DWORD dwSpeakerConfig);
    HRESULT (__stdcall * Initialize)           (IDirectSound * This, LPCGUID pcGuidDevice);
};

typedef struct IDirectSoundBuffer { struct IDirectSoundBufferVtbl * lpVtbl; }
IDirectSoundBuffer; typedef struct IDirectSoundBufferVtbl IDirectSoundBufferVtbl; struct IDirectSoundBufferVtbl
{
    HRESULT (__stdcall * QueryInterface) (IDirectSoundBuffer * This,   const IID * const,  LPVOID*);
    ULONG (__stdcall * AddRef) (IDirectSoundBuffer * This);
    ULONG (__stdcall * Release) (IDirectSoundBuffer * This);

    HRESULT (__stdcall * GetCaps)              (IDirectSoundBuffer * This, LPDSBCAPS pDSBufferCaps);
    HRESULT (__stdcall * GetCurrentPosition)   (IDirectSoundBuffer * This, LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor);
    HRESULT (__stdcall * GetFormat)            (IDirectSoundBuffer * This, LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten);
    HRESULT (__stdcall * GetVolume)            (IDirectSoundBuffer * This, LPLONG plVolume);
    HRESULT (__stdcall * GetPan)               (IDirectSoundBuffer * This, LPLONG plPan);
    HRESULT (__stdcall * GetFrequency)         (IDirectSoundBuffer * This, LPDWORD pdwFrequency);
    HRESULT (__stdcall * GetStatus)            (IDirectSoundBuffer * This, LPDWORD pdwStatus);
    HRESULT (__stdcall * Initialize)           (IDirectSoundBuffer * This, LPDIRECTSOUND pDirectSound, LPCDSBUFFERDESC pcDSBufferDesc);
    HRESULT (__stdcall * Lock)                 (IDirectSoundBuffer * This, DWORD dwOffset, DWORD dwBytes, LPVOID *ppvAudioPtr1, LPDWORD pdwAudioBytes1, LPVOID *ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags);
    HRESULT (__stdcall * Play)                 (IDirectSoundBuffer * This, DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags);
    HRESULT (__stdcall * SetCurrentPosition)   (IDirectSoundBuffer * This, DWORD dwNewPosition);
    HRESULT (__stdcall * SetFormat)            (IDirectSoundBuffer * This, LPCWAVEFORMATEX pcfxFormat);
    HRESULT (__stdcall * SetVolume)            (IDirectSoundBuffer * This, LONG lVolume);
    HRESULT (__stdcall * SetPan)               (IDirectSoundBuffer * This, LONG lPan);
    HRESULT (__stdcall * SetFrequency)         (IDirectSoundBuffer * This, DWORD dwFrequency);
    HRESULT (__stdcall * Stop)                 (IDirectSoundBuffer * This);
    HRESULT (__stdcall * Unlock)               (IDirectSoundBuffer * This, LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2);
    HRESULT (__stdcall * Restore)              (IDirectSoundBuffer * This);
};
#ifdef __cplusplus
}
#endif
#endif

#pragma push_macro( "CHECK" )
#pragma push_macro( "ASSERT" )
#pragma push_macro( "ALIGN" )
#pragma push_macro( "TRUNC" )
#undef CHECK
#undef ASSERT
#undef ALIGN
#undef TRUNC
#define CHECK( X, Y ) do { if ( !(X) ) { g_tsErrorReason = Y; goto err; } } while ( 0 )
#define ASSERT( X ) do { if ( !(X) ) *(int*)0 = 0; } while ( 0 )
#define ALIGN( X, Y ) ((((size_t)X) + ((Y) - 1)) & ~((Y) - 1))
#define TRUNC( X, Y ) ((size_t)(X) & ~((Y) - 1))

const char* g_tsErrorReason;

static void* tsReadFileToMemory( const char* path, int* size )
{
	void* data = 0;
	FILE* fp = fopen( path, "rb" );
	int sizeNum = 0;

	if ( fp )
	{
		fseek( fp, 0, SEEK_END );
		sizeNum = ftell( fp );
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
	p = (char*)ALIGN( p + 16, 16 );
	*((char*)p - 1) = 16 - offset;
	ASSERT( !((size_t)p & 15) );
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

void tsReadMemWAV( void* memory, tsLoadedSound* sound )
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
	char* first = data;
	CHECK( data, "Unable to read input file (file doesn't exist, or could not allocate heap memory." );
	CHECK( tsFourCC( "RIFF", data ), "Incorrect file header; is this a WAV file?" );
	CHECK( tsFourCC( "WAVE", data + 8 ), "Incorrect file header; is this a WAV file?" );

	data += 12;

	CHECK( tsFourCC( "fmt ", data ), "fmt chunk not found." );
	Fmt fmt;
	fmt = *(Fmt*)(data + 8);
	CHECK( fmt.wFormatTag == 1, "Only PCM WAV files are supported." );
	CHECK( fmt.nChannels == 1 || fmt.nChannels == 2, "Only mono or stereo supported (too many channels detected)." );
	CHECK( fmt.wBitsPerSample == 16, "Only 16 bits per sample supported." );
	CHECK( fmt.nBlockAlign == fmt.nChannels * 2, "implementation error" );

	data = tsNext( data );
	CHECK( tsFourCC( "data", data ), "data chunk not found." );
	int sample_size = *((uint32_t*)(data + 4));
	int sample_count = sample_size / (fmt.nChannels * sizeof( uint16_t ));
	sound->sample_count = sample_count;
	sound->channel_count = fmt.nChannels;

	int wide_count = (int)ALIGN( sample_count, 4 );
	wide_count /= 4;
	int wide_offset = sample_count & 3;
	int16_t* samples = (int16_t*)(data + 8);
	float* sample = (float*)alloca( sizeof( float ) * 4 + 16 );
	sample = (float*)ALIGN( sample, 16 );

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
		CHECK( 0, "unsupported channel count (only support mono and stereo)." );
	}

	return;

err:
	free( sound->channels[ 0 ] );
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
void tsReadMemOGG( void* memory, int length, int* sample_rate, tsLoadedSound* sound )
{
	int16_t* samples = 0;
	int channel_count;
	int sample_count = stb_vorbis_decode_memory( (const unsigned char*)memory, length, &channel_count, sample_rate, &samples );

	CHECK( sample_count > 0, "stb_vorbis_decode_memory failed. Make sure your file exists and is a valid OGG file." );

	int wide_count = (int)ALIGN( sample_count, 4 ) / 4;
	int wide_offset = sample_count & 3;
	float* sample = (float*)alloca( sizeof( float ) * 4 + 16 );
	sample = (float*)ALIGN( sample, 16 );
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
		CHECK( 0, "Unsupported channel count." );
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
	free16( sound->channels[ 1 ] );
	memset( sound, 0, sizeof( tsLoadedSound ) );
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

void tsSetVolume( tsPlayingSound* sound, float volume_left, float volume_right )
{
	if ( volume_left < 0.0f ) volume_left = 0.0f;
	if ( volume_right < 0.0f ) volume_right = 0.0f;
	sound->volume0 = volume_left;
	sound->volume1 = volume_right;
}

struct tsContext
{
	unsigned latency_samples;
	unsigned running_index;
	int Hz;
	int bps;
	int buffer_size;
	int wide_count;
	LPDIRECTSOUND dsound;
	LPDIRECTSOUNDBUFFER buffer;
	LPDIRECTSOUNDBUFFER primary;
	tsPlayingSound* playing;
	__m128* floatA;
	__m128* floatB;
	__m128i* samples;
	tsPlayingSound* playing_pool;
	tsPlayingSound* playing_free;
};

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
	int wide_count = (int)ALIGN( sample_count, 4 );
	int pool_size = playing_pool_count * sizeof( tsPlayingSound );
	int mix_buffers_size = sizeof( __m128 ) * wide_count * 2;
	int sample_buffer_size = sizeof( __m128i ) * wide_count;
	tsContext* ctx = (tsContext*)malloc( sizeof( tsContext ) + mix_buffers_size + sample_buffer_size + 16 + pool_size );
	ctx->latency_samples = (unsigned)ALIGN( play_frequency_in_Hz / latency_factor_in_Hz, 4 );
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
	ctx->floatA = (__m128*)ALIGN( ctx->floatA, 16 );
	ASSERT( !((size_t)ctx->floatA & 15) );
	ctx->floatB = ctx->floatA + wide_count;
	ctx->samples = (__m128i*)ctx->floatB + wide_count;

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

void tsShutdownContext( tsContext* ctx )
{
	ctx->buffer->lpVtbl->Release( ctx->buffer );
	ctx->primary->lpVtbl->Release( ctx->primary );
	ctx->dsound->lpVtbl->Release( ctx->dsound );
	free( ctx );
}

void tsInsertSound( tsContext* ctx, tsPlayingSound* sound )
{
	// Cannot use tsPlayingSound if tsMakeContext was passed non-zero for playing_pool_count
	// since non-zero playing_pool_count means the context is doing some memory-management
	// for a playing sound pool. InsertSound assumes the pool does not exist, and is apart
	// of the lower-level API (see top of this header for documentation details).
	ASSERT( ctx->playing_pool == 0 );

	if ( sound->active ) return;
	sound->next = ctx->playing;
	ctx->playing = sound;
	sound->active = 1;
}

// NOTE: does not allow delay_in_seconds to be negative (clamps at 0)
void tsSetDelay( tsContext* ctx, tsPlayingSound* sound, float delay_in_seconds )
{
	if ( delay_in_seconds < 0.0f ) delay_in_seconds = 0.0f;
	sound->sample_index = (int)(delay_in_seconds * (float)ctx->Hz);
	sound->sample_index = -(int)ALIGN( sound->sample_index, 4 );
}

tsPlaySoundDef tsMakeDef( tsLoadedSound* sound )
{
	tsPlaySoundDef def;
	def.paused = 0;
	def.looped = 0;
	def.volume_left = 1.0f;
	def.volume_right = 1.0f;
	def.pan = 0.5f;
	def.delay = 0.0f;
	def.loaded = sound;
	return def;
}

tsPlayingSound* tsPlaySound( tsContext* ctx, tsPlaySoundDef def )
{
	tsPlayingSound* playing = ctx->playing_free;
	if ( !playing ) return 0;
	ctx->playing_free = playing->next;
	*playing = tsMakePlayingSound( def.loaded );
	playing->active = 1;
	playing->paused = def.paused;
	playing->looped = def.looped;
	tsSetVolume( playing, def.volume_left, def.volume_right );
	tsSetPan( playing, def.pan );
	tsSetDelay( ctx, playing, def.delay );
	playing->next = ctx->playing;
	ctx->playing = playing;
	return playing;
}

static void tsPosition( tsContext* ctx, int* byte_to_lock, int* bytes_to_write )
{
	// compute bytes to be written to direct sound
	DWORD play_cursor;
	DWORD write_cursor;
	HRESULT hr = ctx->buffer->lpVtbl->GetCurrentPosition( ctx->buffer, &play_cursor, &write_cursor );
	ASSERT( hr == DS_OK );

	DWORD lock = (ctx->running_index * ctx->bps) % ctx->buffer_size;
	DWORD target_cursor = (write_cursor + ctx->latency_samples * ctx->bps) % ctx->buffer_size;
	target_cursor = (DWORD)ALIGN( target_cursor, 16 );
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

void tsMix( tsContext* ctx )
{
	int byte_to_lock;
	int bytes_to_write;
	tsPosition( ctx, &byte_to_lock, &bytes_to_write );

	if ( !bytes_to_write )
		return;

	// clear mixer buffers
	int samples_to_write = bytes_to_write / ctx->bps;
	int wide_count = samples_to_write / 4;
	ASSERT( !(samples_to_write & 3) );

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

		int mix_count = samples_to_write;
		int offset = playing->sample_index;
		int remaining = loaded->sample_count - offset;
		if ( remaining < mix_count ) mix_count = remaining;
		ASSERT( remaining > 0 );

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
		ASSERT( !(delay_offset & 3) );

		// immediately remove any inactive elements
		if ( !playing->active )
			goto remove;

		// skip all paused sounds
		if ( playing->paused )
			goto get_next_playing_sound;

		// SIMD offets
		int mix_wide = (int)ALIGN( mix_count, 4 ) / 4;
		int offset_wide = (int)TRUNC( offset, 4 ) / 4;
		int delay_wide = (int)ALIGN( delay_offset, 4 ) / 4;

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
}

#pragma pop_macro( "CHECK" )
#pragma pop_macro( "ASSERT" )
#pragma pop_macro( "ALIGN" )
#pragma pop_macro( "TRUNC" )

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
