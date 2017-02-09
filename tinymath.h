#if !defined( TINYMATH_H )

/*
	tinymath - v1.0

	SUMMARY:
		A professional level implementation of SIMD intrinsics, suitable for creating high
		performance output. Please make sure to minimize your loads and stores!

		The __vectorcall convention needs to be used to build on MSVC. Set the flag /Gv to
		setup this convention by default (recommended). This flag will not affect class
		methods, so annotate methods with TM_VCALL as appropriate. Note that when compiling
		and linking to dynamic libs, older libs are probably not aware of __vectorcall and
		will need __cdecl (this is what TM_CDECL is for). Statically linking will bring
		no problems, as __vectorcall will be applied to the statically linked lib.

		This header is not particularly customized for general graphics programming since
		there are no functions implemented here for 4x4 matrices. Personally I never use
		4x4 matrices and instead prefer to represent affine transormations in block form:
		Ax + b, where A is a 3x3 rotation matrix (and possibly scale), and b performs the
		affine translation. A 4x4 matrix would store an additional row of { 0, 0, 0, 1 },
		so in most cases this bottom row is wasted anyways. This is all my own preference
		so feel free to adjust the header and add in 4x4 matrix routines as desired.

	Some tips for writing efficient SIMD code:
		SIMD operates on XMM (128 bit) and YMM (256 bit) registers. There aren't very many
		of these registers. The functions in this file pass arguments by value in an attempt
		to keep values in registers at all times.

		This means that when writing SIMD code tighter (no branching) loops without a whole
		lot of different variables are preffered. Split larger loops into smaller individual
		loops to relieve register pressure when appropriate.

		Loading and storing (load/store, or _mm_set_ps) is very inefficient. Good SIMD code will
		load and store a single time while performing many SIMD operations in between. If done
		properly a 3-4x speedup should arise in most cases.

		Casting between the FPU and SIMD registers is expensive. Often times it's best to
		store a single float inside an entire vector (same value in each component). A single
		_mm_mul_ps operation between two vectors can emulate this FPU operation: Vector *
		Scalar.

		Try to use SIMD comparison functions to create a comparison mask. This mask can be
		used with select() to choose between one of two vectors. This lets the user avoid
		branching in trade for extra computations. Here's a link to the list of SSE comparison
		intrinsics: https://msdn.microsoft.com/en-us/library/w8kez9sf%28v=vs.90%29.aspx

	These links were referenced during the creation of this code:
		http://www.gamasutra.com/view/feature/4248/designing_fast_crossplatform_simd_.php
		https://msdn.microsoft.com/en-us/library/windows/desktop/hh437833%28v=vs.85%29.aspx
		http://box2d.org/2012/08/__m128/
		https://msdn.microsoft.com/en-us/library/dn375768.aspx
		http://www.codersnotes.com/notes/maths-lib-2016/
*/

#include <stdint.h>
#include <xmmintrin.h>

#if 1
	#include <assert.h>
	#define TM_ASSERT assert
#else
	#define TM_ASSERT( ... )
#endif

#define TM_SHUFFLE( a, b, x, y, z ) _mm_shuffle_ps( a, b, _MM_SHUFFLE( 3, z, y, x ) )
#define TM_CDECL __cdecl

#ifdef _WIN32

	#define TM_INLINE __forceinline
	#define TM_SELECTANY extern const __declspec( selectany )

#else

	#define TM_INLINE __attribute__((always_inline))
	#define TM_SELECTANY extern const __attribute__((selectany))

#endif

struct v3
{
	TM_INLINE v3( ) { }
	TM_INLINE explicit v3( float x, float y, float z ) { m = _mm_set_ps( 0, z, y, x ); }
	TM_INLINE explicit v3( float a ) { m = _mm_set_ps( 0, a, a, a ); }
	TM_INLINE explicit v3( float *a ) { m = _mm_set_ps( 0, a[ 0 ], a[ 1 ], a[ 2 ] ); }

