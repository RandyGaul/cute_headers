/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_math2d.h - v1.01

	SUMMARY:

		2d vector algebra implementation in C++. Makes use of operator
		overloading and function overloading, so not quite compatible with
		pure C.

		Here's a recommended alternative for pure C in 2d/3d:
		https://github.com/ferreiradaselva/mathc

	Note:

		No SIMD support. Many 2d applications run quite fast in scalar
		operations without the need for any vectorization. Adding SIMD support
		to cute_math2d would increase implementation difficulty and bloat the
		header quite a bit. As such the initial release went with pure scalar.

	Note:

		This header is basically a C++ port of the math from cute_c2.h:
		https://github.com/RandyGaul/cute_headers/blob/master/cute_c2.h

	Revision history:
		1.00 (11/02/2017) initial release
		1.01 (02/05/2019) rename majority of types to _t prefix
*/

#if !defined(CUTE_MATH2D_H)

// 2d vector
struct v2
{
	v2() {}
	v2(float x, float y) : x(x), y(y) {}
	float x;
	float y;
};

// 2d rotation composed of cos/sin pair
struct rotation_t
{
	float s;
	float c;
};

// 2d matrix
struct m2
{
	v2 x;
	v2 y;
};

// 2d affine transformation
struct transform_t
{
	rotation_t r;
	v2 p; // translation, or position
};

// 2d plane, aka line
struct halfspace_t
{
	v2 n;    // normal
	float d; // distance to origin; d = ax + by = dot(n, p)
};

struct ray_t
{
	v2 p;    // position
	v2 d;    // direction (normalized)
	float t; // distance along d from position p to find endpoint of ray
};

struct raycast_t
{
	float t; // time of impact
	v2 n;    // normal of surface at impact (unit length)
};

struct circle_t
{
	float r;
	v2 p;
};

struct aabb_t
{
	v2 min;
	v2 max;
};

#define CUTE_MATH2D_INLINE inline

#include <cmath>

// scalar ops

#ifndef CUTE_MATH_SCALAR_OPS
#define CUTE_MATH_SCALAR_OPS

CUTE_MATH2D_INLINE float min(float a, float b) { return a < b ? a : b; }
CUTE_MATH2D_INLINE float max(float a, float b) { return b < a ? a : b; }
CUTE_MATH2D_INLINE float clamp(float a, float lo, float hi) { return max(lo, min(a, hi)); }
CUTE_MATH2D_INLINE float sign(float a) { return a < 0 ? -1.0f : 1.0f; }
CUTE_MATH2D_INLINE float intersect(float da, float db) { return da / (da - db); }
CUTE_MATH2D_INLINE float invert_safe(float a) { return a != 0 ? 1.0f / a : 0; }

CUTE_MATH2D_INLINE int min(int a, int b) { return a < b ? a : b; }
CUTE_MATH2D_INLINE int max(int a, int b) { return b < a ? a : b; }
CUTE_MATH2D_INLINE int clamp(int a, int lo, int hi) { return max(lo, min(a, hi)); }
CUTE_MATH2D_INLINE int sign(int a) { return a < 0 ? -1 : 1; }

#endif // CUTE_MATH_SCALAR_OPS

