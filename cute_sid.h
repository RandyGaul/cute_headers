/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_sid.h - v1.0

	To create implementation (the function definitions)
		#define CUTE_SID_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY:
		Implements a compile-time string hasher via preprocessing.
		Preprocesses an input file and turns all SID("string") instances
		into compile-time hashed integers that look more like:
		
		SID("string")
		
		turns into
		
		0x10293858 // "string"

	To use this lib just call sid_preprocess on a file. See the example file at
	https://github.com/RandyGaul/tinyheaders on how to recursively visit and call
	sid_preprocess no all source files in a directory.
*/

#if !defined(CUTE_SID_H)

#include <stdint.h>   // uint64_t

// path and out_path can point to the same file, or to different files
int sid_preprocess(const char* path, const char* out_path);

// Feel free to use SID at run-time to hash strings on the spot. Then at
// some point in time (as a pre-build step or even when your application
// is running) preprocess your files to create hard-coded constants.
// The underlying hash function can be swapped out as-needed by modifying
// this source file. Just replace all instances of sid_FNV1a with your own
// hash function, along with a matching signature.
#define SID(str, len) sid_FNV1a(str, str + strlen(str))
uint64_t sid_FNV1a(const void* buf, int len);

#define CUTE_SID_H
#endif

#ifdef CUTE_SID_IMPLEMENTATION
#undef CUTE_SID_IMPLEMENTATION

#include <stdlib.h>   // malloc, free
#include <stdio.h>    // fopen, fseek, ftell, fclose, fwrite, fread
#include <ctype.h>    // isspace
#include <inttypes.h> // PRIx64

uint64_t sid_FNV1a(const void* buf, int len)
{
	uint64_t h = (uint64_t)14695981039346656037U;
	const char* str = (const char*)buf;

	while (len--)
	{
		char c = *str++;
		h = h ^ (uint64_t)c;
		h = h * (uint64_t)1099511628211;
	}

	return h;
}

uint64_t sid_FNV1a_str_end(char* str, char* end)
{
	int len = end - str;
	return sid_FNV1a(str, len);
}

static void sid_skip_white_internal(char** dataPtr, char** outPtr)
{
	char* data = *dataPtr;
	char* out = *outPtr;

	while (isspace(*data))
		*out++ = *data++;

	*dataPtr = data;
	*outPtr = out;
}

static int sid_next_internal(char** dataPtr, char** outPtr)
{
	char* data = *dataPtr;
	char* out = *outPtr;
	int result = 1;

	while (1)
	{
		if (!*data)
		{
			result = 0;
			goto return_result;
		}

		sid_skip_white_internal(&data, &out);

		if (!*data)
		{
			result = 0;
			goto return_result;
		}

		if (!isalnum(*data))
		{
			*out++ = *data++;
			continue;
		}

		int index = 0;
		int matched = 1;
		const char* match = "SID(";
		while (1)
		{
			if (!match[index])
				break;

			if (!*data)
			{
				result = 0;
				goto return_result;
			}

			if (*data == match[index])
			{
				++data;
				++index;
			}

			else
			{
				matched = 0;
				break;
			}
		}

		data -= index;

		if (matched)
			break;

		while (isalnum(*data))
			*out++ = *data++;
	}

	return_result:

	*dataPtr = data;
	*outPtr = out;

	return result;
}

int sid_preprocess(const char* path, const char* out_path)
{
	char* data = 0;
	int size = 0;
	FILE* fp = fopen(path, "rb");

	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		data = (char*)malloc(size + 1);
		fread(data, size, 1, fp);
		fclose(fp);
		data[size] = 0;
	}

	else
	{
		printf("SID ERROR: sid.exe could not open input file %s. Skipping this file.\n", path);
		return 0;
	}

	char* out = (char*)malloc(size * 2);
	char* outOriginal = out;
	
	int fileWasModified = 0;

	while (sid_next_internal(&data, &out))
	{
		fileWasModified = 1;
		data += 4;
		while (isspace(*data))
			data++;

		// TODO: handle escaped quotes
		if (*data != '"')
		{
			printf("SID WARN (%s): Only strings can be placed inside of the SID macro. Skipping this file.\n", path);
			return 0;
		}

		char* ptr = ++data;

		// TODO: make sure that strings in here aren't over the maximum length
		while (1)
		{
			if (*ptr == '\\')
			{
				ptr += 2;
				continue;
			}

			if (*ptr == '"')
				break;

			++ptr;
		}

		unsigned h = (unsigned)sid_FNV1a_str_end(data, ptr); // TODO: detect and report collisions
		int bytes = ptr - data;
		sprintf(out, "0x%.16" PRIx64 " /* \"%.*s\" */", (long long unsigned int)h, bytes, data);
		out += 19 + bytes;

		data = ptr + 1;
		while (isspace(*data))
			data++;
		if (data[0] != ')')
		{
			printf("SID ERROR (%s): Must have) immediately after the SID macro (look near the string \"%.*s\"). Skipping this file.\n", path, (int)(ptr - data), data);
			return 0;
		}
		data += 1;
	}

	if (fileWasModified)
	{
		fp = fopen(out_path, "wb");
		fwrite(outOriginal, out - outOriginal, 1, fp);
		fclose(fp);
	}

	return fileWasModified;
}

#endif // CUTE_SID_IMPLEMENTATION

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