	// seems a lil sketch? can get rid of these a la Mitton
	TM_INLINE v3( __m128 v ) { m = v; }
	TM_INLINE operator __m128( ) { return m; }
	TM_INLINE operator __m128( ) const { return m; }

	__m128 m;
};

// try not to use these if possible, instead use splat
TM_INLINE float getx( v3 a ) { return _mm_cvtss_f32( a ); }
TM_INLINE float gety( v3 a ) { return _mm_cvtss_f32( TM_SHUFFLE( a, a, 1, 1, 1 ) ); }
TM_INLINE float getz( v3 a ) { return _mm_cvtss_f32( TM_SHUFFLE( a, a, 2, 2, 2 ) ); }

TM_INLINE v3 splatx( v3 a ) { return TM_SHUFFLE( a, a, 0, 0, 0 ); }
TM_INLINE v3 splaty( v3 a ) { return TM_SHUFFLE( a, a, 1, 1, 1 ); }
TM_INLINE v3 splatz( v3 a ) { return TM_SHUFFLE( a, a, 2, 2, 2 ); }

struct m3
{
	TM_INLINE v3 operator[]( int i )
	{
		switch ( i )
		{
			case 0: return x;
			case 1: return y;
			case 2: return z;
			default: TM_ASSERT( 0 ); return x;
		}
	}

	v3 x;
	v3 y;
	v3 z;
};

TM_INLINE m3 rows( v3 x, v3 y, v3 z )
{
	m3 m;
	m.x = x;
	m.y = y;
	m.z = z;
	return m;
}

// helpers for static data
struct v3_consti
{
	union { uint32_t i[ 4 ]; __m128 m; };
	TM_INLINE operator v3( ) const { return v3( m ); }
	TM_INLINE operator __m128( ) const { m; }
};

struct v3_constf
{
	union { float f[ 4 ]; __m128 m; };
	TM_INLINE operator v3( ) const { return v3( m ); }
	TM_INLINE operator __m128( ) const { m; }
};

TM_SELECTANY v3_consti tmMaskSign = { 0x80000000, 0x80000000, 0x80000000, 0x80000000 };
TM_SELECTANY v3_consti tmMaskAllBits = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
TM_SELECTANY v3_constf tmMaskBasis = { 0.57735027f, 0.57735027f, 0.57735027f };

// the binary ops
TM_INLINE v3 operator+( v3 a, v3 b ) { return _mm_add_ps( a, b ); }
TM_INLINE v3 operator-( v3 a, v3 b ) { return _mm_sub_ps( a, b ); }
TM_INLINE v3 operator*( v3 a, v3 b ) { return _mm_mul_ps( a, b ); }
TM_INLINE v3 operator/( v3 a, v3 b ) { return _mm_div_ps( a, b ); }
TM_INLINE v3& operator+=( v3 &a, v3 b ) { a = a + b; return a; }
TM_INLINE v3& operator-=( v3 &a, v3 b ) { a = a - b; return a; }
TM_INLINE v3& operator*=( v3 &a, v3 b ) { a = a * b; return a; }
TM_INLINE v3& operator/=( v3 &a, v3 b ) { a = a / b; return a; }

// generally comparisons are followed up with a mask(v3) call (or any(v3))
TM_INLINE v3 operator==( v3 a, v3 b ) { return _mm_cmpeq_ps( a, b ); }
TM_INLINE v3 operator!=( v3 a, v3 b ) { return _mm_cmpneq_ps( a, b ); }
TM_INLINE v3 operator<( v3 a, v3 b ) { return _mm_cmplt_ps( a, b ); }
TM_INLINE v3 operator>( v3 a, v3 b ) { return _mm_cmpgt_ps( a, b ); }
TM_INLINE v3 operator<=( v3 a, v3 b ) { return _mm_cmple_ps( a, b ); }
TM_INLINE v3 operator>=( v3 a, v3 b ) { return _mm_cmpge_ps( a, b ); }
TM_INLINE v3 operator-( v3 a ) { return _mm_setzero_ps( ) - a; }
TM_INLINE unsigned mask( v3 a ) { return _mm_movemask_ps( a ) & 7; }
TM_INLINE int any( v3 a ) { return mask( a ) != 0; }
TM_INLINE int all( v3 a ) { return mask( a ) == 7; }

