#if !defined(CUTE_MATH_H)

/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_math.h - v1.02

	To create implementation (the function definitions)
		#define CUTE_MATH_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file

	REVISION HISTORY

		1.00 (12/21/2016) initial release
		1.01 (10/05/2017) vfloat data type added, removed out-dated comments,
		                  added compute_mouse_ray, added more m3 ops, added look_at
		1.02 (06/14/2019) removed vfloat, consistent function naming, setup standard
		                  calling convention, added implementation section, added
		                  vector indexing operators, added axis/angle from m3

	SUMMARY

		A professional level implementation of SSE intrinsics.
*/

#include <stdint.h>
#include <math.h>
#include <xmmintrin.h>

#ifndef CUTE_MATH_ASSERT
#	include <assert.h>
#	define CUTE_MATH_ASSERT assert
#endif

#define CUTE_MATH_SHUFFLE(a, b, x, y, z) _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, z, y, x))

#ifdef _MSC_VER
#	define CUTE_MATH_CALL __vectorcall
#else
#	define CUTE_MATH_CALL
#endif

#ifdef _WIN32
#	define CUTE_MATH_INLINE __forceinline
#	define CUTE_MATH_SELECTANY extern const __declspec(selectany)
#	define CUTE_MATH_RESTRICT __restrict
#else
// Just assume a g++-like compiler.
#	define CUTE_MATH_INLINE __attribute__((always_inline))
#	define CUTE_MATH_SELECTANY extern const __attribute__((selectany))
#	define CUTE_MATH_RESTRICT __restrict__
#endif

#define CUTE_MATH_PI 3.14159265358979323846f
#define CUTE_MATH_DEG2RAD(X) ((X) * CUTE_MATH_PI / 180.0f)
#define CUTE_MATH_RAD2DEG(X) ((X) * 180.0f / CUTE_MATH_PI)
#define CUTE_MATH_FLT_MAX 3.402823466e+38F
#define CUTE_MATH_FLT_EPSILON 1.19209290E-07f

// -------------------------------------------------------------------------------------------------
// Scalar operations.

CUTE_MATH_INLINE float min(float a, float b) { return a < b ? a : b; }
CUTE_MATH_INLINE float max(float a, float b) { return b < a ? a : b; }
CUTE_MATH_INLINE float clamp(float a, float lo, float hi) { return max(lo, min(a, hi)); }
CUTE_MATH_INLINE float sign(float a) { return a < 0 ? -1.0f : 1.0f; }
CUTE_MATH_INLINE float intersect(float da, float db) { return da / (da - db); }
CUTE_MATH_INLINE float invert_safe(float a) { return a != 0 ? a / 1.0f : 0; }

CUTE_MATH_INLINE int min(int a, int b) { return a < b ? a : b; }
CUTE_MATH_INLINE int max(int a, int b) { return b < a ? a : b; }
CUTE_MATH_INLINE int clamp(int a, int lo, int hi) { return max(lo, min(a, hi)); }
CUTE_MATH_INLINE int sign(int a) { return a < 0 ? -1 : 1; }

// -------------------------------------------------------------------------------------------------
// 3-Vector definition.

struct v3
{
	CUTE_MATH_INLINE v3() { }
	CUTE_MATH_INLINE explicit v3(float x, float y, float z) { m = _mm_set_ps(0, z, y, x); }
	CUTE_MATH_INLINE explicit v3(float a) { m = _mm_set_ps(0, a, a, a); }
	CUTE_MATH_INLINE explicit v3(float *a) { m = _mm_set_ps(0, a[2], a[1], a[0]); }
	CUTE_MATH_INLINE explicit v3(__m128 v) { m = v; }
	CUTE_MATH_INLINE operator __m128() { return m; }
	CUTE_MATH_INLINE operator const __m128() const { return m; }

	CUTE_MATH_INLINE float operator[](int i)
	{
		switch (i)
		{
		case 0: return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(m, m, 0, 0, 0));
		case 1: return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(m, m, 1, 1, 1));
		case 2: return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(m, m, 2, 2, 2));
		default: CUTE_MATH_ASSERT(0); return 0;
		}
	}

	CUTE_MATH_INLINE float x() { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(this->m, this->m, 0, 0, 0)); }
	CUTE_MATH_INLINE float y() { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(this->m, this->m, 1, 1, 1)); }
	CUTE_MATH_INLINE float z() { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(this->m, this->m, 2, 2, 2)); }

	CUTE_MATH_INLINE v3 xyz() { return *this; }
	CUTE_MATH_INLINE v3 xzy() { return v3(CUTE_MATH_SHUFFLE(this->m, this->m, 0, 2, 1)); }
	CUTE_MATH_INLINE v3 yxz() { return v3(CUTE_MATH_SHUFFLE(this->m, this->m, 1, 0, 2)); }
	CUTE_MATH_INLINE v3 yzx() { return v3(CUTE_MATH_SHUFFLE(this->m, this->m, 1, 2, 0)); }
	CUTE_MATH_INLINE v3 zxy() { return v3(CUTE_MATH_SHUFFLE(this->m, this->m, 2, 0, 1)); }
	CUTE_MATH_INLINE v3 zyx() { return v3(CUTE_MATH_SHUFFLE(this->m, this->m, 2, 1, 0)); }

	__m128 m;
};

