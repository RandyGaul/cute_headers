#if !defined(CUTE_MEMFILE_H)

// change to 0 to compile out any debug checks
#define CUTE_MEMFILE_DEBUG_CHECKS 1

#if CUTE_MEMFILE_DEBUG_CHECKS

	#include <assert.h>
	#include <string.h>
	#define CUTE_MEMFILE_ASSERT assert
	
#else

	#define CUTE_MEMFILE_ASSERT(...)

#endif // CUTE_MEMFILE_DEBUG_CHECKS

#include <stdio.h> // sscanf

// kinda sketchy, but helps compile on Windows
// feel free to remove
#ifdef WIN32
	#define inline __inline
	#if !defined(_CRT_SECURE_NO_WARNINGS)
		#define _CRT_SECURE_NO_WARNINGS
	#endif
#endif

//--------------------------------------------------------------------------------------------------
typedef struct memfile_t
{
	const char* ptr;
	int bytes_read;
	int size;
} memfile_t;

//--------------------------------------------------------------------------------------------------
// Make sure your file is nul-byte terminated! This represents EOF, and
// is important for the internal sscanf call.
inline void memfile_open_in_memory(memfile_t* fp, const void* data, int size)
{
	fp->ptr = (const char*)data;
	fp->bytes_read = 0;
	fp->size = size - 1;
}

//--------------------------------------------------------------------------------------------------
int memfile_eof(memfile_t* fp)
{
	return fp->bytes_read == fp->size;
}

//--------------------------------------------------------------------------------------------------
// returns 1 on succes, 0 on out of bounds access
// feel free to remove the bytes_read clamping for efficiency purposes
inline int memfile_seek(memfile_t* fp, int offset)
{
	CUTE_MEMFILE_ASSERT(offset >= 0);
	CUTE_MEMFILE_ASSERT(offset < fp->size + 1);

	int size = fp->size;
	int ret = 1;
	if (offset < 0)
	{
		ret = 0;
		offset = 0;
	}
	
	else if (offset >= size)
	{
		ret = 0;
		offset = size - 1;
	}
	
	fp->bytes_read = offset;
	return ret;
}

//--------------------------------------------------------------------------------------------------
inline void memfile_format_buffer_internal(const char* format, char* buffer)
{
	int i;
	CUTE_MEMFILE_ASSERT(strlen(format) + 1 < 256 - 3);

	for (i = 0; format[i]; ++i)
	{
		buffer[i] = format[i];
	}

	buffer[i] = '%';
	buffer[i + 1] = 'n';
	buffer[i + 2] = 0;
}

//--------------------------------------------------------------------------------------------------
#define CUTE_MEMFILE_LOOP_0(F, N)
#define CUTE_MEMFILE_LOOP_1(F, N) F(1)
#define CUTE_MEMFILE_LOOP_2(F, N) CUTE_MEMFILE_LOOP_1(F, N), F(2)
#define CUTE_MEMFILE_LOOP_3(F, N) CUTE_MEMFILE_LOOP_2(F, N), F(3)
#define CUTE_MEMFILE_LOOP_4(F, N) CUTE_MEMFILE_LOOP_3(F, N), F(4)
#define CUTE_MEMFILE_LOOP_5(F, N) CUTE_MEMFILE_LOOP_4(F, N), F(5)
#define CUTE_MEMFILE_LOOP_6(F, N) CUTE_MEMFILE_LOOP_5(F, N), F(6)
#define CUTE_MEMFILE_LOOP_7(F, N) CUTE_MEMFILE_LOOP_6(F, N), F(7)
#define CUTE_MEMFILE_LOOP_8(F, N) CUTE_MEMFILE_LOOP_7(F, N), F(8)

#define CUTE_MEMFILE_ITERATE_0(F)
#define CUTE_MEMFILE_ITERATE_1(F) F(1)
#define CUTE_MEMFILE_ITERATE_2(F) CUTE_MEMFILE_ITERATE_1(F) F(2)
#define CUTE_MEMFILE_ITERATE_3(F) CUTE_MEMFILE_ITERATE_2(F) F(3)
#define CUTE_MEMFILE_ITERATE_4(F) CUTE_MEMFILE_ITERATE_3(F) F(4)
#define CUTE_MEMFILE_ITERATE_5(F) CUTE_MEMFILE_ITERATE_4(F) F(5)
#define CUTE_MEMFILE_ITERATE_6(F) CUTE_MEMFILE_ITERATE_5(F) F(6)
#define CUTE_MEMFILE_ITERATE_7(F) CUTE_MEMFILE_ITERATE_6(F) F(7)
#define CUTE_MEMFILE_ITERATE_8(F) CUTE_MEMFILE_ITERATE_7(F) F(8)

#define CUTE_MEMFILE_ITERATE(F, N) CUTE_MEMFILE_ITERATE_##N(F)

#define CUTE_MEMFILE_LOOP(F, N) CUTE_MEMFILE_LOOP_##N(F, N)

#define CUTE_MEMFILE_SCANF_PARAMS(N) \
	void* argument_##N

#define CUTE_MEMFILE_SCANF_ARGS(N) \
	argument_##N

#define CUTE_MEMFILE_SCANF_FUNC(N) \
	inline int memfile_scanf(memfile_t* file, const char* format, CUTE_MEMFILE_LOOP(CUTE_MEMFILE_SCANF_PARAMS, N)) \
	{ \
		char buffer[256]; \
		int bytes_read; \
		int ret; \
		memfile_format_buffer_internal(format, buffer); \
		ret = sscanf(file->ptr + file->bytes_read, buffer, CUTE_MEMFILE_LOOP(CUTE_MEMFILE_SCANF_ARGS, N), &bytes_read); \
		file->bytes_read += bytes_read; \
		CUTE_MEMFILE_ASSERT(file->bytes_read <= file->size); \
		return ret; \
	}

CUTE_MEMFILE_ITERATE(CUTE_MEMFILE_SCANF_FUNC, 8)

#define CUTE_MEMFILE_H
#endif

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