// generally avoid these next three functions
TM_INLINE v3 setx( v3 a, float x )
{
	v3 t0 = _mm_set_ss( x );
	return _mm_move_ss( a, t0 );
}

TM_INLINE v3 sety( v3 a, float y )
{
	v3 t0 = TM_SHUFFLE( a, a, 1, 0, 2 );
	v3 t1 = _mm_set_ss( y );
	v3 t2 = _mm_move_ss( t0, t1 );
	return TM_SHUFFLE( t2, t2, 1, 0, 2 );
}

TM_INLINE v3 setz( v3 a, float z )
{
	v3 t0 = TM_SHUFFLE( a, a, 2, 1, 0 );
	v3 t1 = _mm_set_ss( z );
	v3 t2 = _mm_move_ss( t0, t1 );
	return TM_SHUFFLE( t2, t2, 2, 1, 0 );
}

// try to avoid using these, but sometimes the convenience is just worth it
TM_INLINE v3 operator*( v3 a, float b ) { return _mm_mul_ps( a, _mm_set1_ps( b ) ); }
TM_INLINE v3 operator/( v3 a, float b ) { return _mm_div_ps( a, _mm_set1_ps( b ) ); }
TM_INLINE v3 operator*( float a, v3 b ) { return _mm_mul_ps( _mm_set1_ps( a ), b ); }
TM_INLINE v3 operator/( float a, v3 b ) { return _mm_div_ps( _mm_set1_ps( a ), b ); }
TM_INLINE v3& operator*=( v3 &a, float b ) { a = a * b; return a; }
TM_INLINE v3& operator/=( v3 &a, float b ) { a = a / b; return a; }

// f must be 16 byte aligned
TM_INLINE v3 load( float* f ) { return _mm_load_ps( f ); }
TM_INLINE void store( v3 v, float* f ) { _mm_store_ps( f, v ); }

TM_INLINE v3 dot( v3 a, v3 b )
{
	v3 t0 = _mm_mul_ps( a, b );
	v3 t1 = TM_SHUFFLE( t0, t0, 1, 0, 0 );
	v3 t2 = _mm_add_ss( t0, t1 );
	v3 t3 = TM_SHUFFLE( t2, t2, 2, 0, 0 );
	v3 t4 = _mm_add_ss( t2, t3 );
	return splatx( t4 );
}

TM_INLINE v3 cross( v3 a, v3 b )
{
	v3 t0 = TM_SHUFFLE( a, a, 1, 2, 0 );
	v3 t1 = TM_SHUFFLE( b, b, 2, 0, 1 );
	v3 t2 = _mm_mul_ps( t0, t1 );

	t0 = TM_SHUFFLE( t0, t0, 1, 2, 0 );
	t1 = TM_SHUFFLE( t1, t1, 2, 0, 1 );
	t0 = _mm_mul_ps( t0, t1 );

	return _mm_sub_ps( t2, t0 );
}

TM_INLINE v3 lengthSq( v3 a ) { return dot( a, a ); }
TM_INLINE v3 sqrt( v3 a ) {	return _mm_sqrt_ps( a ); }
TM_INLINE v3 length( v3 a ) { return sqrt( dot( a, a ) ); }
TM_INLINE v3 abs( v3 a ) { return _mm_andnot_ps( tmMaskSign, a ); }
TM_INLINE v3 min( v3 a, v3 b ) { return _mm_min_ps( a, b ); }
TM_INLINE v3 max( v3 a, v3 b ) { return _mm_max_ps( a, b ); }
TM_INLINE v3 select( v3 a, v3 b, v3 mask ) { return _mm_xor_ps( a, _mm_and_ps( mask, _mm_xor_ps( b, a ) ) ); }
TM_INLINE v3 lerp( v3 a, v3 b, float t ) { return a + (b - a) * t; }
TM_INLINE v3 lerp( v3 a, v3 b, v3 t ) { return a + (b - a) * t; }

