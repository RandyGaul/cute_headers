/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	tinyfiles.h - v1.0

	To create implementation (the function definitions)
		#define TINYFILES_IMPLEMENTATION
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
		tfDIR dir;
		tfDirOpen( &dir, "a" );

		while ( dir.has_next )
		{
			tfFILE file;
			tfReadFile( &dir, &file );
			printf( "%s\n", file.name );
			tfDirNext( &dir );
		}

		tfDirClose( &dir );
*/

#if !defined( TINYFILES_H )

#define TF_WINDOWS	1
#define TF_MAC		2
#define TF_UNIX		3

#if defined( _WIN32 )
	#define TF_PLATFORM TF_WINDOWS
	#if !defined( _CRT_SECURE_NO_WARNINGS )
	#define _CRT_SECURE_NO_WARNINGS
	#endif
#elif defined( __APPLE__ )
	#define TF_PLATFORM TF_MAC
#else
	#define TF_PLATFORM TF_UNIX
#endif

#include <string.h> // strerror, strncpy

// change to 0 to compile out any debug checks
#define TF_DEBUG_CHECKS 1

#if TF_DEBUG_CHECKS

	#include <stdio.h>  // printf
	#include <assert.h> // assert
	#include <errno.h>
	#define TF_ASSERT assert
	
#else

	#define TF_ASSERT( ... )

#endif // TF_DEBUG_CHECKS

#define TF_MAX_PATH 1024
#define TF_MAX_FILENAME 256
#define TF_MAX_EXT 32

struct tfFILE;
struct tfDIR;
struct tfFILETIME;
typedef struct tfFILE tfFILE;
typedef struct tfDIR tfDIR;
typedef struct tfFILETIME tfFILETIME;
typedef void (*tfCallback)( tfFILE* file, void* udata );

// Stores the file extension in tfFILE::ext, and returns a pointer to
// tfFILE::ext
const char* tfGetExt( tfFILE* file );

// Applies a function (cb) to all files in a directory. Will recursively visit
// all subdirectories. Useful for asset management, file searching, indexing, etc.
void tfTraverse( const char* path, tfCallback cb, void* udata );

// Fills out a tfFILE struct with file information. Does not actually open the
// file contents, and instead performs more lightweight OS-specific calls.
int tfReadFile( tfDIR* dir, tfFILE* file );

// Once a tfDIR is opened, this function can be used to grab another file
// from the operating system.
void tfDirNext( tfDIR* dir );

// Performs lightweight OS-specific call to close internal handle.
void tfDirClose( tfDIR* dir );

// Performs lightweight OS-specific call to open a file handle on a directory.
int tfDirOpen( tfDIR* dir, const char* path );

// Compares file last write times. -1 if file at path_a was modified earlier than path_b.
// 0 if they are equal. 1 if file at path_b was modified earlier than path_a.
int tfCompareFileTimesByPath( const char* path_a, const char* path_b );

// Retrieves time file was last modified, returns 0 upon failure
int tfGetFileTime( const char* path, tfFILETIME* time );

// Compares file last write times. -1 if time_a was modified earlier than path_b.
// 0 if they are equal. 1 if time_b was modified earlier than path_a.
int tfCompareFileTimes( tfFILETIME* time_a, tfFILETIME* time_b );

// Returns 1 of file exists, otherwise returns 0.
int tfFileExists( const char* path );

// Returns 1 if the file's extension matches the string in ext
// Returns 0 otherwise
int tfMatchExt( tfFILE* file, const char* ext );

// Prints detected errors to stdout
void tfDoUnitTests();

#if TF_PLATFORM == TF_WINDOWS

#if !defined _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <Windows.h>

	struct tfFILE
	{
		char path[ TF_MAX_PATH ];
		char name[ TF_MAX_FILENAME ];
		char ext[ TF_MAX_EXT ];
		int is_dir;
		int is_reg;
		size_t size;
	};

	struct tfDIR
	{
		char path[ TF_MAX_PATH ];
		int has_next;
		HANDLE handle;
		WIN32_FIND_DATAA fdata;
	};

	struct tfFILETIME
	{
		FILETIME time;
	};

#elif TF_PLATFORM == TF_MAC || TF_PLATFORM == TF_UNIX

	#include <sys/stat.h>
	#include <dirent.h>
	#include <unistd.h>
    #include <time.h>

	struct tfFILE
	{
		char path[ TF_MAX_PATH ];
		char name[ TF_MAX_FILENAME ];
		char ext[ TF_MAX_EXT ];
		int is_dir;
		int is_reg;
		int size;
		struct stat info;
	};

	struct tfDIR
	{
		char path[ TF_MAX_PATH ];
		int has_next;
		DIR* dir;
		struct dirent* entry;
	};

	struct tfFILETIME
	{
		time_t time;
	};

