#if !defined( CUTE_MATH_H )

/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_math.h - v1.1

	Revision history:
	1.0  (12/21/2016) initial release
	1.1  (10/05/2017) vfloat data type added, removed out-dated comments,
	                  added compute_mouse_ray, added more m3 ops, added lookAt


	SUMMARY:
		A professional level implementation of SIMD intrinsics, suitable for creating high
		performance output.

		The __vectorcall convention needs to be used to build on MSVC. Set the flag /Gv to
		setup this convention by default (recommended). This flag will not affect class
		methods, so annotate methods with CUTE_MATH_VCALL as appropriate. Note that when compiling
		and linking to dynamic libs, older libs are probably not aware of __vectorcall and
		will need __cdecl (this is what CUTE_MATH_CDECL is for). Statically linking will bring
		no problems, as __vectorcall will be applied to the statically linked lib.

		This header is not particularly customized for general graphics programming since
		there are no functions implemented here for 4x4 matrices. Personally I never use
		4x4 matrices and instead prefer to represent affine transormations in block form:
		Ax + b, where A is a 3x3 rotation matrix (and possibly scale), and b performs the
		affine translation. A 4x4 matrix would store an additional row of { 0, 0, 0, 1 },
		so in most cases this bottom row is wasted anyways. This is all my own preference
		so feel free to adjust the header and add in 4x4 matrix routines as desired.
*/

#include <stdint.h>
#include <xmmintrin.h>

#if 1
	#include <assert.h>
	#define CUTE_MATH_ASSERT assert
#else
	#define CUTE_MATH_ASSERT( ... )
#endif

#define CUTE_MATH_SHUFFLE( a, b, x, y, z ) _mm_shuffle_ps( a, b, _MM_SHUFFLE( 3, z, y, x ) )
#define CUTE_MATH_CDECL __cdecl
#define CUTE_MATH_VCALL __vectorcall

#ifdef _WIN32

	#define CUTE_MATH_INLINE __forceinline
	#define CUTE_MATH_SELECTANY extern const __declspec( selectany )

#else

	#define CUTE_MATH_INLINE __attribute__((always_inline))
	#define CUTE_MATH_SELECTANY extern const __attribute__((selectany))

#endif

struct v3
{
	CUTE_MATH_INLINE v3( ) { }
	CUTE_MATH_INLINE explicit v3( float x, float y, float z ) { m = _mm_set_ps( 0, z, y, x ); }
	CUTE_MATH_INLINE explicit v3( float a ) { m = _mm_set_ps( 0, a, a, a ); }
	CUTE_MATH_INLINE explicit v3( float *a ) { m = _mm_set_ps( 0, a[ 2 ], a[ 1 ], a[ 0 ] ); }
	CUTE_MATH_INLINE explicit v3( __m128 v ) { m = v; }
	CUTE_MATH_INLINE operator __m128( ) { return m; }
	CUTE_MATH_INLINE operator __m128( ) const { return m; }

	__m128 m;
};

struct vfloat
{
	CUTE_MATH_INLINE vfloat( ) { }
	CUTE_MATH_INLINE explicit vfloat( float a ) { m = _mm_set_ps( 0, a, a, a ); }
	CUTE_MATH_INLINE explicit vfloat( v3& a ) { m = _mm_shuffle_ps( a, a, _MM_SHUFFLE( 0, 0, 0, 0 ) ); }
	CUTE_MATH_INLINE explicit vfloat( __m128 v ) { m = v; }
	CUTE_MATH_INLINE operator __m128( ) { return m; }
	CUTE_MATH_INLINE operator __m128( ) const { return m; }

	float to_float( ) { return _mm_cvtss_f32( m ); }
	float to_float( ) const { return _mm_cvtss_f32( m ); }

	operator float( ) { return to_float( ); }
	operator float( ) const { return to_float( ); }

	__m128 m;
};

CUTE_MATH_INLINE vfloat getx( v3 a ) { return vfloat( CUTE_MATH_SHUFFLE( a, a, 0, 0, 0 ) ); }
CUTE_MATH_INLINE vfloat gety( v3 a ) { return vfloat( CUTE_MATH_SHUFFLE( a, a, 1, 1, 1 ) ); }
CUTE_MATH_INLINE vfloat getz( v3 a ) { return vfloat( CUTE_MATH_SHUFFLE( a, a, 2, 2, 2 ) ); }

CUTE_MATH_INLINE v3 splatx( v3 a ) { return v3( CUTE_MATH_SHUFFLE( a, a, 0, 0, 0 ) ); }
CUTE_MATH_INLINE v3 splaty( v3 a ) { return v3( CUTE_MATH_SHUFFLE( a, a, 1, 1, 1 ) ); }
CUTE_MATH_INLINE v3 splatz( v3 a ) { return v3( CUTE_MATH_SHUFFLE( a, a, 2, 2, 2 ) ); }

