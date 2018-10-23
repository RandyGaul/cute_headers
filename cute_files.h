/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	tinyfiles.h - v1.0

	To create implementation (the function definitions)
		#define CUTE_FILES_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	Summary:
		Utility header for traversing directories to apply a function on each found file.
		Recursively finds sub-directories. Can also be used to iterate over files in a
		folder manually. All operations done in a cross-platform manner (thx posix!).

		This header does no dynamic memory allocation, and performs internally safe string
		copies as necessary. Strings for paths, file names and file extensions are all
		capped, and intended to use primarily the C run-time stack memory. Feel free to
		modify the defines in this file to adjust string size limitations.

		Read the header for specifics on each function.

	Here's an example to print all files in a folder:
		cf_dir_t dir;
		cf_dir_open(&dir, "a");

		while (dir.has_next)
		{
			cf_file_t file;
			cf_read_file(&dir, &file);
			printf("%s\n", file.name);
			cf_dir_next(&dir);
		}

		cf_dir_close(&dir);
*/

#if !defined(CUTE_FILES_H)

#define CUTE_FILES_WINDOWS	1
#define CUTE_FILES_MAC		2
#define CUTE_FILES_UNIX		3

#if defined(_WIN32)
	#define CUTE_FILES_PLATFORM CUTE_FILES_WINDOWS
	#if !defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
	#endif
#elif defined(__APPLE__)
	#define CUTE_FILES_PLATFORM CUTE_FILES_MAC
#else
	#define CUTE_FILES_PLATFORM CUTE_FILES_UNIX
#endif

#include <string.h> // strerror, strncpy

// change to 0 to compile out any debug checks
#define CUTE_FILES_DEBUG_CHECKS 1

#if CUTE_FILES_DEBUG_CHECKS

	#include <stdio.h>  // printf
	#include <assert.h> // assert
	#include <errno.h>
	#define CUTE_FILES_ASSERT assert
	
#else

	#define CUTE_FILES_ASSERT(...)

#endif // CUTE_FILES_DEBUG_CHECKS

#define CUTE_FILES_MAX_PATH 1024
#define CUTE_FILES_MAX_FILENAME 256
#define CUTE_FILES_MAX_EXT 32

struct cf_file_t;
struct cf_dir_t;
struct cf_time_t;
typedef struct cf_file_t cf_file_t;
typedef struct cf_dir_t cf_dir_t;
typedef struct cf_time_t cf_time_t;
typedef void (cf_callback_t)(cf_file_t* file, void* udata);

// Stores the file extension in cf_file_t::ext, and returns a pointer to
// cf_file_t::ext
const char* cf_get_ext(cf_file_t* file);

// Applies a function (cb) to all files in a directory. Will recursively visit
// all subdirectories. Useful for asset management, file searching, indexing, etc.
void cf_traverse(const char* path, cf_callback_t* cb, void* udata);

// Fills out a cf_file_t struct with file information. Does not actually open the
// file contents, and instead performs more lightweight OS-specific calls.
int cf_read_file(cf_dir_t* dir, cf_file_t* file);

// Once a cf_dir_t is opened, this function can be used to grab another file
// from the operating system.
void cf_dir_next(cf_dir_t* dir);

// Performs lightweight OS-specific call to close internal handle.
void cf_dir_close(cf_dir_t* dir);

// Performs lightweight OS-specific call to open a file handle on a directory.
int cf_dir_open(cf_dir_t* dir, const char* path);

// Compares file last write times. -1 if file at path_a was modified earlier than path_b.
// 0 if they are equal. 1 if file at path_b was modified earlier than path_a.
int cf_compare_file_times_by_path(const char* path_a, const char* path_b);

// Retrieves time file was last modified, returns 0 upon failure
int cf_get_file_time(const char* path, cf_time_t* time);

// Compares file last write times. -1 if time_a was modified earlier than path_b.
// 0 if they are equal. 1 if time_b was modified earlier than path_a.
int cf_compare_file_times(cf_time_t* time_a, cf_time_t* time_b);

// Returns 1 of file exists, otherwise returns 0.
int cf_file_exists(const char* path);

// Returns 1 if the file's extension matches the string in ext
// Returns 0 otherwise
int cf_match_ext(cf_file_t* file, const char* ext);

// Prints detected errors to stdout
void cf_do_unit_tests();

#if CUTE_FILES_PLATFORM == CUTE_FILES_WINDOWS

#if !defined _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <Windows.h>

	struct cf_file_t
	{
		char path[CUTE_FILES_MAX_PATH];
		char name[CUTE_FILES_MAX_FILENAME];
		char ext[CUTE_FILES_MAX_EXT];
		int is_dir;
		int is_reg;
		size_t size;
	};

	struct cf_dir_t
	{
		char path[CUTE_FILES_MAX_PATH];
		int has_next;
		HANDLE handle;
		WIN32_FIND_DATAA fdata;
	};

	struct cf_time_t
	{
		FILETIME time;
	};

