/*
	tinyutf.h v1.0

	To create implementation (the function definitions)
		#define TINYUTF_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	SUMMARY:
	tinyutf is primarily for the use of utf8 encoding. There is a great website
	called http://utf8everywhere.org/ that contains a lot of great info. The
	gist is that utf8 is quite superior to all other encodings and should be
	used for pretty much everything all the time.

	This header is mostly useful for applications that want to localize or become
	utf8-aware, or to future proof code. This header was especially designed to
	facilitate game localization.

	EXTRA INFO:
	Some advantages of utf8:
	* Can encode anything utf32 can encode
	* Is very widely used on the web and everywhere else
	* Completely backwards compatible with typical ASCII
	* Operations in utf8 buffers can be coded naively as if it were a regular
	  old ASCII buffer
	* utf8 buffers can always be treated as a naive byte buffer making the utf8
	  encoding scheme safe as an opaque data type
	* Endianness independent
	* Lexicographic sorting is identical to utf32 sorting

	Unfortunately when dealing with Windows functions many APIs accept utf16
	encodings in the form of wchar_t. As a response this header provides some
	utilities for on-the-fly conversions to use utf16. The utf8everwhere site
	makes some great arguments to defend this style, so I highly recommend
	reading it if anyone cares.

	CONVERSIONS:
	To convert from utf8 to utf16 use tuDecode8, and pass the codepoint into the
	tuEncode16 function.

	As additional API there are tuWiden and tuNarrow funcs to operate on arrays.
	tuWiden converts utf8 to wchar_t (utf16) and tuNarrow converts utf16 back to
	utf8. Typically these helpers are to facilitate Windows APIs (pretty much
	what MultiByteToWideChar was for). Each function takes as input the size of
	the utf8 array, since this size should always be known. Just size the wide
	array to worst case size (2x number of bytes).

	One last trick on Windows is for using utf8 strings to for dealing with file
	paths. Since utf8 strings cannot be forwarded directly to fopen (or similar
	functions) GetShortPathName should be used. On other platforms symlinks can
	be utilized.

	Here is a post about this header:
	http://www.randygaul.net/2017/02/23/game-localization-and-utf-8/

	CREDIT:
	The utf8 encoder/decoder was written by Richard Mitton for his lovely tigr
	library. His functions have been tenderly extracted and have found a new
	home in this header; they form the most important part of this header so
	a big thanks goes to Mitton for kindly releasing his code to public domain!
*/

#if !defined( TINY_UTF_H )
#define TINY_UTF_H

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <wchar.h>

// reads text and will return text + 1..4 depending on the decoded code point
// the code point is stored in cp
const char* tuDecode8( const char* text, int* cp );

// reads text and will return text + 1..4 depending on code point
char *tuEncode8( char *text, int cp );

// reads text and will return text + 1..2 depending on decoded code point
// the code point is stored in cp
const wchar_t* tuDecode16( const wchar_t* text, int* cp );

// reads text and will return text + 1..2 depending on code point
wchar_t* tuEncode16( wchar_t* text, int cp );

// converts utf8 `in` to "wide" format utf16 `out`
void tuWiden( const char* in, wchar_t* out );

// converts utf16 `in` to "wide" format utf8 `out`
void tuShorten( const wchar_t* in, char* out );

// additional overloads
#ifdef __cplusplus
void tuWiden( const char* in, int in_len, wchar_t* out );
void tuWiden( const char* in, wchar_t* out, int out_len );
void tuWiden( const char* in, int in_len, wchar_t* out, int out_len );
#endif

#ifdef __cplusplus
void tuShorten( const wchar_t* in, int in_len, char* out );
void tuShorten( const wchar_t* in, char* out, int out_len );
void tuShorten( const wchar_t* in, int in_len, char* out, int out_len );
#endif

#endif

#if defined( TINYUTF_IMPLEMENTATION )

// utf8 functions created from RFC-3629

const char* tuDecode8( const char* text, int* cp )
{
	unsigned char c = *text++;
	int extra = 0, min = 0;
	*cp = 0;
	     if (c >= 0xF0) { *cp = c & 0x07; extra = 3; min = 0x10000; }
	else if (c >= 0xE0) { *cp = c & 0x0F; extra = 2; min = 0x800; }
	else if (c >= 0xC0) { *cp = c & 0x1F; extra = 1; min = 0x80; }
	else if (c >= 0x80) { *cp = 0xFFFD; }
	else *cp = c;
	while ( extra-- )
	{
		c = *text++;
		if ((c & 0xC0) != 0x80) { *cp = 0xFFFD; break; }
		(*cp) = ((*cp) << 6) | (c & 0x3F);
	}
	if ( *cp < min ) *cp = 0xFFFD;
	return text;
}

