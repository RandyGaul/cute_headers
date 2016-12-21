#if !defined( TINYTIME_H )

float ttTime( );

#define TINYTIME_H
#endif

#ifdef TT_IMPLEMENTATION

#define TT_WINDOWS	1
#define TT_MAC		2
#define TT_UNIX		3

#if defined( _WIN32 )
	#define TT_PLATFORM TT_WINDOWS
#elif defined( __APPLE__ )
	#define TT_PLATFORM TT_MAC
#else
	#define TT_PLATFORM TT_UNIX
#endif

// These functions are intended be called from a single thread only. In a
// multi-threaded environment make sure to call Time from the main thread only.
// Also try to set a thread affinity for the main thread to avoid core swaps.
// This can help prevent annoying bios bugs etc. from giving weird time-warp
// effects as the main thread gets passed from one core to another. This shouldn't
// be a problem, but it's a good practice to setup the affinity. Calling these
// functions on multiple threads multiple times will grant a heft performance
// loss in the form of false sharing due to CPU cache synchronization across
// multiple cores.
// More info: https://msdn.microsoft.com/en-us/library/windows/desktop/ee417693(v=vs.85).aspx

#if TT_PLATFORM == TT_WINDOWS

	float ttTime( )
	{
		static int first = 1;
		static LARGE_INTEGER prev;
		static double factor;

		LARGE_INTEGER now;
		QueryPerformanceCounter( &now );

		if ( first )
		{
			first = 0;
			prev = now;
			LARGE_INTEGER freq;
			QueryPerformanceFrequency( &freq );
			factor = 1.0 / (double)freq.QuadPart;
			return 0;
		}

		float elapsed = (float)((double)(now.QuadPart - prev.QuadPart) * factor);
		prev = now;
		return elapsed;
	}

#elif TT_PLATFORM == TT_MAC

	#include <mach/mach_time.h>

	float ttTime( )
	{
		static int first = 1;
		static uint64_t prev = 0;
		static double factor;
	
		if ( first )
		{
			first = 0;
			mach_timebase_info_data_t info;
			mach_timebase_info( &info );
			factor = (double)info.numer / ((double)info.denom * 1.0e9);
			prev = mach_absolute_time( );
			return 0;
		}
	
		uint64_t now = mach_absolute_time( );
		float elapsed = (float)((double)(now - prev) * factor);
		prev = now;
		return elapsed;
	}

#else

	#include <time.h>
	
	float ttTime( )
	{
		static int first = 1;
		struct timespec prev;
		
		if ( first )
		{
			first = 0;
			clock_gettime( CLOCK_MONOTONIC, &prev );
			return 0;
		}
		
		struct timespec now;
		clock_gettime( CLOCK_MONOTONIC, &now );
		float elapsed = (float)((double)(now.tv_nsec - prev.tv_nsec) * 1.0e-9);
		prev = now;
		return elapsed;
	}

#endif

#endif // TT_IMPLEMENTATION

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