#elif CUTE_FILES_PLATFORM == CUTE_FILES_MAC || CUTE_FILES_PLATFORM == CUTE_FILES_UNIX

	#include <sys/stat.h>
	#include <dirent.h>
	#include <unistd.h>
    #include <time.h>

	struct cf_file_t
	{
		char path[CUTE_FILES_MAX_PATH];
		char name[CUTE_FILES_MAX_FILENAME];
		char ext[CUTE_FILES_MAX_EXT];
		int is_dir;
		int is_reg;
		int size;
		struct stat info;
	};

	struct cf_dir_t
	{
		char path[CUTE_FILES_MAX_PATH];
		int has_next;
		DIR* dir;
		struct dirent* entry;
	};

	struct cf_time_t
	{
		time_t time;
	};

#endif

#define CUTE_FILES_H
#endif

#ifdef CUTE_FILES_IMPLEMENTATION
#ifndef CUTE_FILES_IMPLEMENTATION_ONCE
#define CUTE_FILES_IMPLEMENTATION_ONCE

#define cf_safe_strcpy(dst, src, n, max) cf_safe_strcpy_internal(dst, src, n, max, __FILE__, __LINE__)
static int cf_safe_strcpy_internal(char* dst, const char* src, int n, int max, const char* file, int line)
{
	int c;
	const char* original = src;

	do
	{
		if (n >= max)
		{
			if (!CUTE_FILES_DEBUG_CHECKS) break;
			printf("ERROR: String \"%s\" too long to copy on line %d in file %s (max length of %d).\n"
				, original
				, line
				, file
				, max);
			CUTE_FILES_ASSERT(0);
		}

		c = *src++;
		dst[n] = c;
		++n;
	} while (c);

	return n;
}

const char* cf_get_ext(cf_file_t* file)
{
	char* name = file->name;
	char* period = NULL;
	while (*name++) if (*name == '.') period = name;
	if (period) cf_safe_strcpy(file->ext, period, 0, CUTE_FILES_MAX_EXT);
	else file->ext[0] = 0;
	return file->ext;
}

void cf_traverse(const char* path, cf_callback_t* cb, void* udata)
{
	cf_dir_t dir;
	cf_dir_open(&dir, path);

	while (dir.has_next)
	{
		cf_file_t file;
		cf_read_file(&dir, &file);

		if (file.is_dir && file.name[0] != '.')
		{
			char path2[CUTE_FILES_MAX_PATH];
			int n = cf_safe_strcpy(path2, path, 0, CUTE_FILES_MAX_PATH);
			n = cf_safe_strcpy(path2, "/", n - 1, CUTE_FILES_MAX_PATH);
			cf_safe_strcpy(path2, file.name, n -1, CUTE_FILES_MAX_PATH);
			cf_traverse(path2, cb, udata);
		}

		if (file.is_reg) cb(&file, udata);
		cf_dir_next(&dir);
	}

	cf_dir_close(&dir);
}

int cf_match_ext(cf_file_t* file, const char* ext)
{
	return !strcmp(file->ext, ext);
}

