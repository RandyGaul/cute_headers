/*
	tinyfiles.h - v1.0

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

// change to 0 to compile out any debug checks
#define TF_DEBUG_CHECKS 1

#if TF_DEBUG_CHECKS

	#include <stdio.h>  // printf
	#include <assert.h> // assert
	#include <string.h> // strerror
	#include <errno.h>
	#define TF_ASSERT assert
	
#else

	#define TF_ASSERT( ... )

#endif // TF_DEBUG_CHECKS

#define TF_WINDOWS	1
#define TF_MAC		2
#define TF_UNIX		3

#if defined( _WIN32 )
	#define TF_PLATFORM TF_WINDOWS
#elif defined( __APPLE__ )
	#define TF_PLATFORM TF_MAC
#else
	#define TF_PLATFORM TF_UNIX
#endif

#define TF_MAX_PATH 1024
#define TF_MAX_FILENAME 256
#define TF_MAX_EXT 32

struct tfFILE;
struct tfDIR;
typedef struct tfFILE tfFILE;
typedef struct tfDIR tfDIR;
typedef void (*tfCallback)( tfFILE* file, void* udata );

// Stores the file extension in tfFILE::ext
const char* tfGetExt( tfFILE* file );

// Applies a function (cb) to all files in a directory. Will recursively visit
// all subdirectories. Useful for asset management, file searching, indexing, etc.
void tfTraverse( const char* path, tfCallback cb, void* udata );

// Fills out a tfFILE struct with file information. Does not actually open the
// file contents, and instead performs more lightweight OS-specific calls.
int tfReadFile( tfDIR* dir, tfFILE* file );

// Once a tfDIR is opened, this function can be used to grab another directory
// from the operating system.
void tfDirNext( tfDIR* dir );

// Performs lightweight OS-specific call to close internal handle.
void tfDirClose( tfDIR* dir );

// Performs lightweight OS-specific call to open a file handle on a directory.
int tfDirOpen( tfDIR* dir, const char* path );

#if TF_PLATFORM == TF_WINDOWS

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
	
#elif TF_PLATFORM == TF_MAC || TN_PLATFORM == TN_UNIX

	#include <sys/stat.h>
	#include <dirent.h>

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

#endif

#define TINYFILES_H
#endif

#ifdef TINYFILES_IMPL

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

		file->size = ((size_t)dir->fdata.nFileSizeHigh * ((size_t)MAXDWORD + 1)) + (size_t)dir->fdata.nFileSizeLow;
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

#elif TF_PLATFORM == TF_MAC || TN_PLATFORM == TN_UNIX

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

#endif // TN_PLATFORM

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