// for posterity
// TM_INLINE v3 lerp( v3 a, v3 b, float t ) { return a * (1.0f - t) + b * t; }
// TM_INLINE v3 lerp( v3 a, v3 b, v3 t ) { return a * (1.0f - t) + b * t; }

TM_INLINE float hmin( v3 a )
{
	v3 t0 = TM_SHUFFLE( a, a, 1, 0, 2 );
	a = min( a, t0 );
	v3 t1 = TM_SHUFFLE( a, a, 2, 0, 1 );
	return getx( min( a, t1 ) );
}

TM_INLINE float hmax( v3 a )
{
	v3 t0 = TM_SHUFFLE( a, a, 1, 0, 2 );
	a = max( a, t0 );
	v3 t1 = TM_SHUFFLE( a, a, 2, 0, 1 );
	return getx( max( a, t1 ) );
}

TM_INLINE v3 norm( v3 a )
{ 
	v3 t0 = dot( a, a );
	v3 t1 = _mm_sqrt_ps( t0 );
	v3 t2 = _mm_div_ps( a, t1 );
	return _mm_and_ps( t2, tmMaskAllBits );
}

TM_INLINE v3 clamp( v3 a, v3 vmin, v3 vmax )
{
	v3 t0 = _mm_max_ps( vmin, a );
	return _mm_min_ps( t0, vmax );
}

// sets up a mask of { x ? ~0 : 0, y ? ~0 : 0, z ? ~0 : 0 }
// x, y and z should be 0 or 1
TM_INLINE v3 mask( int x, int y, int z )
{
	v3_consti c;
	int elements[] = { 0x00000000, 0xFFFFFFFF };

	TM_ASSERT( x < 2 && x >= 0 );
	TM_ASSERT( y < 2 && y >= 0 );
	TM_ASSERT( z < 2 && z >= 0 );

	c.i[ 0 ] = elements[ x ];
	c.i[ 1 ] = elements[ y ];
	c.i[ 2 ] = elements[ z ];
	c.i[ 3 ] = elements[ 0 ];

	return c;
}

// Does not preserve 0.0f in w to get rid of extra shuffles.
// Oh well. Anyone have a better idea?
TM_INLINE m3 transpose( m3 a )
{
	v3 t0 = _mm_shuffle_ps( a.x, a.y, _MM_SHUFFLE( 1, 0, 1, 0 ) );
	v3 t1 = _mm_shuffle_ps( a.x, a.y, _MM_SHUFFLE( 2, 2, 2, 2 ) );
	v3 x = _mm_shuffle_ps( t0, a.z, _MM_SHUFFLE( 0, 0, 2, 0 ) );
	v3 y = _mm_shuffle_ps( t0, a.z, _MM_SHUFFLE( 0, 1, 3, 1 ) );
	v3 z = _mm_shuffle_ps( t1, a.z, _MM_SHUFFLE( 0, 2, 2, 0 ) );

	a.x = x;
	a.y = y;
	a.z = z;

	return a;
}

//  a * b
TM_INLINE v3 mul( m3 a, v3 b )
{
	v3 x = splatx( b );
	v3 y = splaty( b );
	v3 z = splatz( b );
	x = _mm_mul_ps( x, a.x );
	y = _mm_mul_ps( y, a.y );
	z = _mm_mul_ps( z, a.z );
	v3 t0 = _mm_add_ps( x, y );
	return _mm_add_ps( t0, z );
}