struct m3
{
	CUTE_MATH_INLINE v3 operator[]( int i )
	{
		switch ( i )
		{
			case 0: return x;
			case 1: return y;
			case 2: return z;
			default: CUTE_MATH_ASSERT( 0 ); return x;
		}
	}

	v3 x;
	v3 y;
	v3 z;
};

CUTE_MATH_INLINE m3 rows( v3 x, v3 y, v3 z )
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
	CUTE_MATH_INLINE operator v3( ) const { return v3( m ); }
	CUTE_MATH_INLINE operator __m128( ) const { return m; }
};

struct v3_constf
{
	union { float f[ 4 ]; __m128 m; };
	CUTE_MATH_INLINE operator v3( ) const { return v3( m ); }
	CUTE_MATH_INLINE operator __m128( ) const { m; }
};

CUTE_MATH_SELECTANY v3_consti tmMaskSign = { 0x80000000, 0x80000000, 0x80000000, 0x80000000 };
CUTE_MATH_SELECTANY v3_consti tmMaskAllBits = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
CUTE_MATH_SELECTANY v3_constf tmMaskBasis = { 0.57735027f, 0.57735027f, 0.57735027f };

// the binary ops
CUTE_MATH_INLINE v3 operator+( v3 a, v3 b ) { return v3( _mm_add_ps( a, b ) ); }
CUTE_MATH_INLINE v3 operator-( v3 a, v3 b ) { return v3( _mm_sub_ps( a, b ) ); }
CUTE_MATH_INLINE v3& operator+=( v3 &a, v3 b ) { a = a + b; return a; }
CUTE_MATH_INLINE v3& operator-=( v3 &a, v3 b ) { a = a - b; return a; }