// vector ops
CUTE_MATH2D_INLINE v2 operator+(v2 a, v2 b) { return v2(a.x + b.x, a.y + b.y); }
CUTE_MATH2D_INLINE v2 operator-(v2 a, v2 b) { return v2(a.x - b.x, a.y - b.y); }
CUTE_MATH2D_INLINE v2& operator+=(v2& a, v2 b) { return a = a + b; }
CUTE_MATH2D_INLINE v2& operator-=(v2& a, v2 b) { return a = a - b; }
CUTE_MATH2D_INLINE float dot(v2 a, v2 b) { return a.x * b.x + a.y * b.y; }
CUTE_MATH2D_INLINE v2 operator*(v2 a, float b) { return v2(a.x * b, a.y * b); }
CUTE_MATH2D_INLINE v2 operator*(v2 a, v2 b) { return v2(a.x * b.x, a.y * b.y); }
CUTE_MATH2D_INLINE v2& operator*=(v2& a, float b) { return a = a * b; }
CUTE_MATH2D_INLINE v2& operator*=(v2& a, v2 b) { return a = a * b; }
CUTE_MATH2D_INLINE v2 operator/(v2 a, float b) { return v2(a.x / b, a.y / b); }
CUTE_MATH2D_INLINE v2& operator/=(v2& a, float b) { return a = a / b; }
CUTE_MATH2D_INLINE v2 skew(v2 a) { return v2(-a.y, a.x); }
CUTE_MATH2D_INLINE v2 ccw90(v2 a) { return v2(a.y, -a.x); }
CUTE_MATH2D_INLINE float det2(v2 a, v2 b) { return a.x * b.y - a.y * b.x; }
CUTE_MATH2D_INLINE v2 min(v2 a, v2 b) { return v2(min(a.x, b.x), min(a.y, b.y)); }
CUTE_MATH2D_INLINE v2 max(v2 a, v2 b) { return v2(max(a.x, b.x), max(a.y, b.y)); }
CUTE_MATH2D_INLINE v2 clamp(v2 a, v2 lo, v2 hi) { return max(lo, min(a, hi)); }
CUTE_MATH2D_INLINE v2 abs(v2 a ) { return v2(abs(a.x), abs(a.y)); }
CUTE_MATH2D_INLINE float hmin(v2 a ) { return min(a.x, a.y); }
CUTE_MATH2D_INLINE float hmax(v2 a ) { return max(a.x, a.y); }
CUTE_MATH2D_INLINE float len(v2 a) { return sqrt(dot(a, a)); }
CUTE_MATH2D_INLINE float distance(v2 a, v2 b) { return sqrt(powf((a.x - b.x), 2) + powf((a.y - b.y), 2)); }
CUTE_MATH2D_INLINE v2 norm(v2 a) { return a / len(a); }
CUTE_MATH2D_INLINE v2 safe_norm(v2 a) { float sq = dot(a, a); return sq ? a / sqrt(sq) : v2(0, 0); }
CUTE_MATH2D_INLINE v2 operator-(v2 a) { return v2(-a.x, -a.y); }
CUTE_MATH2D_INLINE v2 lerp(v2 a, v2 b, float t) { return a + (b - a) * t; }
CUTE_MATH2D_INLINE int operator<(v2 a, v2 b) { return a.x < b.x && a.y < b.y; }
CUTE_MATH2D_INLINE int operator>(v2 a, v2 b) { return a.x > b.x && a.y > b.y; }
CUTE_MATH2D_INLINE int operator<=(v2 a, v2 b) { return a.x <= b.x && a.y <= b.y; }
CUTE_MATH2D_INLINE int operator>=(v2 a, v2 b) { return a.x >= b.x && a.y >= b.y; }
CUTE_MATH2D_INLINE v2 floor(v2 a) { return v2(floor(a.x), floor(a.y)); }
CUTE_MATH2D_INLINE v2 round(v2 a) { return v2(round(a.x), round(a.y)); }
CUTE_MATH2D_INLINE v2 invert_safe(v2 a) { return v2(invert_safe(a.x), invert_safe(a.y)); }

CUTE_MATH2D_INLINE int parallel(v2 a, v2 b, float tol)
{
	float k = len(a) / len(b);
	b =  b * k;
	if (abs(a.x - b.x) < tol && abs(a.y - b.y) < tol ) return 1;
	return 0;
}

// rotation ops
CUTE_MATH2D_INLINE rotation_t make_rotation(float radians) { rotation_t r; r.s = sin(radians); r.c = cos(radians); return r; }
CUTE_MATH2D_INLINE rotation_t make_rotation() { rotation_t r; r.c = 1.0f; r.s = 0; return r; }
CUTE_MATH2D_INLINE v2 x_axis(rotation_t r) { return v2(r.c, r.s); }
CUTE_MATH2D_INLINE v2 y_axis(rotation_t r) { return v2(-r.s, r.c); }
CUTE_MATH2D_INLINE v2 mul(rotation_t a, v2 b) { return v2(a.c * b.x - a.s * b.y,  a.s * b.x + a.c * b.y); }
CUTE_MATH2D_INLINE v2 mulT(rotation_t a, v2 b) { return v2(a.c * b.x + a.s * b.y, -a.s * b.x + a.c * b.y); }
CUTE_MATH2D_INLINE rotation_t mul(rotation_t a, rotation_t b)  { rotation_t c; c.c = a.c * b.c - a.s * b.s; c.s = a.s * b.c + a.c * b.s; return c; }
CUTE_MATH2D_INLINE rotation_t mulT(rotation_t a, rotation_t b) { rotation_t c; c.c = a.c * b.c + a.s * b.s; c.s = a.c * b.s - a.s * b.c; return c; }

CUTE_MATH2D_INLINE v2 mul(m2 a, v2 b)  { v2 c; c.x = a.x.x * b.x + a.y.x * b.y; c.y = a.x.y * b.x + a.y.y * b.y; return c; }
CUTE_MATH2D_INLINE v2 mulT(m2 a, v2 b) { v2 c; c.x = a.x.x * b.x + a.x.y * b.y; c.y = a.y.x * b.x + a.y.y * b.y; return c; }
CUTE_MATH2D_INLINE m2 mul(m2 a, m2 b)  { m2 c; c.x = mul(a,  b.x); c.y = mul(a,  b.y); return c; }
CUTE_MATH2D_INLINE m2 mulT(m2 a, m2 b) { m2 c; c.x = mulT(a, b.x); c.y = mulT(a, b.y); return c; }