#if CUTE_FILES_PLATFORM == CUTE_FILES_WINDOWS

	int cf_read_file(cf_dir_t* dir, cf_file_t* file)
	{
		CUTE_FILES_ASSERT(dir->handle != INVALID_HANDLE_VALUE);

		int n = 0;
		char* fpath = file->path;
		char* dpath = dir->path;

		n = cf_safe_strcpy(fpath, dpath, 0, CUTE_FILES_MAX_PATH);
		n = cf_safe_strcpy(fpath, "/", n - 1, CUTE_FILES_MAX_PATH);

		char* dname = dir->fdata.cFileName;
		char* fname = file->name;

		cf_safe_strcpy(fname, dname, 0, CUTE_FILES_MAX_FILENAME);
		cf_safe_strcpy(fpath, fname, n - 1, CUTE_FILES_MAX_PATH);

		size_t max_dword = MAXDWORD;
		file->size = ((size_t)dir->fdata.nFileSizeHigh * (max_dword + 1)) + (size_t)dir->fdata.nFileSizeLow;
		cf_get_ext(file);

		file->is_dir = !!(dir->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		file->is_reg = !!(dir->fdata.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) ||
			!(dir->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

		return 1;
	}

	void cf_dir_next(cf_dir_t* dir)
	{
		CUTE_FILES_ASSERT(dir->has_next);

		if (!FindNextFileA(dir->handle, &dir->fdata))
		{
			dir->has_next = 0;
			DWORD err = GetLastError();
			CUTE_FILES_ASSERT(err == ERROR_SUCCESS || err == ERROR_NO_MORE_FILES);
		}
	}

	void cf_dir_close(cf_dir_t* dir)
	{
		dir->path[0] = 0;
		dir->has_next = 0;
		if (dir->handle != INVALID_HANDLE_VALUE) FindClose(dir->handle);
	}

	int cf_dir_open(cf_dir_t* dir, const char* path)
	{
		int n = cf_safe_strcpy(dir->path, path, 0, CUTE_FILES_MAX_PATH);
		n = cf_safe_strcpy(dir->path, "\\*", n - 1, CUTE_FILES_MAX_PATH);
		dir->handle = FindFirstFileA(dir->path, &dir->fdata);
		dir->path[n - 3] = 0;

		if (dir->handle == INVALID_HANDLE_VALUE)
		{
			printf("ERROR: Failed to open directory (%s): %s.\n", path, strerror(errno));
			cf_dir_close(dir);
			CUTE_FILES_ASSERT(0);
			return 0;
		}

		dir->has_next = 1;

		return 1;
	}

	int cf_compare_file_times_by_path(const char* path_a, const char* path_b)
	{
		FILETIME time_a = { 0 };
		FILETIME time_b = { 0 };
		WIN32_FILE_ATTRIBUTE_DATA data;

		if (GetFileAttributesExA(path_a, GetFileExInfoStandard, &data)) time_a = data.ftLastWriteTime;
		if (GetFileAttributesExA(path_b, GetFileExInfoStandard, &data)) time_b = data.ftLastWriteTime;
		return CompareFileTime(&time_a, &time_b);
	}

	int cf_get_file_time(const char* path, cf_time_t* time)
	{
		FILETIME initialized_to_zero = { 0 };
		time->time = initialized_to_zero;
		WIN32_FILE_ATTRIBUTE_DATA data;
		if (GetFileAttributesExA(path, GetFileExInfoStandard, &data))
		{
			time->time = data.ftLastWriteTime;
			return 1;
		}
		return 0;
	}

	int cf_compare_file_times(cf_time_t* time_a, cf_time_t* time_b)
	{
		return CompareFileTime(&time_a->time, &time_b->time);
	}

	int cf_file_exists(const char* path)
	{
		WIN32_FILE_ATTRIBUTE_DATA unused;
		return GetFileAttributesExA(path, GetFileExInfoStandard, &unused);
	}

#elif CUTE_FILES_PLATFORM == CUTE_FILES_MAC || CUTE_FILES_PLATFORM == CUTE_FILES_UNIX

	int cf_read_file(cf_dir_t* dir, cf_file_t* file)
	{
		CUTE_FILES_ASSERT(dir->entry);

		int n = 0;
		char* fpath = file->path;
		char* dpath = dir->path;

		n = cf_safe_strcpy(fpath, dpath, 0, CUTE_FILES_MAX_PATH);
		n = cf_safe_strcpy(fpath, "/", n - 1, CUTE_FILES_MAX_PATH);

		char* dname = dir->entry->d_name;
		char* fname = file->name;

		cf_safe_strcpy(fname, dname, 0, CUTE_FILES_MAX_FILENAME);
		cf_safe_strcpy(fpath, fname, n - 1, CUTE_FILES_MAX_PATH);

		if (stat(file->path, &file->info))
			return 0;

		file->size = file->info.st_size;
		cf_get_ext(file);

		file->is_dir = S_ISDIR(file->info.st_mode);
		file->is_reg = S_ISREG(file->info.st_mode);

		return 1;
	}

	void cf_dir_next(cf_dir_t* dir)
	{
		CUTE_FILES_ASSERT(dir->has_next);
		dir->entry = readdir(dir->dir);
		dir->has_next = dir->entry ? 1 : 0;
	}

	void cf_dir_close(cf_dir_t* dir)
	{
		dir->path[0] = 0;
		if (dir->dir) closedir(dir->dir);
		dir->dir = 0;
		dir->has_next = 0;
		dir->entry = 0;
	}

	int cf_dir_open(cf_dir_t* dir, const char* path)
	{
		cf_safe_strcpy(dir->path, path, 0, CUTE_FILES_MAX_PATH);
		dir->dir = opendir(path);

		if (!dir->dir)
		{
			printf("ERROR: Failed to open directory (%s): %s.\n", path, strerror(errno));
			cf_dir_close(dir);
			CUTE_FILES_ASSERT(0);
			return 0;
		}

		dir->has_next = 1;
		dir->entry = readdir(dir->dir);
		if (!dir->dir) dir->has_next = 0;

		return 1;
	}

	// Warning : untested code! (let me know if it breaks)
	int cf_compare_file_times_by_path(const char* path_a, const char* path_b)
	{
		time_t time_a;
		time_t time_b;
		struct stat info;
		if (stat(path_a, &info)) return 0;
		time_a = info.st_mtime;
		if (stat(path_b, &info)) return 0;
		time_b = info.st_mtime;
		return (int)difftime(time_a, time_b);
	}

	// Warning : untested code! (let me know if it breaks)
	int cf_get_file_time(const char* path, cf_time_t* time)
	{
		struct stat info;
		if (stat(path, &info)) return 0;
		time->time = info.st_mtime;
		return 1;
	}

	// Warning : untested code! (let me know if it breaks)
	int cf_compare_file_times(cf_time_t* time_a, cf_time_t* time_b)
	{
		return (int)difftime(time_a->time, time_b->time);
	}

	// Warning : untested code! (let me know if it breaks)
	int cf_file_exists(const char* path)
	{
		return access(path, F_OK) != -1;
	}

#endif // CUTE_FILES_PLATFORM

#endif // CUTE_FILES_IMPLEMENTATION_ONCE
#endif // CUTE_FILES_IMPLEMENTATION

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