#endif

#define TINYFILES_H
#endif

#ifdef TINYFILES_IMPLEMENTATION
#undef TINYFILES_IMPLEMENTATION

#define tfSafeStrCpy( dst, src, n, max ) tfSafeStrCopy_internal( dst, src, n, max, __FILE__, __LINE__ )
static int tfSafeStrCopy_internal( char* dst, const char* src, int n, int max, const char* file, int line )
{
	int c;
	const char* original = src;

	do
	{
		if ( n >= max )
		{
			if ( !TF_DEBUG_CHECKS ) break;
			printf( "ERROR: String \"%s\" too long to copy on line %d in file %s (max length of %d).\n"
				, original
				, line
				, file
				, max );
			TF_ASSERT( 0 );
		}

		c = *src++;
		dst[ n ] = c;
		++n;
	} while ( c );

	return n;
}

const char* tfGetExt( tfFILE* file )
{
	char* period = file->name;
	char c;
	while ( (c = *period++) ) if ( c == '.' ) break;
	if ( c ) tfSafeStrCpy( file->ext, period, 0, TF_MAX_EXT );
	else file->ext[ 0 ] = 0;
	return file->ext;
}

void tfTraverse( const char* path, tfCallback cb, void* udata )
{
	tfDIR dir;
	tfDirOpen( &dir, path );

	while ( dir.has_next )
	{
		tfFILE file;
		tfReadFile( &dir, &file );

		if ( file.is_dir && file.name[ 0 ] != '.' )
		{
			char path2[ TF_MAX_PATH ];
			int n = tfSafeStrCpy( path2, path, 0, TF_MAX_PATH );
			n = tfSafeStrCpy( path2, "/", n - 1, TF_MAX_PATH );
			tfSafeStrCpy( path2, file.name, n -1, TF_MAX_PATH );
			tfTraverse( path2, cb, udata );
		}

		if ( file.is_reg ) cb( &file, udata );
		tfDirNext( &dir );
	}

	tfDirClose( &dir );
}

int tfMatchExt( tfFILE* file, const char* ext )
{
	if (*ext == '.') ++ext;
	return !strcmp( file->ext, ext );
}