CUTE_MATH_INLINE float CUTE_MATH_CALL getx(v3 a) { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(a, a, 0, 0, 0)); }
CUTE_MATH_INLINE float CUTE_MATH_CALL gety(v3 a) { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(a, a, 1, 1, 1)); }
CUTE_MATH_INLINE float CUTE_MATH_CALL getz(v3 a) { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(a, a, 2, 2, 2)); }

CUTE_MATH_INLINE v3 CUTE_MATH_CALL splatx(v3 a) { return v3(CUTE_MATH_SHUFFLE(a, a, 0, 0, 0)); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL splaty(v3 a) { return v3(CUTE_MATH_SHUFFLE(a, a, 1, 1, 1)); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL splatz(v3 a) { return v3(CUTE_MATH_SHUFFLE(a, a, 2, 2, 2)); }

// -------------------------------------------------------------------------------------------------
// Binary operators.

CUTE_MATH_INLINE v3 CUTE_MATH_CALL operator+(v3 a, v3 b) { return v3(_mm_add_ps(a, b)); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL operator-(v3 a, v3 b) { return v3(_mm_sub_ps(a, b)); }
CUTE_MATH_INLINE v3& CUTE_MATH_CALL operator+=(v3 &a, v3 b) { a = a + b; return a; }
CUTE_MATH_INLINE v3& CUTE_MATH_CALL operator-=(v3 &a, v3 b) { a = a - b; return a; }

// SIMD comparisons return a 4-lane vector. To keep things simple `bool3` is merely a descriptive alias
// for `v3`, and is not its own type.
using bool3 = v3;

// Generally comparisons are followed up with a mask(v3) or any(v3) call.
CUTE_MATH_INLINE bool3 CUTE_MATH_CALL operator==(v3 a, v3 b) { return bool3(_mm_cmpeq_ps(a, b)); }
CUTE_MATH_INLINE bool3 CUTE_MATH_CALL operator!=(v3 a, v3 b) { return bool3(_mm_cmpneq_ps(a, b)); }
CUTE_MATH_INLINE bool3 CUTE_MATH_CALL operator<(v3 a, v3 b) { return bool3(_mm_cmplt_ps(a, b)); }
CUTE_MATH_INLINE bool3 CUTE_MATH_CALL operator>(v3 a, v3 b) { return bool3(_mm_cmpgt_ps(a, b)); }
CUTE_MATH_INLINE bool3 CUTE_MATH_CALL operator<=(v3 a, v3 b) { return bool3(_mm_cmple_ps(a, b)); }
CUTE_MATH_INLINE bool3 CUTE_MATH_CALL operator>=(v3 a, v3 b) { return bool3(_mm_cmpge_ps(a, b)); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL operator-(v3 a) { return v3(_mm_setzero_ps()) - a; }

CUTE_MATH_INLINE unsigned CUTE_MATH_CALL mask(v3 a) { return _mm_movemask_ps(a) & 7; }
CUTE_MATH_INLINE bool CUTE_MATH_CALL any(v3 a) { return mask(a) != 0; }
CUTE_MATH_INLINE bool CUTE_MATH_CALL all(v3 a) { return mask(a) == 7; }

CUTE_MATH_INLINE v3 CUTE_MATH_CALL setx(v3 a, float x)
{
	v3 t0 = v3(_mm_set_ss(x));
	return v3(_mm_move_ss(a, t0));
}

CUTE_MATH_INLINE v3 CUTE_MATH_CALL sety(v3 a, float y)
{
	v3 t0 = v3(CUTE_MATH_SHUFFLE(a, a, 1, 0, 2));
	v3 t1 = v3(_mm_set_ss(y));
	v3 t2 = v3(_mm_move_ss(t0, t1));
	return v3(CUTE_MATH_SHUFFLE(t2, t2, 1, 0, 2));
}

CUTE_MATH_INLINE v3 CUTE_MATH_CALL setz(v3 a, float z)
{
	v3 t0 = v3(CUTE_MATH_SHUFFLE(a, a, 2, 1, 0));
	v3 t1 = v3(_mm_set_ss(z));
	v3 t2 = v3(_mm_move_ss(t0, t1));
	return v3(CUTE_MATH_SHUFFLE(t2, t2, 2, 1, 0));
}

CUTE_MATH_INLINE v3 CUTE_MATH_CALL operator*(v3 a, v3 b) { return v3(_mm_mul_ps(a, b)); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL operator/(v3 a, v3 b) { return v3(_mm_div_ps(a, b)); }
CUTE_MATH_INLINE v3& CUTE_MATH_CALL operator*=(v3& a, v3 b) { a = a * b; return a; }
CUTE_MATH_INLINE v3& CUTE_MATH_CALL operator/=(v3& a, v3 b) { a = a / b; return a; }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL operator*(v3 a, float b) { return v3(_mm_mul_ps(a, v3(b))); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL operator/(v3 a, float b) { return v3(_mm_div_ps(a, v3(b))); }
CUTE_MATH_INLINE v3& CUTE_MATH_CALL operator*=(v3& a, float b) { a = a * b; return a; }
CUTE_MATH_INLINE v3& CUTE_MATH_CALL operator/=(v3& a, float b) { a = a / b; return a; }

// -------------------------------------------------------------------------------------------------
// Helpers for static data.

struct cute_math_const_integer
{
	union { uint32_t i[4]; __m128 m; };
	CUTE_MATH_INLINE operator v3() const { return v3(m); }
	CUTE_MATH_INLINE operator __m128() const { return m; }
};

struct cute_math_const_float
{
	union { float f[4]; __m128 m; };
	CUTE_MATH_INLINE operator v3() const { return v3(m); }
	CUTE_MATH_INLINE operator __m128() const { m; }
};

CUTE_MATH_SELECTANY cute_math_const_integer cute_math_mask_sign = { 0x80000000, 0x80000000, 0x80000000, 0x80000000 };
CUTE_MATH_SELECTANY cute_math_const_integer cute_math_mask_all_bits = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000 };
CUTE_MATH_SELECTANY cute_math_const_float cute_math_mask_basis = { 0.57735027f, 0.57735027f, 0.57735027f };

// -------------------------------------------------------------------------------------------------
// Vector operations.

// `f` must be 16 byte aligned.
CUTE_MATH_INLINE v3 CUTE_MATH_CALL load(float* f) { return v3(_mm_load_ps(f)); }
CUTE_MATH_INLINE void CUTE_MATH_CALL store(v3 v, float* f) { _mm_store_ps(f, v); }

CUTE_MATH_INLINE float CUTE_MATH_CALL dot(v3 a, v3 b)
{
	v3 t0 = v3(_mm_mul_ps(a, b));
	v3 t1 = v3(CUTE_MATH_SHUFFLE(t0, t0, 1, 0, 0));
	v3 t2 = v3(_mm_add_ss(t0, t1));
	v3 t3 = v3(CUTE_MATH_SHUFFLE(t2, t2, 2, 0, 0));
	v3 t4 = v3(_mm_add_ss(t2, t3));
	return getx(t4);
}

CUTE_MATH_INLINE v3 CUTE_MATH_CALL cross(v3 a, v3 b)
{
	v3 t0 = v3(CUTE_MATH_SHUFFLE(a, a, 1, 2, 0));
	v3 t1 = v3(CUTE_MATH_SHUFFLE(b, b, 2, 0, 1));
	v3 t2 = v3(_mm_mul_ps(t0, t1));

	t0 = v3(CUTE_MATH_SHUFFLE(t0, t0, 1, 2, 0));
	t1 = v3(CUTE_MATH_SHUFFLE(t1, t1, 2, 0, 1));
	t0 = v3(_mm_mul_ps(t0, t1));

	return v3(_mm_sub_ps(t2, t0));
}

CUTE_MATH_INLINE float CUTE_MATH_CALL length_sq(v3 a) { return dot(a, a); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL sqrt(v3 a) { return v3(_mm_sqrt_ps(a)); }
CUTE_MATH_INLINE float CUTE_MATH_CALL length(v3 a) { return sqrtf(dot(a, a)); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL abs(v3 a) { return v3(_mm_andnot_ps(cute_math_mask_sign, a)); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL min(v3 a, v3 b) { return v3(_mm_min_ps(a, b)); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL max(v3 a, v3 b) { return v3(_mm_max_ps(a, b)); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL select(v3 a, v3 b, v3 mask) { return v3(_mm_xor_ps(a, _mm_and_ps(mask, _mm_xor_ps(b, a)))); }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL lerp(v3 a, v3 b, float t) { return a + (b - a) * t; }

CUTE_MATH_INLINE float CUTE_MATH_CALL hmin(v3 a)
{
	v3 t0 = v3(CUTE_MATH_SHUFFLE(a, a, 1, 0, 2));
	a = min(a, t0);
	v3 t1 = v3(CUTE_MATH_SHUFFLE(a, a, 2, 0, 1));
	return getx(min(a, t1));
}

CUTE_MATH_INLINE float CUTE_MATH_CALL hmax(v3 a)
{
	v3 t0 = v3(CUTE_MATH_SHUFFLE(a, a, 1, 0, 2));
	a = max(a, t0);
	v3 t1 = v3(CUTE_MATH_SHUFFLE(a, a, 2, 0, 1));
	return getx(max(a, t1));
}

CUTE_MATH_INLINE v3 CUTE_MATH_CALL norm(v3 a)
{
	float t0 = dot(a, a);
	float t1 = sqrtf(t0);
	v3 t2 = v3(_mm_div_ps(a, v3(t1)));
	return v3(_mm_and_ps(t2, cute_math_mask_all_bits));
}

// Optimize me.
CUTE_MATH_INLINE v3 CUTE_MATH_CALL safe_norm(v3 a)
{
	float t0 = dot(a, a);
	if (t0 == 0) {
		return v3(0, 0, 0);
	} else {
		float t1 = sqrtf(t0);
		v3 t2 = v3(_mm_div_ps(a, v3(t1)));
		return v3(_mm_and_ps(t2, cute_math_mask_all_bits));
	}
}

CUTE_MATH_INLINE v3 CUTE_MATH_CALL invert(v3 a)
{
	return v3(_mm_div_ps(v3(1.0f), a));
}

// Optimize me.
CUTE_MATH_INLINE v3 CUTE_MATH_CALL invert_safe(v3 a)
{
	float x = a.x();
	float y = a.y();
	float z = a.z();
	return v3(
		x == 0 ? 0 : 1.0f / x,
		y == 0 ? 0 : 1.0f / y,
		z == 0 ? 0 : 1.0f / z
	);
}

CUTE_MATH_INLINE v3 CUTE_MATH_CALL clamp(v3 a, v3 vmin, v3 vmax)
{
	v3 t0 = v3(_mm_max_ps(vmin, a));
	return v3(_mm_min_ps(t0, vmax));
}

// Sets up a mask of { x ? ~0 : 0, y ? ~0 : 0, z ? ~0 : 0 }, where x, y and z should be 0 or 1.
CUTE_MATH_INLINE v3 CUTE_MATH_CALL mask(int x, int y, int z)
{
	cute_math_const_integer c;
	unsigned elements[] = { 0x00000000, 0xFFFFFFFF };

	CUTE_MATH_ASSERT(x < 2 && x >= 0);
	CUTE_MATH_ASSERT(y < 2 && y >= 0);
	CUTE_MATH_ASSERT(z < 2 && z >= 0);

	c.i[0] = elements[x];
	c.i[1] = elements[y];
	c.i[2] = elements[z];
	c.i[3] = elements[0];

	return c;
}

// `da` and `db` (standing for dot a and dot b) should be distances to plane, i.e. `halfspace::distance`.
CUTE_MATH_INLINE v3 CUTE_MATH_CALL intersect(v3 a, v3 b, float da, float db)
{
	return a + (b - a) * (da / (da - db));
}

// Carefully choose `tolerance`, see: http://www.randygaul.net/2014/11/07/robust-parallel-vector-test/
CUTE_MATH_INLINE bool CUTE_MATH_CALL parallel(v3 a, v3 b, float tolerance)
{
	float k = length(a) / length(b);
	v3 bk = b * k;
	if (all(abs(a - bk) < v3(tolerance))) return 1;
	return 0;
}

// -------------------------------------------------------------------------------------------------
// Matrix operations.

struct m3
{
	CUTE_MATH_INLINE v3 operator[](int i)
	{
		switch (i)
		{
		case 0: return x;
		case 1: return y;
		case 2: return z;
		default: CUTE_MATH_ASSERT(0); return x;
		}
	}

	v3 x;
	v3 y;
	v3 z;
};

CUTE_MATH_INLINE m3 CUTE_MATH_CALL rows(v3 x, v3 y, v3 z)
{
	m3 m;
	m.x = x;
	m.y = y;
	m.z = z;
	return m;
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL operator-(m3 a, m3 b)
{
	m3 c;
	c.x = a.x - b.x;
	c.y = a.y - b.y;
	c.z = a.z - b.z;
	return c;
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL operator+(m3 a, m3 b)
{
	m3 c;
	c.x = a.x + b.x;
	c.y = a.y + b.y;
	c.z = a.z + b.z;
	return c;
}

CUTE_MATH_INLINE m3& CUTE_MATH_CALL operator+=(m3& a, m3 b) { a = a + b; return a; }
CUTE_MATH_INLINE m3& CUTE_MATH_CALL operator-=(m3& a, m3 b) { a = a - b; return a; }

CUTE_MATH_INLINE m3 CUTE_MATH_CALL operator*(float a, m3 b)
{
	m3 c;
	c.x = b.x * a;
	c.y = b.y * a;
	c.z = b.z * a;
	return c;
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL m3_from_quat(float x, float y, float z, float w)
{
	float x2 = x + x;
	float y2 = y + y;
	float z2 = z + z;

	float xx = x * x2;
	float xy = x * y2;
	float xz = x * z2;
	float xw = w * x2;
	float yy = y * y2;
	float yz = y * z2;
	float yw = w * y2;
	float zz = z * z2;
	float zw = w * z2;

	return rows(
		v3(1.0f - yy - zz, xy + zw, xz - yw),
		v3(xy - zw, 1.0f - xx - zz, yz + xw),
		v3(xz + yw, yz - xw, 1.0f - xx - yy)
	);
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL m3_from_axis_angle(v3 axis, float angle)
{
	float s = sinf(angle * 0.5f);
	float c = cosf(angle * 0.5f);

	float x = getx(axis) * s;
	float y = gety(axis) * s;
	float z = getz(axis) * s;
	float w = c;

	return m3_from_quat(x, y, z, w);
}

// Does not preserve 0 in w to get rid of extra shuffles.
// Oh well. Anyone have a better idea?
CUTE_MATH_INLINE m3 CUTE_MATH_CALL transpose(m3 a)
{
	v3 t0 = v3(_mm_shuffle_ps(a.x, a.y, _MM_SHUFFLE(1, 0, 1, 0)));
	v3 t1 = v3(_mm_shuffle_ps(a.x, a.y, _MM_SHUFFLE(2, 2, 2, 2)));
	v3 x =  v3(_mm_shuffle_ps(t0, a.z, _MM_SHUFFLE(0, 0, 2, 0)));
	v3 y =  v3(_mm_shuffle_ps(t0, a.z, _MM_SHUFFLE(0, 1, 3, 1)));
	v3 z =  v3(_mm_shuffle_ps(t1, a.z, _MM_SHUFFLE(0, 2, 2, 0)));

	a.x = x;
	a.y = y;
	a.z = z;

	return a;
}

//  a * b
CUTE_MATH_INLINE v3 CUTE_MATH_CALL mul(m3 a, v3 b)
{
	v3 x = splatx(b);
	v3 y = splaty(b);
	v3 z = splatz(b);
	x = v3(_mm_mul_ps(x, a.x));
	y = v3(_mm_mul_ps(y, a.y));
	z = v3(_mm_mul_ps(z, a.z));
	v3 t0 = v3(_mm_add_ps(x, y));
	return v3(_mm_add_ps(t0, z));
}

// a^T * b
CUTE_MATH_INLINE v3 CUTE_MATH_CALL mul_transpose(m3 a, v3 b) { return mul(transpose(a), b); }

// a * b
CUTE_MATH_INLINE m3 CUTE_MATH_CALL mul(m3 a, m3 b)
{
	v3 x = mul(a, b.x);
	v3 y = mul(a, b.y);
	v3 z = mul(a, b.z);
	return rows(x, y, z);
}

// a^T * b
CUTE_MATH_INLINE m3 CUTE_MATH_CALL mul_transpose(m3 a, m3 b) { return mul(transpose(a), b); }

// http://box2d.org/2014/02/computing-a-basis/
CUTE_MATH_INLINE m3 CUTE_MATH_CALL basis(v3 a)
{
	// Suppose vector a has all equal components and is a unit vector: a = (s, s, s)
	// Then 3*s*s = 1, s = sqrt(1/3) = 0.57735027. This means that at least one component
	// of a unit vector must be greater or equal to 0.57735027.

	v3 neg_a = -a;
	v3 t0 = v3(CUTE_MATH_SHUFFLE(a, neg_a, 1, 1, 0));
	v3 b0 = v3(CUTE_MATH_SHUFFLE(t0, t0, 0, 2, 3));
	t0 = v3(CUTE_MATH_SHUFFLE(a, neg_a, 2, 2, 1));
	v3 b1 = v3(CUTE_MATH_SHUFFLE(t0, t0, 3, 1, 2));

	v3 mask = v3(_mm_cmpge_ps(cute_math_mask_basis, abs(a)));
	mask = splatx(mask);
	v3 b = select(b0, b1, mask);
	b = v3(norm(b).m);
	v3 c = cross(a, b);

	return rows(a, b, c);
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL outer_product(v3 u, v3 v)
{
	v3 a = v * getx(u);
	v3 b = v * gety(u);
	v3 c = v * getz(u);
	return rows(a, b, c);
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL m3_from_x_axis(float radians)
{
	float s = sinf(radians);
	float c = cosf(radians);
	return rows(
		v3(1,  0,  0),
		v3(0,  c, -s),
		v3(0,  s,  c)
	);
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL m3_from_y_axis(float radians)
{
	float s = sinf(radians);
	float c = cosf(radians);
	return rows(
		v3( c, 0, s),
		v3( 0, 1, 0),
		v3(-s, 0, c)
	);
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL m3_from_z_axis(float radians)
{
	float s = sinf(radians);
	float c = cosf(radians);
	return rows(
		v3(c, -s, 0),
		v3(s,  c, 0),
		v3(0,  0, 1)
	);
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL m3_from_euler_xyz(float x_radians, float y_radians, float z_radians)
{
	m3 x = m3_from_x_axis(x_radians);
	m3 y = m3_from_y_axis(y_radians);
	m3 z = m3_from_z_axis(z_radians);
	return mul(mul(x, y), z);
}

CUTE_MATH_INLINE m3 CUTE_MATH_CALL m3_from_euler_degrees_xyz(float x_degrees, float y_degrees, float z_degrees)
{
	return m3_from_euler_xyz(CUTE_MATH_DEG2RAD(x_degrees), CUTE_MATH_DEG2RAD(y_degrees), CUTE_MATH_DEG2RAD(z_degrees));
}

// -------------------------------------------------------------------------------------------------
// Transform operations.

struct transform
{
	v3 p; // position
	m3 r; // rotation
};

CUTE_MATH_INLINE v3 CUTE_MATH_CALL mul(transform tx, v3 a) { return mul(tx.r, a) + tx.p; }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL mul_transpose(transform tx, v3 a) { return mul(tx.r, a - tx.p); }

CUTE_MATH_INLINE transform CUTE_MATH_CALL mul(transform a, transform b)
{
	transform c;
	c.p = mul(a.r, b.p) + a.p;
	c.r = mul(a.r, b.r);
	return c;
}

CUTE_MATH_INLINE transform CUTE_MATH_CALL mul_transpose(transform a, transform b)
{
	transform c;
	c.p = mul_transpose(a.r, b.p - a.p);
	c.r = mul_transpose(a.r, b.r);
	return c;
}

struct halfspace
{
	v3 n;
	float d;
};

CUTE_MATH_INLINE v3 CUTE_MATH_CALL origin(halfspace h) { return h.n * h.d; }
CUTE_MATH_INLINE float CUTE_MATH_CALL distance(halfspace h, v3 p) { return dot(h.n, p) - h.d; }
CUTE_MATH_INLINE v3 CUTE_MATH_CALL projected(halfspace h, v3 p) { return p - h.n * distance(h, p); }

CUTE_MATH_INLINE halfspace CUTE_MATH_CALL mul(transform a, halfspace b)
{
	v3 o = origin(b);
	o = mul(a, o);
	v3 normal = mul(a.r, b.n);
	halfspace c;
	c.n = normal;
	c.d = dot(o, normal);
	return c;
}

CUTE_MATH_INLINE halfspace CUTE_MATH_CALL mul_transpose(transform a, halfspace b)
{
	v3 o = origin(b);
	o = mul_transpose(a, o);
	v3 normal = mul_transpose(a.r, b.n);
	halfspace c;
	c.n = normal;
	c.d = dot(o, normal);
	return c;
}

// -------------------------------------------------------------------------------------------------
// Quaternion operations.

struct q4
{
	q4() { }
	CUTE_MATH_INLINE explicit q4(v3 vector_part, float scalar_part) { m = _mm_set_ps(scalar_part, getz(vector_part), gety(vector_part), getx(vector_part)); }
	CUTE_MATH_INLINE explicit q4(float x, float y, float z, float w) { m = _mm_set_ps(w, z, y, x); }

	CUTE_MATH_INLINE operator __m128() { return m; }
	CUTE_MATH_INLINE operator __m128() const { return m; }

	__m128 m;
};

CUTE_MATH_INLINE q4 CUTE_MATH_CALL q4_from_axis_angle(v3 axis_normalized, float angle)
{
	float s = sinf(angle * 0.5f);
	float c = cosf(angle * 0.5f);
	return q4(axis_normalized * s, c);
}

CUTE_MATH_INLINE float CUTE_MATH_CALL getx(q4 a) { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(a, a, 0, 0, 0)); }
CUTE_MATH_INLINE float CUTE_MATH_CALL gety(q4 a) { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(a, a, 1, 1, 1)); }
CUTE_MATH_INLINE float CUTE_MATH_CALL getz(q4 a) { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(a, a, 2, 2, 2)); }
CUTE_MATH_INLINE float CUTE_MATH_CALL getw(q4 a) { return _mm_cvtss_f32(CUTE_MATH_SHUFFLE(a, a, 3, 3, 3)); }

// Optimize me.
CUTE_MATH_INLINE q4 CUTE_MATH_CALL norm(q4 q)
{
	float x = getx(q);
	float y = gety(q);
	float z = getz(q);
	float w = getw(q);

	float d = w * w + x * x + y * y + z * z;

	if(d == 0) w = 1.0f;
	d = 1.0f / sqrtf(d);

	if (d > 1.0e-8f) {
		x *= d;
		y *= d;
		z *= d;
		w *= d;
	}

	return q4(x, y, z, w);
}

// Optimize me.
CUTE_MATH_INLINE q4 CUTE_MATH_CALL operator*(q4 a, q4 b)
{
	return q4(
		getw(a) * getx(b) + getx(a) * getw(b) + gety(a) * getz(b) - getz(a) * gety(b),
		getw(a) * gety(b) + gety(a) * getw(b) + getz(a) * getx(b) - getx(a) * getz(b),
		getw(a) * getz(b) + getz(a) * getw(b) + getx(a) * gety(b) - gety(a) * getx(b),
		getw(a) * getw(b) - getx(a) * getx(b) - gety(a) * gety(b) - getz(a) * getz(b)
	);
}

// Optimize me.
CUTE_MATH_INLINE q4 CUTE_MATH_CALL integrate(q4 q, v3 w, float h)
{
	q4 wq(getx(w) * h, gety(w) * h, getz(w) * h, 0.0f);

	wq = wq * q;

	q4 q0 = q4(
		getx(q) + getx(wq) * 0.5f,
		gety(q) + gety(wq) * 0.5f,
		getz(q) + getz(wq) * 0.5f,
		getw(q) + getw(wq) * 0.5f
	);

	return norm(q0);
}

// Optimize me.
CUTE_MATH_INLINE m3 CUTE_MATH_CALL m3_from_q4(q4 q)
{
	return m3_from_quat(getx(q), gety(q), getz(q), getw(q));
}

CUTE_MATH_INLINE float CUTE_MATH_CALL trace(m3 m)
{
	return getx(m.x) + gety(m.y) + getz(m.z);
}

// -------------------------------------------------------------------------------------------------
// Globals.

CUTE_MATH_SELECTANY m3 identity_m3 = rows(v3(1.0f, 0.0f, 0.0f), v3(0.0f, 1.0f, 0.0f), v3(0.0f, 0.0f, 1.0f));
CUTE_MATH_SELECTANY m3 zero_m3 = rows(v3(0.0f, 0.0f, 0.0f), v3(0.0f, 0.0f, 0.0f), v3(0.0f, 0.0f, 0.0f));
CUTE_MATH_SELECTANY v3 zero_v3 = v3(0.0f, 0.0f, 0.0f);
CUTE_MATH_SELECTANY q4 identity_q4 = q4(0.0f, 0.0f, 0.0f, 1.0f);
CUTE_MATH_SELECTANY transform identity_transform = { zero_v3, identity_m3 };

// -------------------------------------------------------------------------------------------------
// Larger utility functions, defined in the `CUTE_MATH_IMPLEMENTATION` section.

void CUTE_MATH_CALL look_at(float* world_to_cam, v3 eye, v3 target, v3 up, float* cam_to_world = NULL);
void CUTE_MATH_CALL mul_vector4_by_matrix4x4(float* a_matrix4x4, float* b_vector4, float* out_vector);
void CUTE_MATH_CALL mul_matrix4x4_by_matrix4x4(float* a, float* b, float* out);
void CUTE_MATH_CALL compute_mouse_ray(float mouse_x, float mouse_y, float fov, float viewport_w, float viewport_h, float* cam_inv, float near_plane_dist, v3* mouse_pos, v3* mouse_dir);
void CUTE_MATH_CALL axis_angle_from_m3(m3 m, v3* axis, float* angle_radians);

#define CUTE_MATH_H
#endif

#ifdef CUTE_MATH_IMPLEMENTATION
#ifndef CUTE_MATH_IMPLEMENTATION_ONCE
#define CUTE_MATH_IMPLEMENTATION_ONCE

void CUTE_MATH_CALL look_at(float* world_to_cam, v3 eye, v3 target, v3 up, float* cam_to_world)
{
	v3 front = norm(target - eye);
	v3 side = norm(cross(front, up));
	v3 top = norm(cross(side, front));

	world_to_cam[0] = getx(side);
	world_to_cam[1] = getx(top);
	world_to_cam[2] = -getx(front);
	world_to_cam[3] = 0;

	world_to_cam[4] = gety(side);
	world_to_cam[5] = gety(top);
	world_to_cam[6] = -gety(front);
	world_to_cam[7] = 0;

	world_to_cam[8] = getz(side);
	world_to_cam[9] = getz(top);
	world_to_cam[10] = -getz(front);
	world_to_cam[11] = 0;

	v3 x = v3(world_to_cam[0], world_to_cam[4], world_to_cam[8]);
	v3 y = v3(world_to_cam[1], world_to_cam[5], world_to_cam[9]);
	v3 z = v3(world_to_cam[2], world_to_cam[6], world_to_cam[10]);

	world_to_cam[12] = -dot(x, eye);
	world_to_cam[13] = -dot(y, eye);
	world_to_cam[14] = -dot(z, eye);
	world_to_cam[15] = 1.0f;

	if (cam_to_world) {
		cam_to_world[0] = getx(side);
		cam_to_world[1] = gety(side);
		cam_to_world[2] = getz(side);
		cam_to_world[3] = 0;

		cam_to_world[4] = getx(top);
		cam_to_world[5] = gety(top);
		cam_to_world[6] = getz(top);
		cam_to_world[7] = 0;

		cam_to_world[8] = -getx(front);
		cam_to_world[9] = -gety(front);
		cam_to_world[10] = -getz(front);
		cam_to_world[11] = 0;

		cam_to_world[12] = getx(eye);
		cam_to_world[13] = gety(eye);
		cam_to_world[14] = getz(eye);
		cam_to_world[15] = 1.0f;
	}
}

void CUTE_MATH_CALL mul_vector4_by_matrix4x4(float* a_matrix4x4, float* b_vector4, float* out_vector)
{
	float result[4];

	result[0] = a_matrix4x4[0] * b_vector4[0] + a_matrix4x4[4] * b_vector4[1] + a_matrix4x4[8]  * b_vector4[2] + a_matrix4x4[12] * b_vector4[3];
	result[1] = a_matrix4x4[1] * b_vector4[0] + a_matrix4x4[5] * b_vector4[1] + a_matrix4x4[9]  * b_vector4[2] + a_matrix4x4[13] * b_vector4[3];
	result[2] = a_matrix4x4[2] * b_vector4[0] + a_matrix4x4[6] * b_vector4[1] + a_matrix4x4[10] * b_vector4[2] + a_matrix4x4[14] * b_vector4[3];
	result[3] = a_matrix4x4[3] * b_vector4[0] + a_matrix4x4[7] * b_vector4[1] + a_matrix4x4[11] * b_vector4[2] + a_matrix4x4[15] * b_vector4[3];

	out_vector[0] = result[0];
	out_vector[1] = result[1];
	out_vector[2] = result[2];
	out_vector[3] = result[3];
}

void CUTE_MATH_CALL mul_matrix4x4_by_matrix4x4(float* a, float* b, float* out)
{
	float result[16];

	mul_vector4_by_matrix4x4(a, b, result);
	mul_vector4_by_matrix4x4(a, b + 4, result + 4);
	mul_vector4_by_matrix4x4(a, b + 8, result + 8);
	mul_vector4_by_matrix4x4(a, b + 12, result + 12);

	for (int i = 0; i < 16; ++i) out[i] = result[i];
}

void CUTE_MATH_CALL compute_mouse_ray(float mouse_x, float mouse_y, float fov, float viewport_w, float viewport_h, float* cam_inv, float near_plane_dist, v3* mouse_pos, v3* mouse_dir)
{
	float aspect = (float)viewport_w / (float)viewport_h;
	float px = 2.0f * aspect * mouse_x / viewport_w - aspect;
	float py = -2.0f * mouse_y / viewport_h + 1.0f;
	float pz = -1.0f / tanf(fov / 2.0f);
	v3 point_in_view_space(px, py, pz);

	v3 cam_pos(cam_inv[12], cam_inv[13], cam_inv[14]);
	float pf[4] = { getx(point_in_view_space), gety(point_in_view_space), getz(point_in_view_space), 1.0f };
	mul_matrix4x4_by_matrix4x4(cam_inv, pf, pf);
	v3 point_on_clipping_plane(pf[0] , pf[1], pf[2]);
	v3 dir_in_world_space = point_on_clipping_plane - cam_pos;

	v3 dir = norm(dir_in_world_space);
	v3 cam_forward(cam_inv[8], cam_inv[9], cam_inv[10]);

	*mouse_dir = dir;
	*mouse_pos = cam_pos + dir * dot(dir, cam_forward) * near_plane_dist;
}

void CUTE_MATH_CALL axis_angle_from_m3(m3 m, v3* axis, float* angle_radians)
{
	const float k_tol = 1.0e-8f;
	float c = 0.5f * (trace(m) - 1.0f);
	float angle = acosf(c);
	*angle_radians = angle;

	bool angle_near_zero = fabsf(angle) < k_tol;
	bool angle_not_near_pi = angle < CUTE_MATH_PI - k_tol;
	if (angle_near_zero) {
		// When angle is zero the axis can be anything. X axis is good.
		*axis = v3(1, 0, 0);
	} else if (angle_not_near_pi) {
		// Standard case with no singularity.
		v3 n = v3(m[1][2] - m[2][1], m[2][0] - m[0][2], m[0][1] - m[1][0]);
		*axis = norm(n);
	} else {
		// Angle is near 180-degrees.
		int i = 0;
		if (m[1][1] > m[0][0]) i = 1;
		if (m[2][2] > m[i][i]) i = 2;
		int j = (i + 1) % 3;
		int k = (j + 1) % 3;
		float s = sqrtf(m[i][i] - m[j][j] - m[k][k] + 1.0f);
		float inv_s = s != 0 ? 1.0f / s : 0;
		float v[3];
		v[i] = 0.5f * s;
		v[j] = m[j][i] * inv_s;
		v[k] = m[i][k] * inv_s;
		*axis = v3(v[0], v[1], v[2]);
	}
}

#endif // CUTE_MATH_IMPLEMENTATION_ONCE
#endif // CUTE_MATH_IMPLEMENTATION

/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2019 Randy Gaul http://www.randygaul.net
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