// transform ops
CUTE_MATH2D_INLINE transform_t make_transform() { transform_t x; x.p = v2(0, 0); x.r = make_rotation(); return x; }
CUTE_MATH2D_INLINE transform_t make_transform(v2 p, float radians) { transform_t x; x.r = make_rotation(radians); x.p = p; return x; }
CUTE_MATH2D_INLINE v2 mul(transform_t a, v2 b) { return mul(a.r, b) + a.p; }
CUTE_MATH2D_INLINE v2 mulT(transform_t a, v2 b) { return mulT(a.r, b - a.p); }
CUTE_MATH2D_INLINE transform_t mul(transform_t a, transform_t b) { transform_t c; c.r = mul(a.r, b.r); c.p = mul( a.r, b.p ) + a.p; return c; }
CUTE_MATH2D_INLINE transform_t mulT(transform_t a, transform_t b) { transform_t c; c.r = mulT(a.r, b.r); c.p = mulT(a.r, b.p - a.p); return c; }

// halfspace ops
CUTE_MATH2D_INLINE v2 origin(halfspace_t h) { return h.n * h.d; }
CUTE_MATH2D_INLINE float distance(halfspace_t h, v2 p) { return dot(h.n, p) - h.d; }
CUTE_MATH2D_INLINE v2 project(halfspace_t h, v2 p) { return p - h.n * distance(h, p); }
CUTE_MATH2D_INLINE halfspace_t mul(transform_t a, halfspace_t b) { halfspace_t c; c.n = mul(a.r, b.n); c.d = dot(mul(a, origin(b) ), c.n); return c; }
CUTE_MATH2D_INLINE halfspace_t mulT(transform_t a, halfspace_t b) { halfspace_t c; c.n = mulT(a.r, b.n); c.d = dot(mulT(a, origin(b) ), c.n); return c; }
CUTE_MATH2D_INLINE v2 intersect(v2 a, v2 b, float da, float db) { return a + (b - a) * (da / (da - db)); }

// aabb helpers
CUTE_MATH2D_INLINE aabb_t make_aabb(v2 min, v2 max) { aabb_t bb; bb.min = min; bb.max = max; return bb; }
CUTE_MATH2D_INLINE aabb_t make_aabb(v2 pos, float w, float h) { aabb_t bb; v2 he = v2(w, h) * 0.5f; bb.min = pos - he; bb.max = pos + he; return bb; }
CUTE_MATH2D_INLINE aabb_t make_aabb_center_half_extents(v2 center, v2 half_extents) { aabb_t bb; bb.min = center - half_extents; bb.max = center + half_extents; return bb; }
CUTE_MATH2D_INLINE aabb_t make_aabb_from_top_left(v2 top_left, float w, float h) { return make_aabb(top_left + v2(0, -h), top_left + v2(w, 0)); }
CUTE_MATH2D_INLINE float width(aabb_t bb) { return bb.max.x - bb.min.x; }
CUTE_MATH2D_INLINE float height(aabb_t bb) { return bb.max.y - bb.min.y; }
CUTE_MATH2D_INLINE float half_width(aabb_t bb) { return width(bb) * 0.5f; }
CUTE_MATH2D_INLINE float half_height(aabb_t bb) { return height(bb) * 0.5f; }
CUTE_MATH2D_INLINE v2 half_extents(aabb_t bb) { return (bb.max - bb.min) * 0.5f; }
CUTE_MATH2D_INLINE v2 extents(aabb_t aabb) { return aabb.max - aabb.min; }
CUTE_MATH2D_INLINE aabb_t expand(aabb_t aabb, v2 v) { return make_aabb(aabb.min - v, aabb.max + v); }
CUTE_MATH2D_INLINE aabb_t expand(aabb_t aabb, float v) { v2 factor(v, v); return make_aabb(aabb.min - factor, aabb.max + factor); }
CUTE_MATH2D_INLINE v2 min(aabb_t bb) { return bb.min; }
CUTE_MATH2D_INLINE v2 max(aabb_t bb) { return bb.max; }
CUTE_MATH2D_INLINE v2 midpoint(aabb_t bb) { return (bb.min + bb.max) * 0.5f; }
CUTE_MATH2D_INLINE v2 center(aabb_t bb) { return (bb.min + bb.max) * 0.5f; }
CUTE_MATH2D_INLINE v2 top_left(aabb_t bb) { return v2(bb.min.x, bb.max.y); }
CUTE_MATH2D_INLINE v2 top_right(aabb_t bb) { return v2(bb.max.x, bb.max.y); }
CUTE_MATH2D_INLINE v2 bottom_left(aabb_t bb) { return v2(bb.min.x, bb.min.y); }
CUTE_MATH2D_INLINE v2 bottom_right(aabb_t bb) { return v2(bb.max.x, bb.min.y); }
CUTE_MATH2D_INLINE int contains(aabb_t bb, v2 p) { return p >= bb.min && p <= bb.max; }
CUTE_MATH2D_INLINE int contains(aabb_t a, aabb_t b) { return a.min >= b.min && a.max <= b.max; }
CUTE_MATH2D_INLINE float surface_area(aabb_t bb) { return 2.0f * width(bb) * height(bb); }
CUTE_MATH2D_INLINE float area(aabb_t bb) { return width(bb) * height(bb); }
CUTE_MATH2D_INLINE v2 clamp(aabb_t bb, v2 p) { return clamp(p, bb.min, bb.max); }
CUTE_MATH2D_INLINE aabb_t clamp(aabb_t a, aabb_t b) { return make_aabb(clamp(a.min, b.min, b.max), clamp(a.max, b.min, b.max)); }
CUTE_MATH2D_INLINE aabb_t combine(aabb_t a, aabb_t b) { return make_aabb(min(a.min, b.min), max(a.max, b.max)); }

