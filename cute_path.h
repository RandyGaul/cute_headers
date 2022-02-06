/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_path.h - v1.01

	To create implementation (the function definitions)
		#define CUTE_PATH_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY:

		Collection of c-string manipulation functions for dealing with common file-path
		operations. More or less a less-fully-featured replacement for Shlwapi.h path
		functions on Windows.

		Performs no dynamic memory management and has no external dependencies (other than
		some crt funcs).

	Revision history:
		1.0  (11/01/2017) initial release
		1.01 (11/10/2017) path_compact, path_pop bugfixes
*/

/*
	Contributors:
		sro5h             1.01 - path_compact, path_pop bugfixes
*/

#if !defined(CUTE_PATH_H)

#define CUTE_PATH_MAX_PATH 1024
#define CUTE_PATH_MAX_EXT 32

// Copies path to out, but not the extension. Places a nul terminator in out.
// Returns the length of the string in out, excluding the nul byte.
// Length of copied output can be up to CUTE_PATH_MAX_PATH. Can also copy the file
// extension into ext, up to CUTE_PATH_MAX_EXT.
#if !defined(__cplusplus)
int path_pop_ext(const char* path, char* out, char* ext);
#else
int path_pop_ext(const char* path, char* out = 0, char* ext = 0);
#endif

// Copies path to out, but excludes the final file or folder from the output.
// If the final file or folder contains a period, the file or folder will
// still be appropriately popped. If the path contains only one file or folder,
// the output will contain a period representing the current directory. All
// outputs are nul terminated.
// Returns the length of the string in out, excluding the nul byte.
// Length of copied output can be up to CUTE_PATH_MAX_PATH.
// Optionally stores the popped filename in pop. pop can be NULL.
// out can also be NULL.
#if !defined(__cplusplus)
int path_pop(const char* path, char* out, char* pop);
#else
int path_pop(const char* path, char* out = 0, char* pop = 0);
#endif

// Concatenates path_b onto the end of path_a. Will not write beyond max_buffer_length.
// Places a single '/' character between path_a and path_b. Does no other "intelligent"
// manipulation of path_a and path_b; it's a basic strcat kind of function.
void path_concat(const char* path_a, const char* path_b, char* out, int max_buffer_length);

// Copies the name of the folder the file sits in (but not the entire path) to out. Will
// not write beyond max_buffer_length. Length of copied output can be up to CUTE_PATH_MAX_PATH.
// path contains the full path to the file in question.
// Returns 0 for inputs of "", "." or ".." as the path, 1 otherwise (success).
int path_name_of_folder_im_in(const char* path, char* out);

// Shrinks the path to the desired length n, the out buffer will never be bigger than
// n + 1. Places three '.' between the last part of the path and the first part that
// will be shortened to fit. If the last part is too long to fit in a string of length n,
// the last part will be shortened to fit and three '.' will be added in front & back.
int path_compact(const char* path, char* out, int n);

// Some useful (but not yet implemented) functions
/*
	int path_root(const char* path, char* out);
*/

#define CUTE_PATH_UNIT_TESTS 1
void path_do_unit_tests();

#define CUTE_PATH_H
#endif

#ifdef CUTE_PATH_IMPLEMENTATION
#ifndef CUTE_PATH_IMPLEMENTATION_ONCE
#define CUTE_PATH_IMPLEMENTATION_ONCE

#ifdef _WIN32

	#if !defined(_CRT_SECURE_NO_WARNINGS)
		#define _CRT_SECURE_NO_WARNINGS
	#endif

#endif

#include <string.h> // strncpy, strncat, strlen
#define CUTE_PATH_STRNCPY strncpy
#define CUTE_PATH_STRNCAT strncat
#define CUTE_PATH_STRLEN strlen

int path_is_slash(char c)
{
	return (c == '/') | (c == '\\');
}

int path_pop_ext(const char* path, char* out, char* ext)
{
	if (out != NULL)
		*out = '\0';
	if (ext != NULL)
		*ext = '\0';

	// skip leading dots
	const char *p = path;
	while (*p == '.')
		++p;

	const char *last_slash = path;
	const char *last_period = NULL;
	while (*p != '\0')
	{
		if (path_is_slash(*p))
			last_slash = p;
		else if (*p == '.')
			last_period = p;
		++p;
	}

	if (last_period != NULL && last_period > last_slash)
	{
		if (ext != NULL)
			CUTE_PATH_STRNCPY(ext, last_period + 1, CUTE_PATH_MAX_EXT);
	}
	else
	{
		last_period = p;
	}

	int len = (int)(last_period - path);
	if (len > CUTE_PATH_MAX_PATH - 1) len = CUTE_PATH_MAX_PATH - 1;

	if (out != NULL)
	{
		CUTE_PATH_STRNCPY(out, path, len);
		out[len] = '\0';
	}

	return len;
}