// a^T * b
TM_INLINE v3 mulT( m3 a, v3 b ) { mul( transpose( a ), b ); }

// a * b
TM_INLINE m3 mul( m3 a, m3 b )
{
	v3 x = mul( a, b.x );
	v3 y = mul( a, b.y );
	v3 z = mul( a, b.z );
	return rows( x, y, z );
}

// a^T * b
TM_INLINE m3 mulT( m3 a, m3 b ) { return mul( transpose( a ), b ); }

// http://box2d.org/2014/02/computing-a-basis/
TM_INLINE m3 basis( v3 a )
{
	// Suppose vector a has all equal components and is a unit vector: a = (s, s, s)
	// Then 3*s*s = 1, s = sqrt(1/3) = 0.57735027. This means that at least one component
	// of a unit vector must be greater or equal to 0.57735027.

	v3 negA = -a;
	v3 t0 = TM_SHUFFLE( a, negA, 1, 1, 0 );
	v3 b0 = TM_SHUFFLE( t0, t0, 0, 2, 3 );
	t0 = TM_SHUFFLE( a, negA, 2, 2, 1 );
	v3 b1 = TM_SHUFFLE( t0, t0, 3, 1, 2 );

	v3 mask = _mm_cmpge_ps( tmMaskBasis, abs( a ) );
	mask = splatx( mask );
	v3 b = select( b0, b1, mask );
	b = norm( b );
	v3 c = cross( a, b );

	return rows( a, b, c );
}

struct transform
{
	v3 p; // position
	m3 r; // rotation
};

TM_INLINE v3 mul( transform tx, v3 a ) { return mul( tx.r, a ) + tx.p; }
TM_INLINE v3 mulT( transform tx, v3 a ) { return mul( tx.r, a - tx.p ); }

TM_INLINE transform mul( transform a, transform b )
{
	transform c;
	c.p = mul( a.r, b.p ) + a.p;
	c.r = mul( a.r, b.r );
	return c;
}

TM_INLINE transform mulT( transform a, transform b )
{
	transform c;
	c.p = mulT( a.r, b.p - a.p );
	c.r = mulT( a.r, b.r );
	return c;
}

struct halfspace
{
	v3 n;
	v3 d;
};

TM_INLINE v3 origin( halfspace h ) { return h.n * h.d; }
TM_INLINE v3 distance( halfspace h, v3 p ) { return dot( h.n, p ) - h.d; }
TM_INLINE v3 projected( halfspace h, v3 p ) { return p - h.n * distance( h, p ); }

TM_INLINE halfspace mul( transform a, halfspace b )
{
	v3 o = origin( b );
	o = mul( a, o );
	v3 normal = mul( a.r, b.n );
	halfspace c;
	c.n = normal;
	c.d = dot( o, normal );
	return c;
}

TM_INLINE halfspace mulT( transform a, halfspace b )
{
	v3 o = origin( b );
	o = mulT( a, o );
	v3 normal = mulT( a.r, b.n );
	halfspace c;
	c.n = normal;
	c.d = dot( o, normal );
	return c;
}

// da and db should be distances to plane, i.e. halfspace::distance
TM_INLINE v3 intersect( v3 a, v3 b, v3 da, v3 db )
{
	return a + (b - a) * (da / (da - db));
}

// carefully choose kTol, see: http://www.randygaul.net/2014/11/07/robust-parallel-vector-test/
TM_INLINE int parallel( v3 a, v3 b, float kTol )
{
	v3 k = length( a ) / length( b );
	v3 bk = b * k;
	if ( all( abs( a - bk ) < v3( kTol ) ) ) return 1;
	return 0;
}

TM_INLINE m3 outer( v3 u, v3 v )
{
	v3 a = v * splatx( u );
	v3 b = v * splaty( u );
	v3 c = v * splatz( u );
	return rows( a, b, c );
}

#define TINYMATH_H
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