CUTE_MATH2D_INLINE int overlaps(aabb_t a, aabb_t b)
{
	int d0 = b.max.x < a.min.x;
	int d1 = a.max.x < b.min.x;
	int d2 = b.max.y < a.min.y;
	int d3 = a.max.y < b.min.y;
	return !(d0 | d1 | d2 | d3);
}
CUTE_MATH2D_INLINE int collide(aabb_t a, aabb_t b) { return overlaps(a, b); }

CUTE_MATH2D_INLINE aabb_t make_aabb(v2* verts, int count)
{
	v2 min = verts[0];
	v2 max = min;
	for (int i = 0; i < count; ++i)
	{
		min = ::min(min, verts[i]);
		max = ::max(max, verts[i]);
	}
	return make_aabb(min, max);
}

CUTE_MATH2D_INLINE void aabb_verts(v2* out, aabb_t* bb)
{
	out[0] = bb->min;
	out[1] = v2(bb->max.x, bb->min.y);
	out[2] = bb->max;
	out[3] = v2(bb->min.x, bb->max.y);
}

// circle helpers
CUTE_MATH2D_INLINE float area(circle_t c) { return 3.14159265f * c.r * c.r; }
CUTE_MATH2D_INLINE float surface_area(circle_t c) { return 2.0f * 3.14159265f * c.r; }
CUTE_MATH2D_INLINE circle_t mul(transform_t tx, circle_t a) { circle_t b; b.p = mul(tx, a.p); b.r = a.r; return b; }

// ray ops
CUTE_MATH2D_INLINE v2 impact(ray_t r, float t) { return r.p + r.d * t; }
CUTE_MATH2D_INLINE v2 endpoint(ray_t r) { return r.p + r.d * r.t; }

CUTE_MATH2D_INLINE int ray_to_halfspace(ray_t A, halfspace_t B, raycast_t* out)
{
	float da = distance(B, A.p);
	float db = distance(B, impact(A, A.t));
	if (da * db > 0) return 0;
	out->n = B.n * sign(da);
	out->t = intersect(da, db);
}

CUTE_MATH2D_INLINE int ray_to_circle(ray_t A, circle_t B, raycast_t* out)
{
	v2 p = B.p;
	v2 m = A.p - p;
	float c = dot(m, m) - B.r * B.r;
	float b = dot(m, A.d);
	float disc = b * b - c;
	if (disc < 0) return 0;

	float t = -b - sqrt(disc);
	if (t >= 0 && t <= A.t)
	{
		out->t = t;
		v2 impact = ::impact(A, t);
		out->n = norm(impact - p);
		return 1;
	}
	return 0;
}

CUTE_MATH2D_INLINE int ray_to_aabb(ray_t A, aabb_t B, raycast_t* out)
{
	v2 inv = v2(1.0f / A.d.x, 1.0f / A.d.y);
	v2 d0 = (B.min - A.p) * inv;
	v2 d1 = (B.max - A.p) * inv;
	v2 v0 = min(d0, d1);
	v2 v1 = max(d0, d1);
	float lo = hmax(v0);
	float hi = hmin(v1);

	if (hi >= 0 && hi >= lo && lo <= A.t)
	{
		v2 c = midpoint(B);
		c = impact(A, lo) - c;
		v2 abs_c = abs(c);
		if (abs_c.x > abs_c.y) out->n = v2(sign(c.x), 0);
		else out->n = v2(0, sign(c.y));
		out->t = lo;
		return 1;
	}
	return 0;
}

#define CUTE_MATH2D_H
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