int path_pop(const char* path, char* out, char* pop)
{
	const char* original = path;
	int total_len = 0;
	while (*path)
	{
		++total_len;
		++path;
	}

	// ignore trailing slash from input path
	if (path_is_slash(*(path - 1)))
	{
		--path;
		total_len -= 1;
	}

	int pop_len = 0; // length of substring to be popped
	while (!path_is_slash(*--path) && pop_len != total_len)
		++pop_len;
	int len = total_len - pop_len; // length to copy

        // don't ignore trailing slash if it is the first character
        if (len > 1)
        {
                len -= 1;
        }

	if (len > 0)
	{
		if (out)
		{
			CUTE_PATH_STRNCPY(out, original, len);
			out[len] = 0;
		}

		if (pop)
		{
			CUTE_PATH_STRNCPY(pop, path + 1, pop_len);
			pop[pop_len] = 0;
		}

		return len;
	}

	else
	{
		if (out)
		{
			out[0] = '.';
			out[1] = 0;
		}
		if (pop) *pop = 0;
		return 1;
	}
}

static int path_strncpy(char* dst, const char* src, int n, int max)
{
	int c;

	do
	{
		if (n >= max - 1)
		{
			dst[max - 1] = 0;
			break;
		}
		c = *src++;
		dst[n] = c;
		++n;
	} while (c);

	return n;
}

void path_concat(const char* path_a, const char* path_b, char* out, int max_buffer_length)
{
	int n = path_strncpy(out, path_a, 0, max_buffer_length);
	n = path_strncpy(out, "/", n - 1, max_buffer_length);
	path_strncpy(out, path_b, n - 1, max_buffer_length);
}

int path_name_of_folder_im_in(const char* path, char* out)
{
	// return failure for empty strings and "." or ".."
	if (!*path || (*path == '.' && CUTE_PATH_STRLEN(path) < 3)) return 0;
	int len = path_pop(path, out, NULL);
	int has_slash = 0;
	for (int i = 0; out[i]; ++i)
	{
		if (path_is_slash(out[i]))
		{
			has_slash = 1;
			break;
		}
	}

	if (has_slash)
	{
		int n = path_pop(out, NULL, NULL) + 1;
		len -= n;
		CUTE_PATH_STRNCPY(out, path + n, len);
	}

	else CUTE_PATH_STRNCPY(out, path, len);
	out[len] = 0;
	return 1;
}

int path_compact(const char* path, char* out, int n)
{
	if (n <= 6) return 0;

	const char* sep = "...";
	const int seplen = (int)strlen(sep);

	int pathlen = (int)strlen(path);
	out[0] = 0;

	if (pathlen <= n)
	{
		CUTE_PATH_STRNCPY(out, path, pathlen);
		out[pathlen] = 0;
		return pathlen;
	}

	// Find last path separator
	// Ignores the last character as it could be a path separator
	int i = pathlen - 1;
	do
	{
		--i;
	} while (!path_is_slash(path[i]) && i > 0);

	const char* back = path + i;
	int backlen = (int)strlen(back);

	// No path separator was found or the first character was one
	if (pathlen == backlen)
	{
		CUTE_PATH_STRNCPY(out, path, n - seplen);
		out[n - seplen] = 0;
		CUTE_PATH_STRNCAT(out, sep, seplen + 1);
		return n;
	}

	// Last path part with separators in front equals n
	if (backlen == n - seplen)
	{
		CUTE_PATH_STRNCPY(out, sep, seplen + 1);
		CUTE_PATH_STRNCAT(out, back, backlen);
		return n;
	}

	// Last path part with separators in front is too long
	if (backlen > n - seplen)
	{
		CUTE_PATH_STRNCPY(out, sep, seplen + 1);
		CUTE_PATH_STRNCAT(out, back, n - (2 * seplen));
		CUTE_PATH_STRNCAT(out, sep, seplen);
		return n;
	}

	int remaining = n - backlen - seplen;

	CUTE_PATH_STRNCPY(out, path, remaining);
	out[remaining] = 0;
	CUTE_PATH_STRNCAT(out, sep, seplen);
	CUTE_PATH_STRNCAT(out, back, backlen);

	return n;
}

