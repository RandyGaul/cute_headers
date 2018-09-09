/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_time.h - v1.00

	To create implementation (the function definitions)
		#define CUTE_TIME_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file
*/

#if !defined(CUTE_TIME_H)

#include <stdint.h>

typedef struct ct_timer_t ct_timer_t;

// quick and dirty elapsed time since last call
float ct_time();

// high precision timer functions found below

// Call once to setup this timer on this thread
// does a cs_record call
void ct_init_timer(ct_timer_t* timer);

// returns raw ticks in platform-specific units between now-time and last cs_record call
int64_t ct_elapsed(ct_timer_t* timer);

// ticks to seconds conversion
int64_t ct_seconds(ct_timer_t* timer, int64_t ticks);

// ticks to milliseconds conversion
int64_t ct_milliseconds(ct_timer_t* timer, int64_t ticks);

// ticks to microseconds conversion
int64_t ct_microseconds(ct_timer_t* timer, int64_t ticks);

// records the now-time in raw platform-specific units
void cs_record(ct_timer_t* timer);

#define CUTE_TIME_WINDOWS	1
#define CUTE_TIME_MAC		2
#define CUTE_TIME_UNIX		3

#if defined(_WIN32)
	#define CUTE_TIME_PLATFORM CUTE_TIME_WINDOWS
#elif defined(__APPLE__)
	#define CUTE_TIME_PLATFORM CUTE_TIME_MAC
#else
	#define CUTE_TIME_PLATFORM CUTE_TIME_UNIX
#endif

#if CUTE_TIME_PLATFORM == CUTE_TIME_WINDOWS

	struct ct_timer_t
	{
		LARGE_INTEGER freq;
		LARGE_INTEGER prev;
	};

#elif CUTE_TIME_PLATFORM == CUTE_TIME_MAC
#else
#endif

#define CUTE_TIME_H
#endif

#ifdef CUTE_TIME_IMPLEMENTATION
#ifndef CUTE_TIME_IMPLEMENTATION_ONCE
#define CUTE_TIME_IMPLEMENTATION_ONCE

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

#if CUTE_TIME_PLATFORM == CUTE_TIME_WINDOWS

	float ct_time()
	{
		static int first = 1;
		static LARGE_INTEGER prev;
		static double factor;

		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);

		if (first)
		{
			first = 0;
			prev = now;
			LARGE_INTEGER freq;
			QueryPerformanceFrequency(&freq);
			factor = 1.0 / (double)freq.QuadPart;
			return 0;
		}

		float elapsed = (float)((double)(now.QuadPart - prev.QuadPart) * factor);
		prev = now;
		return elapsed;
	}

	void ct_init_timer(ct_timer_t* timer)
	{
		QueryPerformanceCounter(&timer->prev);
		QueryPerformanceFrequency(&timer->freq);
	}

	int64_t ct_elapsed(ct_timer_t* timer)
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (int64_t)(now.QuadPart - timer->prev.QuadPart);
	}

	int64_t ct_seconds(ct_timer_t* timer, int64_t ticks)
	{
		return ticks / timer->freq.QuadPart;
	}

	int64_t ct_milliseconds(ct_timer_t* timer, int64_t ticks)
	{
		return ticks / (timer->freq.QuadPart / 1000);
	}

	int64_t ct_microseconds(ct_timer_t* timer, int64_t ticks)
	{
		return ticks / (timer->freq.QuadPart / 1000000);
	}

	void cs_record(ct_timer_t* timer)
	{
		QueryPerformanceCounter(&timer->prev);
	}

#elif CUTE_TIME_PLATFORM == CUTE_TIME_MAC

	#include <mach/mach_time.h>

	float ct_time()
	{
		static int first = 1;
		static uint64_t prev = 0;
		static double factor;
	
		if (first)
		{
			first = 0;
			mach_timebase_info_data_t info;
			mach_timebase_info(&info);
			factor = (double)info.numer / ((double)info.denom * 1.0e9);
			prev = mach_absolute_time();
			return 0;
		}
	
		uint64_t now = mach_absolute_time();
		float elapsed = (float)((double)(now - prev) * factor);
		prev = now;
		return elapsed;
	}

#else

	#include <time.h>
	
	float ct_time()
	{
		static int first = 1;
		static struct timespec prev;
		
		if (first)
		{
			first = 0;
			clock_gettime(CLOCK_MONOTONIC, &prev);
			return 0;
		}
		
		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);
		float elapsed = (float)((double)(now.tv_sec - prev.tv_sec) + ((double)(now.tv_nsec - prev.tv_nsec) * 1.0e-9));
		prev = now;
		return elapsed;
	}

#endif

#endif // CUTE_TIME_IMPLEMENTATION_ONCE
#endif // CUTE_TIME_IMPLEMENTATION

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