#if TF_PLATFORM == TF_WINDOWS

	int tfReadFile( tfDIR* dir, tfFILE* file )
	{
		TF_ASSERT( dir->handle != INVALID_HANDLE_VALUE );

		int n = 0;
		char* fpath = file->path;
		char* dpath = dir->path;

		n = tfSafeStrCpy( fpath, dpath, 0, TF_MAX_PATH );
		n = tfSafeStrCpy( fpath, "/", n - 1, TF_MAX_PATH );

		char* dname = dir->fdata.cFileName;
		char* fname = file->name;

		tfSafeStrCpy( fname, dname, 0, TF_MAX_FILENAME );
		tfSafeStrCpy( fpath, fname, n - 1, TF_MAX_PATH );

		size_t max_dword = MAXDWORD;
		file->size = ((size_t)dir->fdata.nFileSizeHigh * (max_dword + 1)) + (size_t)dir->fdata.nFileSizeLow;
		tfGetExt( file );

		file->is_dir = !!(dir->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
		file->is_reg = !!(dir->fdata.dwFileAttributes & FILE_ATTRIBUTE_NORMAL) ||
			!(dir->fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);

		return 1;
	}

	void tfDirNext( tfDIR* dir )
	{
		TF_ASSERT( dir->has_next );

		if ( !FindNextFileA( dir->handle, &dir->fdata ) )
		{
			dir->has_next = 0;
			DWORD err = GetLastError( );
			TF_ASSERT( err == ERROR_SUCCESS || err == ERROR_NO_MORE_FILES );
		}
	}

	void tfDirClose( tfDIR* dir )
	{
		dir->path[ 0 ] = 0;
		dir->has_next = 0;
		if ( dir->handle != INVALID_HANDLE_VALUE ) FindClose( dir->handle );
	}

	int tfDirOpen( tfDIR* dir, const char* path )
	{
		int n = tfSafeStrCpy( dir->path, path, 0, TF_MAX_PATH );
		n = tfSafeStrCpy( dir->path, "\\*", n - 1, TF_MAX_PATH );
		dir->handle = FindFirstFileA( dir->path, &dir->fdata );
		dir->path[ n - 3 ] = 0;

		if ( dir->handle == INVALID_HANDLE_VALUE )
		{
			printf( "ERROR: Failed to open directory (%s): %s.\n", path, strerror( errno ) );
			tfDirClose( dir );
			TF_ASSERT( 0 );
			return 0;
		}

		dir->has_next = 1;

		return 1;
	}

	int tfCompareFileTimesByPath( const char* path_a, const char* path_b )
	{
		FILETIME time_a = { 0 };
		FILETIME time_b = { 0 };
		WIN32_FILE_ATTRIBUTE_DATA data;

		if ( GetFileAttributesExA( path_a, GetFileExInfoStandard, &data ) ) time_a = data.ftLastWriteTime;
		if ( GetFileAttributesExA( path_b, GetFileExInfoStandard, &data ) ) time_b = data.ftLastWriteTime;
		return CompareFileTime( &time_a, &time_b );
	}

	int tfGetFileTime( const char* path, tfFILETIME* time )
	{
		FILETIME initialized_to_zero = { 0 };
		time->time = initialized_to_zero;
		WIN32_FILE_ATTRIBUTE_DATA data;
		if ( GetFileAttributesExA( path, GetFileExInfoStandard, &data ) )
		{
			time->time = data.ftLastWriteTime;
			return 1;
		}
		return 0;
	}

	int tfCompareFileTimes( tfFILETIME* time_a, tfFILETIME* time_b )
	{
		return CompareFileTime( &time_a->time, &time_b->time );
	}

	int tfFileExists( const char* path )
	{
		WIN32_FILE_ATTRIBUTE_DATA unused;
		return GetFileAttributesExA( path, GetFileExInfoStandard, &unused );
	}

#elif TF_PLATFORM == TF_MAC || TF_PLATFORM == TF_UNIX

	int tfReadFile( tfDIR* dir, tfFILE* file )
	{
		TF_ASSERT( dir->entry );

		int n = 0;
		char* fpath = file->path;
		char* dpath = dir->path;

		n = tfSafeStrCpy( fpath, dpath, 0, TF_MAX_PATH );
		n = tfSafeStrCpy( fpath, "/", n - 1, TF_MAX_PATH );

		char* dname = dir->entry->d_name;
		char* fname = file->name;

		tfSafeStrCpy( fname, dname, 0, TF_MAX_FILENAME );
		tfSafeStrCpy( fpath, fname, n - 1, TF_MAX_PATH );

		if ( stat( file->path, &file->info ) )
			return 0;

		file->size = file->info.st_size;
		tfGetExt( file );

		file->is_dir = S_ISDIR( file->info.st_mode );
		file->is_reg = S_ISREG( file->info.st_mode );

		return 1;
	}

	void tfDirNext( tfDIR* dir )
	{
		TF_ASSERT( dir->has_next );
		dir->entry = readdir( dir->dir );
		dir->has_next = dir->entry ? 1 : 0;
	}

	void tfDirClose( tfDIR* dir )
	{
		dir->path[ 0 ] = 0;
		if ( dir->dir ) closedir( dir->dir );
		dir->dir = 0;
		dir->has_next = 0;
		dir->entry = 0;
	}

	int tfDirOpen( tfDIR* dir, const char* path )
	{
		tfSafeStrCpy( dir->path, path, 0, TF_MAX_PATH );
		dir->dir = opendir( path );

		if ( !dir->dir )
		{
			printf( "ERROR: Failed to open directory (%s): %s.\n", path, strerror( errno ) );
			tfDirClose( dir );
			TF_ASSERT( 0 );
			return 0;
		}

		dir->has_next = 1;
		dir->entry = readdir( dir->dir );
		if ( !dir->dir ) dir->has_next = 0;

		return 1;
	}

	// Warning : untested code! (let me know if it breaks)
	int tfCompareFileTimesByPath( const char* path_a, const char* path_b )
	{
		time_t time_a;
		time_t time_b;
		struct stat info;
		if ( stat( path_a, &info ) ) return 0;
		time_a = info.st_mtime;
		if ( stat( path_b, &info ) ) return 0;
		time_b = info.st_mtime;
		return (int)difftime( time_a, time_b );
	}

	// Warning : untested code! (let me know if it breaks)
	int tfGetFileTime( const char* path, tfFILETIME* time )
	{
		struct stat info;
		if ( stat( path, &info ) ) return 0;
		time->time = info.st_mtime;
		return 1;
	}

	// Warning : untested code! (let me know if it breaks)
	int tfCompareFileTimes( tfFILETIME* time_a, tfFILETIME* time_b )
	{
		return (int)difftime( time_a->time, time_b->time );
	}

	// Warning : untested code! (let me know if it breaks)
	int tfFileExists( const char* path )
	{
		return access( path, F_OK ) != -1;
	}

#endif // TF_PLATFORM

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