#if CUTE_PATH_UNIT_TESTS

	#include <stdio.h>

	#define CUTE_PATH_STRCMP strcmp
	#define CUTE_PATH_EXPECT(X) do { if (!(X)) printf("Failed cute_path.h unit test at line %d of file %s.\n", __LINE__, __FILE__); } while (0)

	void path_do_unit_tests()
	{
		char out[CUTE_PATH_MAX_PATH];
                char pop[CUTE_PATH_MAX_PATH];
                char ext[CUTE_PATH_MAX_PATH];
                int n;

		const char* path = "../root/file.ext";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "../root/file"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, "ext"));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "../root"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, "file.ext"));

		path = "../root/file";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "../root/file"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, ""));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "../root"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, "file"));

		path = "../root/";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "../root/"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, ""));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, ".."));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, "root"));

		path = "../root";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "../root"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, ""));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, ".."));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, "root"));

		path = "/file";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "/file"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, ""));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "/"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, "file"));

		path = "../";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "../"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, ""));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "."));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, ""));

		path = "..";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, ".."));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, ""));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "."));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, ""));

		path = ".";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "."));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, ""));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "."));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, ""));

		path = "";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, ""));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, ""));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "."));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, ""));

		path = "../../file.ext";
		path_pop_ext(path, out, ext);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "../../file"));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(ext, "ext"));

		path_pop(path, out, pop);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "../.."));
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(pop, "file.ext"));

		path = "asdf/file.ext";
		path_name_of_folder_im_in(path, out);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "asdf"));

		path = "asdf/lkjh/file.ext";
		path_name_of_folder_im_in(path, out);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "lkjh"));

		path = "poiu/asdf/lkjh/file.ext";
		path_name_of_folder_im_in(path, out);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "lkjh"));

		path = "poiu/asdf/lkjhqwer/file.ext";
		path_name_of_folder_im_in(path, out);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "lkjhqwer"));

		path = "../file.ext";
		path_name_of_folder_im_in(path, out);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, ".."));

		path = "./file.ext";
		path_name_of_folder_im_in(path, out);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "."));

		path = "..";
		CUTE_PATH_EXPECT(!path_name_of_folder_im_in(path, out));

		path = ".";
		CUTE_PATH_EXPECT(!path_name_of_folder_im_in(path, out));

		path = "";
		CUTE_PATH_EXPECT(!path_name_of_folder_im_in(path, out));

		const char* path_a = "asdf";
		const char* path_b = "qwerzxcv";
		path_concat(path_a, path_b, out, CUTE_PATH_MAX_PATH);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "asdf/qwerzxcv"));

		path_a = "path/owoasf.as.f.q.e.a";
		path_b = "..";
		path_concat(path_a, path_b, out, CUTE_PATH_MAX_PATH);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "path/owoasf.as.f.q.e.a/.."));

		path_a = "a/b/c";
		path_b = "d/e/f/g/h/i";
		path_concat(path_a, path_b, out, CUTE_PATH_MAX_PATH);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "a/b/c/d/e/f/g/h/i"));

		path = "/path/to/file.vim";
		n = path_compact(path, out, 17);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "/path/to/file.vim"));
		CUTE_PATH_EXPECT(n == CUTE_PATH_STRLEN(out));

		path = "/path/to/file.vim";
		n = path_compact(path, out, 16);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "/pat.../file.vim"));
		CUTE_PATH_EXPECT(n == CUTE_PATH_STRLEN(out));

		path = "/path/to/file.vim";
		n = path_compact(path, out, 12);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, ".../file.vim"));
		CUTE_PATH_EXPECT(n == CUTE_PATH_STRLEN(out));

		path = "/path/to/file.vim";
		n = path_compact(path, out, 11);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, ".../file..."));
		CUTE_PATH_EXPECT(n == CUTE_PATH_STRLEN(out));

		path = "longfile.vim";
		n = path_compact(path, out, 12);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "longfile.vim"));
		CUTE_PATH_EXPECT(n == CUTE_PATH_STRLEN(out));

		path = "longfile.vim";
		n = path_compact(path, out, 11);
		CUTE_PATH_EXPECT(!CUTE_PATH_STRCMP(out, "longfile..."));
		CUTE_PATH_EXPECT(n == CUTE_PATH_STRLEN(out));
	}

#else

	void path_do_unit_tests()
	{
	}

#endif // CUTE_PATH_UNIT_TESTS

#endif // CUTE_PATH_IMPLEMENTATION_ONCE
#endif // CUTE_PATH_IMPLEMENTATION

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