CUTE_MATH_INLINE vfloat operator+( vfloat a, vfloat b ) { return vfloat( _mm_add_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat operator-( vfloat a, vfloat b ) { return vfloat( _mm_sub_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat operator*( vfloat a, vfloat b ) { return vfloat( _mm_mul_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat operator/( vfloat a, vfloat b ) { return vfloat( _mm_div_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat& operator+=( vfloat &a, vfloat b ) { a = a + b; return a; }
CUTE_MATH_INLINE vfloat& operator-=( vfloat &a, vfloat b ) { a = a - b; return a; }
CUTE_MATH_INLINE vfloat& operator*=( vfloat &a, vfloat b ) { a = a * b; return a; }
CUTE_MATH_INLINE vfloat& operator/=( vfloat &a, vfloat b ) { a = a / b; return a; }

CUTE_MATH_INLINE vfloat operator+( vfloat a, float b ) { return vfloat( _mm_add_ps( a, vfloat( b ) ) ); }
CUTE_MATH_INLINE vfloat operator-( vfloat a, float b ) { return vfloat( _mm_sub_ps( a, vfloat( b ) ) ); }
CUTE_MATH_INLINE vfloat operator*( vfloat a, float b ) { return vfloat( _mm_mul_ps( a, vfloat( b ) ) ); }
CUTE_MATH_INLINE vfloat operator/( vfloat a, float b ) { return vfloat( _mm_div_ps( a, vfloat( b ) ) ); }
CUTE_MATH_INLINE vfloat& operator+=( vfloat &a, float b ) { a = a + b; return a; }
CUTE_MATH_INLINE vfloat& operator-=( vfloat &a, float b ) { a = a - b; return a; }
CUTE_MATH_INLINE vfloat& operator*=( vfloat &a, float b ) { a = a * b; return a; }
CUTE_MATH_INLINE vfloat& operator/=( vfloat &a, float b ) { a = a / b; return a; }

CUTE_MATH_INLINE vfloat operator+( float a, vfloat b ) { return vfloat( _mm_add_ps( vfloat( a ), b ) ); }
CUTE_MATH_INLINE vfloat operator-( float a, vfloat b ) { return vfloat( _mm_sub_ps( vfloat( a ), b ) ); }
CUTE_MATH_INLINE vfloat operator*( float a, vfloat b ) { return vfloat( _mm_mul_ps( vfloat( a ), b ) ); }
CUTE_MATH_INLINE vfloat operator/( float a, vfloat b ) { return vfloat( _mm_div_ps( vfloat( a ), b ) ); }
CUTE_MATH_INLINE float& operator+=( float &a, vfloat b ) { a = a + b; return a; }
CUTE_MATH_INLINE float& operator-=( float &a, vfloat b ) { a = a - b; return a; }
CUTE_MATH_INLINE float& operator*=( float &a, vfloat b ) { a = a * b; return a; }
CUTE_MATH_INLINE float& operator/=( float &a, vfloat b ) { a = a / b; return a; }

// generally comparisons are followed up with a mask(v3) call (or any(v3))
CUTE_MATH_INLINE v3 operator==( v3 a, v3 b ) { return v3( _mm_cmpeq_ps( a, b ) ); }
CUTE_MATH_INLINE v3 operator!=( v3 a, v3 b ) { return v3( _mm_cmpneq_ps( a, b ) ); }
CUTE_MATH_INLINE v3 operator<( v3 a, v3 b ) { return v3( _mm_cmplt_ps( a, b ) ); }
CUTE_MATH_INLINE v3 operator>( v3 a, v3 b ) { return v3( _mm_cmpgt_ps( a, b ) ); }
CUTE_MATH_INLINE v3 operator<=( v3 a, v3 b ) { return v3( _mm_cmple_ps( a, b ) ); }
CUTE_MATH_INLINE v3 operator>=( v3 a, v3 b ) { return v3( _mm_cmpge_ps( a, b ) ); }
CUTE_MATH_INLINE v3 operator-( v3 a ) { return v3( _mm_setzero_ps( ) ) - a; }

CUTE_MATH_INLINE vfloat operator==( vfloat a, vfloat b ) { return vfloat( _mm_cmpeq_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat operator!=( vfloat a, vfloat b ) { return vfloat( _mm_cmpneq_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat operator<( vfloat a, vfloat b ) { return vfloat( _mm_cmplt_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat operator>( vfloat a, vfloat b ) { return vfloat( _mm_cmpgt_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat operator<=( vfloat a, vfloat b ) { return vfloat( _mm_cmple_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat operator>=( vfloat a, vfloat b ) { return vfloat( _mm_cmpge_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat operator-( vfloat a ) { return vfloat( _mm_setzero_ps( ) ) - a; }

CUTE_MATH_INLINE unsigned mask( vfloat a ) { return _mm_movemask_ps( a ) & 7; }
CUTE_MATH_INLINE int any( vfloat a ) { return mask( a ) != 0; }
CUTE_MATH_INLINE int all( vfloat a ) { return mask( a ) == 7; }

CUTE_MATH_INLINE unsigned mask( v3 a ) { return _mm_movemask_ps( a ) & 7; }
CUTE_MATH_INLINE int any( v3 a ) { return mask( a ) != 0; }
CUTE_MATH_INLINE int all( v3 a ) { return mask( a ) == 7; }

CUTE_MATH_INLINE v3 setx( v3 a, float x )
{
	v3 t0 = v3( _mm_set_ss( x ) );
	return v3( _mm_move_ss( a, t0 ) );
}

CUTE_MATH_INLINE v3 sety( v3 a, float y )
{
	v3 t0 = v3( CUTE_MATH_SHUFFLE( a, a, 1, 0, 2 ) );
	v3 t1 = v3( _mm_set_ss( y ) );
	v3 t2 = v3( _mm_move_ss( t0, t1 ) );
	return v3( CUTE_MATH_SHUFFLE( t2, t2, 1, 0, 2 ) );
}

CUTE_MATH_INLINE v3 setz( v3 a, float z )
{
	v3 t0 = v3( CUTE_MATH_SHUFFLE( a, a, 2, 1, 0 ) );
	v3 t1 = v3( _mm_set_ss( z ) );
	v3 t2 = v3( _mm_move_ss( t0, t1 ) );
	return v3( CUTE_MATH_SHUFFLE( t2, t2, 2, 1, 0 ) );
}

CUTE_MATH_INLINE v3 operator*( v3 a, v3 b ) { return v3( _mm_mul_ps( a, b ) ); }
CUTE_MATH_INLINE v3 operator/( v3 a, v3 b ) { return v3( _mm_div_ps( a, b ) ); }
CUTE_MATH_INLINE v3& operator*=( v3& a, v3 b ) { a = a * b; return a; }
CUTE_MATH_INLINE v3& operator/=( v3& a, v3 b ) { a = a / b; return a; }
CUTE_MATH_INLINE v3 operator*( v3 a, vfloat b ) { return v3( _mm_mul_ps( a, b ) ); }
CUTE_MATH_INLINE v3 operator/( v3 a, vfloat b ) { return v3( _mm_div_ps( a, b ) ); }
CUTE_MATH_INLINE v3& operator*=( v3& a, vfloat b ) { a = a * b; return a; }
CUTE_MATH_INLINE v3& operator/=( v3& a, vfloat b ) { a = a / b; return a; }
CUTE_MATH_INLINE v3 operator*( v3 a, float b ) { return v3( _mm_mul_ps( a, vfloat( b ) ) ); }
CUTE_MATH_INLINE v3 operator/( v3 a, float b ) { return v3( _mm_div_ps( a, vfloat( b ) ) ); }
CUTE_MATH_INLINE v3& operator*=( v3& a, float b ) { a = a * b; return a; }
CUTE_MATH_INLINE v3& operator/=( v3& a, float b ) { a = a / b; return a; }

// f must be 16 byte aligned
CUTE_MATH_INLINE v3 load( float* f ) { return v3( _mm_load_ps( f ) ); }
CUTE_MATH_INLINE void store( v3 v, float* f ) { _mm_store_ps( f, v ); }

CUTE_MATH_INLINE vfloat dot( v3 a, v3 b )
{
	v3 t0 = v3( _mm_mul_ps( a, b ) );
	v3 t1 = v3( CUTE_MATH_SHUFFLE( t0, t0, 1, 0, 0 ) );
	v3 t2 = v3( _mm_add_ss( t0, t1 ) );
	v3 t3 = v3( CUTE_MATH_SHUFFLE( t2, t2, 2, 0, 0 ) );
	v3 t4 = v3( _mm_add_ss( t2, t3 ) );
	return vfloat( t4 );
}

CUTE_MATH_INLINE v3 cross( v3 a, v3 b )
{
	v3 t0 = v3( CUTE_MATH_SHUFFLE( a, a, 1, 2, 0 ) );
	v3 t1 = v3( CUTE_MATH_SHUFFLE( b, b, 2, 0, 1 ) );
	v3 t2 = v3( _mm_mul_ps( t0, t1 ) );

	t0 = v3( CUTE_MATH_SHUFFLE( t0, t0, 1, 2, 0 ) );
	t1 = v3( CUTE_MATH_SHUFFLE( t1, t1, 2, 0, 1 ) );
	t0 = v3( _mm_mul_ps( t0, t1 ) );

	return v3( _mm_sub_ps( t2, t0 ) );
}

CUTE_MATH_INLINE vfloat lengthSq( v3 a ) { return dot( a, a ); }
CUTE_MATH_INLINE vfloat sqrt( vfloat a ) {	return vfloat( _mm_sqrt_ps( a ) ); }
CUTE_MATH_INLINE vfloat length( v3 a ) { return sqrt( dot( a, a ) ); }
CUTE_MATH_INLINE v3 abs( v3 a ) { return v3( _mm_andnot_ps( tmMaskSign, a ) ); }
CUTE_MATH_INLINE v3 min( v3 a, v3 b ) { return v3( _mm_min_ps( a, b ) ); }
CUTE_MATH_INLINE v3 max( v3 a, v3 b ) { return v3( _mm_max_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat abs( vfloat a ) { return vfloat( _mm_andnot_ps( tmMaskSign, a ) ); }
CUTE_MATH_INLINE vfloat min( vfloat a, vfloat b ) { return vfloat( _mm_min_ps( a, b ) ); }
CUTE_MATH_INLINE vfloat max( vfloat a, vfloat b ) { return vfloat( _mm_max_ps( a, b ) ); }
CUTE_MATH_INLINE v3 select( v3 a, v3 b, v3 mask ) { return v3( _mm_xor_ps( a, _mm_and_ps( mask, _mm_xor_ps( b, a ) ) ) ); }
CUTE_MATH_INLINE v3 lerp( v3 a, v3 b, vfloat t ) { return a + (b - a) * t; }

// for posterity
// CUTE_MATH_INLINE v3 lerp( v3 a, v3 b, float t ) { return a * (1.0f - t) + b * t; }
// CUTE_MATH_INLINE v3 lerp( v3 a, v3 b, v3 t ) { return a * (1.0f - t) + b * t; }

CUTE_MATH_INLINE vfloat hmin( v3 a )
{
	v3 t0 = v3( CUTE_MATH_SHUFFLE( a, a, 1, 0, 2 ) );
	a = min( a, t0 );
	v3 t1 = v3( CUTE_MATH_SHUFFLE( a, a, 2, 0, 1 ) );
	return vfloat( min( a, t1 ) );
}

CUTE_MATH_INLINE vfloat hmax( v3 a )
{
	v3 t0 = v3( CUTE_MATH_SHUFFLE( a, a, 1, 0, 2 ) );
	a = max( a, t0 );
	v3 t1 = v3( CUTE_MATH_SHUFFLE( a, a, 2, 0, 1 ) );
	return vfloat( max( a, t1 ) );
}

CUTE_MATH_INLINE v3 norm( v3 a )
{ 
	vfloat t0 = dot( a, a );
	vfloat t1 = sqrt( t0 );
	v3 t2 = v3( _mm_div_ps( a, t1 ) );
	return v3( _mm_and_ps( t2, tmMaskAllBits ) );
}

CUTE_MATH_INLINE v3 clamp( v3 a, v3 vmin, v3 vmax )
{
	v3 t0 = v3( _mm_max_ps( vmin, a ) );
	return v3( _mm_min_ps( t0, vmax ) );
}

// sets up a mask of { x ? ~0 : 0, y ? ~0 : 0, z ? ~0 : 0 }
// x, y and z should be 0 or 1
CUTE_MATH_INLINE v3 mask( int x, int y, int z )
{
	v3_consti c;
	unsigned elements[] = { 0x00000000, 0xFFFFFFFF };

	CUTE_MATH_ASSERT( x < 2 && x >= 0 );
	CUTE_MATH_ASSERT( y < 2 && y >= 0 );
	CUTE_MATH_ASSERT( z < 2 && z >= 0 );

	c.i[ 0 ] = elements[ x ];
	c.i[ 1 ] = elements[ y ];
	c.i[ 2 ] = elements[ z ];
	c.i[ 3 ] = elements[ 0 ];

	return c;
}

CUTE_MATH_INLINE m3 m3_from_quat( vfloat x, vfloat y, vfloat z, vfloat w )
{
	vfloat x2 = x + x;
	vfloat y2 = y + y;
	vfloat z2 = z + z;

	vfloat xx = x * x2;
	vfloat xy = x * y2;
	vfloat xz = x * z2;
	vfloat xw = w * x2;
	vfloat yy = y * y2;
	vfloat yz = y * z2;
	vfloat yw = w * y2;
	vfloat zz = z * z2;
	vfloat zw = w * z2;

	vfloat one = vfloat( 1.0f );

	return rows(
		v3( one - yy - zz, xy + zw, xz - yw ),
		v3( xy - zw, one - xx - zz, yz + xw ),
		v3( xz + yw, yz - xw, one - xx - yy )
	);
}

CUTE_MATH_INLINE m3 m3_axis_angle( v3 axis, vfloat angle )
{
	vfloat s = vfloat( sinf( angle * 0.5f ) );
	vfloat c = vfloat( cosf( angle * 0.5f ) );

	vfloat x = getx( axis ) * s;
	vfloat y = gety( axis ) * s;
	vfloat z = getz( axis ) * s;
	vfloat w = c;

	return m3_from_quat( x, y, z, w );
}

CUTE_MATH_INLINE m3 m3_axis_angle( v3 axis, float angle )
{
	return m3_axis_angle( axis, vfloat( angle ) );
}

// Does not preserve 0.0f in w to get rid of extra shuffles.
// Oh well. Anyone have a better idea?
CUTE_MATH_INLINE m3 transpose( m3 a )
{
	v3 t0 = v3( _mm_shuffle_ps( a.x, a.y, _MM_SHUFFLE( 1, 0, 1, 0 ) ) );
	v3 t1 = v3( _mm_shuffle_ps( a.x, a.y, _MM_SHUFFLE( 2, 2, 2, 2 ) ) );
	v3 x =  v3( _mm_shuffle_ps( t0, a.z, _MM_SHUFFLE( 0, 0, 2, 0 ) ) );
	v3 y =  v3( _mm_shuffle_ps( t0, a.z, _MM_SHUFFLE( 0, 1, 3, 1 ) ) );
	v3 z =  v3( _mm_shuffle_ps( t1, a.z, _MM_SHUFFLE( 0, 2, 2, 0 ) ) );

	a.x = x;
	a.y = y;
	a.z = z;

	return a;
}

//  a * b
CUTE_MATH_INLINE v3 mul( m3 a, v3 b )
{
	v3 x = splatx( b );
	v3 y = splaty( b );
	v3 z = splatz( b );
	x = v3( _mm_mul_ps( x, a.x ) );
	y = v3( _mm_mul_ps( y, a.y ) );
	z = v3( _mm_mul_ps( z, a.z ) );
	v3 t0 = v3( _mm_add_ps( x, y ) );
	return v3( _mm_add_ps( t0, z ) );
}

// a^T * b
CUTE_MATH_INLINE v3 mulT( m3 a, v3 b ) { mul( transpose( a ), b ); }

// a * b
CUTE_MATH_INLINE m3 mul( m3 a, m3 b )
{
	v3 x = mul( a, b.x );
	v3 y = mul( a, b.y );
	v3 z = mul( a, b.z );
	return rows( x, y, z );
}

// a^T * b
CUTE_MATH_INLINE m3 mulT( m3 a, m3 b ) { return mul( transpose( a ), b ); }

// http://box2d.org/2014/02/computing-a-basis/
CUTE_MATH_INLINE m3 basis( v3 a )
{
	// Suppose vector a has all equal components and is a unit vector: a = (s, s, s)
	// Then 3*s*s = 1, s = sqrt(1/3) = 0.57735027. This means that at least one component
	// of a unit vector must be greater or equal to 0.57735027.

	v3 negA = -a;
	v3 t0 = v3( CUTE_MATH_SHUFFLE( a, negA, 1, 1, 0 ) );
	v3 b0 = v3( CUTE_MATH_SHUFFLE( t0, t0, 0, 2, 3 ) );
	t0 = v3( CUTE_MATH_SHUFFLE( a, negA, 2, 2, 1 ) );
	v3 b1 = v3( CUTE_MATH_SHUFFLE( t0, t0, 3, 1, 2 ) );

	v3 mask = v3( _mm_cmpge_ps( tmMaskBasis, abs( a ) ) );
	mask = splatx( mask );
	v3 b = select( b0, b1, mask );
	b = v3( norm( b ).m );
	v3 c = cross( a, b );

	return rows( a, b, c );
}

CUTE_MATH_INLINE m3 operator-( m3 a, m3 b )
{
	m3 c;
	c.x = a.x - b.x;
	c.y = a.y - b.y;
	c.z = a.z - b.z;
	return c;
}

CUTE_MATH_INLINE m3 operator+( m3 a, m3 b )
{
	m3 c;
	c.x = a.x + b.x;
	c.y = a.y + b.y;
	c.z = a.z + b.z;
	return c;
}

CUTE_MATH_INLINE m3& operator+=( m3& a, m3 b ) { a = a + b; return a; }
CUTE_MATH_INLINE m3& operator-=( m3& a, m3 b ) { a = a - b; return a; }

CUTE_MATH_INLINE m3 operator*( vfloat a, m3 b )
{
	m3 c;
	c.x = b.x * a;
	c.y = b.y * a;
	c.z = b.z * a;
	return c;
}

CUTE_MATH_INLINE m3 operator*( float a, m3 b )
{
	return vfloat( a ) * b;
}

struct transform
{
	v3 p; // position
	m3 r; // rotation
};

CUTE_MATH_INLINE v3 mul( transform tx, v3 a ) { return mul( tx.r, a ) + tx.p; }
CUTE_MATH_INLINE v3 mulT( transform tx, v3 a ) { return mul( tx.r, a - tx.p ); }

CUTE_MATH_INLINE transform mul( transform a, transform b )
{
	transform c;
	c.p = mul( a.r, b.p ) + a.p;
	c.r = mul( a.r, b.r );
	return c;
}

CUTE_MATH_INLINE transform mulT( transform a, transform b )
{
	transform c;
	c.p = mulT( a.r, b.p - a.p );
	c.r = mulT( a.r, b.r );
	return c;
}

struct halfspace
{
	v3 n;
	vfloat d;
};

CUTE_MATH_INLINE v3 origin( halfspace h ) { return h.n * h.d; }
CUTE_MATH_INLINE vfloat distance( halfspace h, v3 p ) { return dot( h.n, p ) - h.d; }
CUTE_MATH_INLINE v3 projected( halfspace h, v3 p ) { return p - h.n * distance( h, p ); }

CUTE_MATH_INLINE halfspace mul( transform a, halfspace b )
{
	v3 o = origin( b );
	o = mul( a, o );
	v3 normal = mul( a.r, b.n );
	halfspace c;
	c.n = normal;
	c.d = dot( o, normal );
	return c;
}

CUTE_MATH_INLINE halfspace mulT( transform a, halfspace b )
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
CUTE_MATH_INLINE v3 intersect( v3 a, v3 b, vfloat da, vfloat db )
{
	return a + (b - a) * (da / (da - db));
}

// carefully choose kTol, see: http://www.randygaul.net/2014/11/07/robust-parallel-vector-test/
CUTE_MATH_INLINE int parallel( v3 a, v3 b, float kTol )
{
	vfloat k = length( a ) / length( b );
	v3 bk = b * k;
	if ( all( abs( a - bk ) < v3( kTol ) ) ) return 1;
	return 0;
}

CUTE_MATH_INLINE m3 outer( v3 u, v3 v )
{
	v3 a = v * getx( u );
	v3 b = v * gety( u );
	v3 c = v * getz( u );
	return rows( a, b, c );
}

void lookAt( float* world_to_cam, v3 eye, v3 target, v3 up, float* cam_to_world = 0 )
{
	v3 front = norm( target - eye );
	v3 side = norm( cross( front, up ) );
	v3 top = norm( cross( side, front ) );

	world_to_cam[ 0 ] = getx( side );
	world_to_cam[ 1 ] = getx( top );
	world_to_cam[ 2 ] = -getx( front );
	world_to_cam[ 3 ] = 0;

	world_to_cam[ 4 ] = gety( side );
	world_to_cam[ 5 ] = gety( top );
	world_to_cam[ 6 ] = -gety( front );
	world_to_cam[ 7 ] = 0;

	world_to_cam[ 8 ] = getz( side );
	world_to_cam[ 9 ] = getz( top );
	world_to_cam[ 10 ] = -getz( front );
	world_to_cam[ 11 ] = 0;

	v3 x = v3( world_to_cam[ 0 ], world_to_cam[ 4 ], world_to_cam[ 8 ] );
	v3 y = v3( world_to_cam[ 1 ], world_to_cam[ 5 ], world_to_cam[ 9 ] );
	v3 z = v3( world_to_cam[ 2 ], world_to_cam[ 6 ], world_to_cam[ 10 ] );

	world_to_cam[ 12 ] = -dot( x, eye );
	world_to_cam[ 13 ] = -dot( y, eye );
	world_to_cam[ 14 ] = -dot( z, eye );
	world_to_cam[ 15 ] = 1.0f;

	if ( cam_to_world )
	{
		cam_to_world[ 0 ] = getx( side );
		cam_to_world[ 1 ] = gety( side );
		cam_to_world[ 2 ] = getz( side );
		cam_to_world[ 3 ] = 0;

		cam_to_world[ 4 ] = getx( top );
		cam_to_world[ 5 ] = gety( top );
		cam_to_world[ 6 ] = getz( top );
		cam_to_world[ 7 ] = 0;

		cam_to_world[ 8 ] = -getx( front );
		cam_to_world[ 9 ] = -gety( front );
		cam_to_world[ 10 ] = -getz( front );
		cam_to_world[ 11 ] = 0;

		cam_to_world[ 12 ] = getx( eye );
		cam_to_world[ 13 ] = gety( eye );
		cam_to_world[ 14 ] = getz( eye );
		cam_to_world[ 15 ] = 1.0f;
	}
}

#if !defined( TINYGL_H )
void tgMulv( float* a, float* b )
{
	float result[ 4 ];

	result[ 0 ] = a[ 0 ] * b[ 0 ] + a[4] * b[ 1 ] + a[ 8 ] * b[ 2 ] + a[ 12 ] * b[ 3 ];
	result[ 1 ] = a[ 1 ] * b[ 0 ] + a[5] * b[ 1 ] + a[ 9 ] * b[ 2 ] + a[ 13 ] * b[ 3 ];
	result[ 2 ] = a[ 2 ] * b[ 0 ] + a[6] * b[ 1 ] + a[ 10 ] * b[ 2 ] + a[ 14 ] * b[ 3 ];
	result[ 3 ] = a[ 3 ] * b[ 0 ] + a[7] * b[ 1 ] + a[ 11 ] * b[ 2 ] + a[ 15 ] * b[ 3 ];

	b[ 0 ] = result[ 0 ];
	b[ 1 ] = result[ 1 ];
	b[ 2 ] = result[ 2 ];
	b[ 3 ] = result[ 3 ];
}
#endif

#include <cmath>

void compute_mouse_ray( float mouse_x, float mouse_y, float fov, float viewport_w, float viewport_h, float* cam_inv, float near_plane_dist, v3* mouse_pos, v3* mouse_dir )
{
	float aspect = (float)viewport_w / (float)viewport_h;
	float px = 2.0f * aspect * mouse_x / viewport_w - aspect;
	float py = -2.0f * mouse_y / viewport_h + 1.0f;
	float pz = -1.0f / tanf( fov / 2.0f );
	v3 point_in_view_space( px, py, pz );

	v3 cam_pos( cam_inv[ 12 ], cam_inv[ 13 ], cam_inv[ 14 ] );
	float pf[ 4 ] = { getx( point_in_view_space ), gety( point_in_view_space ), getz( point_in_view_space ), 1.0f };
	tgMulv( cam_inv, pf );
	v3 point_on_clipping_plane( pf[ 0 ] , pf[ 1 ], pf[ 2 ] );
	v3 dir_in_world_space = point_on_clipping_plane - cam_pos;

	v3 dir = norm( dir_in_world_space );
	v3 cam_forward( cam_inv[ 8 ], cam_inv[ 9 ], cam_inv[ 10 ] );

	*mouse_dir = dir;
	*mouse_pos = cam_pos + dir * dot( dir, cam_forward ) * vfloat( near_plane_dist );
}

struct q4
{
	q4( ) { }
	CUTE_MATH_INLINE explicit q4( v3& vector_part, vfloat& scalar_part ) { m = _mm_set_ps( scalar_part, getz( vector_part ), gety( vector_part ), getx( vector_part ) ); }
	CUTE_MATH_INLINE explicit q4( float x, float y, float z, float w ) { m = _mm_set_ps( w, z, y, x ); }

	CUTE_MATH_INLINE operator __m128( ) { return m; }
	CUTE_MATH_INLINE operator __m128( ) const { return m; }

	__m128 m;
};

CUTE_MATH_INLINE q4 q3_axis_angle( v3 axis_normalized, vfloat angle )
{
	vfloat s = vfloat( sinf( angle * 0.5f ) );
	vfloat c = vfloat( cosf( angle * 0.5f ) );
	return q4( axis_normalized * s, c );
}

CUTE_MATH_INLINE vfloat getx( q4 a ) { return vfloat( CUTE_MATH_SHUFFLE( a, a, 0, 0, 0 ) ); }
CUTE_MATH_INLINE vfloat gety( q4 a ) { return vfloat( CUTE_MATH_SHUFFLE( a, a, 1, 1, 1 ) ); }
CUTE_MATH_INLINE vfloat getz( q4 a ) { return vfloat( CUTE_MATH_SHUFFLE( a, a, 2, 2, 2 ) ); }
CUTE_MATH_INLINE vfloat getw( q4 a ) { return vfloat( CUTE_MATH_SHUFFLE( a, a, 3, 3, 3 ) ); }

// un-optimized
CUTE_MATH_INLINE q4 norm( q4 q )
{
	vfloat x = getx( q );
	vfloat y = gety( q );
	vfloat z = getz( q );
	vfloat w = getw( q );

	vfloat d = w * w + x * x + y * y + z * z;

	if( d == 0 )
		w = vfloat( 1.0 );

	d = vfloat( 1.0 ) / sqrtf( d );

	if ( d > vfloat( 1.0e-8f ) )
	{
		x *= d;
		y *= d;
		z *= d;
		w *= d;
	}

	return q4( x, y, z, w );
}

// un-optimized
CUTE_MATH_INLINE q4 operator*( q4 a, q4 b )
{
	return q4(
		getw( a ) * getx( b ) + getx( a ) * getw( b ) + gety( a ) * getz( b ) - getz( a ) * gety( b ),
		getw( a ) * gety( b ) + gety( a ) * getw( b ) + getz( a ) * getx( b ) - getx( a ) * getz( b ),
		getw( a ) * getz( b ) + getz( a ) * getw( b ) + getx( a ) * gety( b ) - gety( a ) * getx( b ),
		getw( a ) * getw( b ) - getx( a ) * getx( b ) - gety( a ) * gety( b ) - getz( a ) * getz( b )
		);
}

// un-optimized
CUTE_MATH_INLINE q4 integrate( q4 q, v3 w, vfloat h )
{
	q4 wq( getx( w ) * h, gety( w ) * h, getz( w ) * h, 0.0f );

	wq = wq * q;

	q4 q0 = q4(
		getx( q ) + getx( wq ) * vfloat( 0.5 ),
		gety( q ) + gety( wq ) * vfloat( 0.5 ),
		getz( q ) + getz( wq ) * vfloat( 0.5 ),
		getw( q ) + getw( wq ) * vfloat( 0.5 )
	);

	return norm( q0 );
}

// un-optimized
CUTE_MATH_INLINE m3 m3_from_quat( q4 q )
{
	return m3_from_quat( getx( q ), gety( q ), getz( q ), getw( q ) );
}

// globals
CUTE_MATH_SELECTANY m3 identity_m3 = rows( v3( 1.0f, 0.0f, 0.0f ), v3( 0.0f, 1.0f, 0.0f ), v3( 0.0f, 0.0f, 1.0f ) );
CUTE_MATH_SELECTANY m3 zero_m3 = rows( v3( 0.0f, 0.0f, 0.0f ), v3( 0.0f, 0.0f, 0.0f ), v3( 0.0f, 0.0f, 0.0f ) );
CUTE_MATH_SELECTANY v3 zero_v3 = v3( 0.0f, 0.0f, 0.0f );
CUTE_MATH_SELECTANY vfloat zero_vf = vfloat( 0.0f );
CUTE_MATH_SELECTANY vfloat one_vf = vfloat( 1.0f );
CUTE_MATH_SELECTANY q4 identity_q4 = q4( 0.0f, 0.0f, 0.0f, 1.0f );

#define CUTE_MATH_H
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