char *tuEncode8( char *text, int cp )
{
	if ( cp < 0 || cp > 0x10FFFF ) cp = 0xFFFD;

#define TU_EMIT( X, Y, Z ) *text++ = X | ((cp >> Y) & Z)
	     if (cp <    0x80) { TU_EMIT(0x00,0,0x7F); }
	else if (cp <   0x800) { TU_EMIT(0xC0,6,0x1F); TU_EMIT(0x80, 0, 0x3F); }
	else if (cp < 0x10000) { TU_EMIT(0xE0,12,0xF); TU_EMIT(0x80, 6, 0x3F); TU_EMIT(0x80, 0, 0x3F); }
	else                   { TU_EMIT(0xF0,18,0x7); TU_EMIT(0x80, 12, 0x3F); TU_EMIT(0x80, 6, 0x3F); TU_EMIT(0x80, 0, 0x3F); }
	return text;
}

// utf16 functions created via RFC-2781

const wchar_t* tuDecode16( const wchar_t* text, int* cp )
{
	int in = *text++;
	if ( in < 0xD800 || in > 0xDFFF ) *cp = in;
	else if ( in > 0xD800 && in < 0xDBFF ) *cp = ((in & 0x03FF) << 10) | (*text++ & 0x03FF);
	else *cp = 0xFFFD;
	return text;
}

wchar_t* tuEncode16( wchar_t* text, int cp )
{
	if ( cp < 0x10000 ) *text++ = cp;
	else
	{
		cp -= 0x10000;
		*text++ = 0xD800 | ((cp >> 10) & 0x03FF);
		*text++ = 0xDC00 | (cp & 0x03FF);
	}
	return text;
}

void tuWiden( const char* in, wchar_t* out )
{
	int cp;
	while ( *in )
	{
		in = tuDecode8( in, &cp );
		out = tuEncode16( out, cp );
	}
}

#ifdef __cplusplus
void tuWiden( const char* in, int in_len, wchar_t* out )
{
	const char* original = in;
	int cp;
	while ( in < original + in_len )
	{
		in = tuDecode8( in, &cp );
		out = tuEncode16( out, cp );
	}
}

void tuWiden( const char* in, wchar_t* out, int out_len )
{
	wchar_t* original = out;
	int cp;
	while ( *in && (out < original + out_len) )
	{
		in = tuDecode8( in, &cp );
		out = tuEncode16( out, cp );
	}
}

void tuWiden( const char* in, int in_len, wchar_t* out, int out_len )
{
	const char* original_in = in;
	wchar_t* original_out = out;
	int cp;
	while ( (in < original_in + in_len) && (out < original_out + out_len) )
	{
		in = tuDecode8( in, &cp );
		out = tuEncode16( out, cp );
	}
}
#endif

void tuShorten( const wchar_t* in, char* out )
{
	int cp;
	while ( *in )
	{
		in = tuDecode16( in, &cp );
		out = tuEncode8( out, cp );
	}
}

#ifdef __cplusplus
void tuShorten( const wchar_t* in, int in_len, char* out )
{
	const wchar_t* original = in;
	int cp;
	while ( in < original + in_len )
	{
		in = tuDecode16( in, &cp );
		out = tuEncode8( out, cp );
	}
}

void tuShorten( const wchar_t* in, char* out, int out_len )
{
	char* original = out;
	int cp;
	while ( *in && (out < original + out_len) )
	{
		in = tuDecode16( in, &cp );
		out = tuEncode8( out, cp );
	}
}

void tuShorten( const wchar_t* in, int in_len, char* out, int out_len )
{
	const wchar_t* original_in = in;
	char* original_out = out;
	int cp;
	while ( (in < original_in + in_len) && (out < original_out + out_len) )
	{
		in = tuDecode16( in, &cp );
		out = tuEncode8( out, cp );
	}
}
#endif

#endif // TINYUTF_IMPLEMENTATION

/*
	This is free and unencumbered software released into the public domain.
	Our intent is that anyone is free to copy and use this software,
	for any purpose, in any form, and by any means.
	The authors dedicate any and all copyright interest in the software
	to the public domain, at their own expense for the betterment of mankind.
	The software is provided "as is", without any kind of warranty, including
	any implied warranty. If it breaks, you get to keep both pieces.
*/
