/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_c2.h - v1.09

	To create implementation (the function definitions)
		#define CUTE_C2_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file


	SUMMARY

		cute_c2 is a single-file header that implements 2D collision detection routines
		that test for overlap, and optionally can find the collision manifold. The
		manifold contains all necessary information to prevent shapes from inter-
		penetrating, which is useful for character controllers, general physics
		simulation, and user-interface programming.

		This header implements a group of "immediate mode" functions that should be
		very easily adapted into pre-existing projects.


	THE IMPORTANT PARTS

		Most of the math types in this header are for internal use. Users care about
		the shape types and the collision functions.

		SHAPE TYPES:
		* c2Circle
		* c2Capsule
		* c2AABB
		* c2Ray
		* c2Poly

		COLLISION FUNCTIONS (*** is a shape name from the above list):
		* c2***to***         - boolean YES/NO hittest
		* c2***to***Manifold - construct manifold to describe how shapes hit
		* c2GJK              - runs GJK algorithm to find closest point pair between two shapes
		* c2TOI              - computes the time of impact between two shapes, useful for sweeping shapes, or doing shape casts
		* c2MakePoly         - Runs convex hull algorithm and computes normals on input point-set
		* c2Collided         - generic version of c2***to*** funcs
		* c2Collide          - generic version of c2***to***Manifold funcs
		* c2CastRay          - generic version of c2Rayto*** funcs

		The rest of the header is more or less for internal use. Here is an example of
		making some shapes and testing for collision:

			c2Circle c;
			c.p = position;
			c.r = radius;

			c2Capsule cap;
			cap.a = first_endpoint;
			cap.b = second_endpoint;
			cap.r = radius;

			int hit = c2CircletoCapsule(c, cap);
			if (hit)
			{
				handle collision here...
			}
	
		For more code examples and tests please see:
		https://github.com/RandyGaul/cute_header/tree/master/examples_cute_gl_and_c2

		Here is a past discussion thread on this header:
		https://www.reddit.com/r/gamedev/comments/5tqyey/tinyc2_2d_collision_detection_library_in_c/

		Here is a very nice repo containing various tests and examples using SFML for rendering:
		https://github.com/sro5h/tinyc2-tests


	FEATURES

		* Circles, capsules, AABBs, rays and convex polygons are supported
		* Fast boolean only result functions (hit yes/no)
		* Slghtly slower manifold generation for collision normals + depths +points
		* GJK implementation (finds closest points for disjoint pairs of shapes)
		* Shape casts/sweeps with c2TOI function (time of impact)
		* Robust 2D convex hull generator
		* Lots of correctly implemented and tested 2D math routines
		* Implemented in portable C, and is readily portable to other languages
		* Generic c2Collide, c2Collided and c2CastRay function (can pass in any shape type)
		* Extensive examples at: https://github.com/RandyGaul/cute_headers/tree/master/examples_cute_gl_and_c2


	Revision History
	
		1.0  (02/13/2017) initial release
		1.01 (02/13/2017) const crusade, minor optimizations, capsule degen
		1.02 (03/21/2017) compile fixes for c on more compilers
		1.03 (09/15/2017) various bugfixes and quality of life changes to manifolds
		1.04 (03/25/2018) fixed manifold bug in c2CircletoAABBManifold
		1.05 (11/01/2018) added c2TOI (time of impact) for shape cast/sweep test
		1.06 (08/23/2019) C2_*** types to C2_TYPE_***, and CUTE_C2_API
		1.07 (10/19/2019) Optimizations to c2TOI - breaking change to c2GJK API
		1.08 (12/22/2019) Remove contact point + normal from c2TOI, removed feather
		                  radius from c2GJK, fixed various bugs in capsule to poly
		                  manifold, did a pass on all docs
		1.09 (07/27/2019) Added c2Inflate - to inflate/deflate shapes for c2TOI


	Contributors

		Plastburk         1.01 - const pointers pull request
		mmozeiko          1.02 - 3 compile bugfixes
		felipefs          1.02 - 3 compile bugfixes
		seemk             1.02 - fix branching bug in c2Collide
		sro5h             1.02 - bug reports for multiple manifold funcs
		sro5h             1.03 - work involving quality of life fixes for manifolds
		Wizzard033        1.06 - C2_*** types to C2_TYPE_***, and CUTE_C2_API
		Tyler Glaeil      1.08 - Lots of bug reports and disussion on capsules + TOIs


	DETAILS/ADVICE

		BROAD PHASE

			This header does not implement a broad-phase, and instead concerns itself with
			the narrow-phase. This means this header just checks to see if two individual
			shapes are touching, and can give information about how they are touching.

			Very common 2D broad-phases are tree and grid approaches. Quad trees are good
			for static geometry that does not move much if at all. Dynamic AABB trees are
			good for general purpose use, and can handle moving objects very well. Grids
			are great and are similar to quad trees.

			If implementing a grid it can be wise to have each collideable grid cell hold
			an integer. This integer refers to a 2D shape that can be passed into the
			various functions in this header. The shape can be transformed from "model"
			space to "world" space using c2x -- a transform struct. In this way a grid
			can be implemented that holds any kind of convex shape (that this header
			supports) while conserving memory with shape instancing.

		NUMERIC ROBUSTNESS

			Many of the functions in cute c2 use `c2GJK`, an implementation of the GJK
			algorithm. Internally GJK computes signed area values, and these values are
			very numerically sensitive to large shapes. This means the GJK function will
			break down if input shapes are too large or too far away from the origin.

			In general it is best to compute collision detection on small shapes very
			close to the origin. One trick is to keep your collision information numerically
			very tiny, and simply scale it up when rendering to the appropriate size.

			For reference, if your shapes are all AABBs and contain a width and height
			of somewhere between 1.0f and 10.0f, everything will be fine. However, once
			your shapes start approaching a width/height of 100.0f to 1000.0f GJK can
			start breaking down.

			This is a complicated topic, so feel free to ask the author for advice here.

			Here is an example demonstrating this problem with two large AABBs:
			https://github.com/RandyGaul/cute_headers/issues/160

		Please email at my address with any questions or comments at:
		author's last name followed by 1748 at gmail
*/

#if !defined(CUTE_C2_H)

// this can be adjusted as necessary, but is highly recommended to be kept at 8.
// higher numbers will incur quite a bit of memory overhead, and convex shapes
// over 8 verts start to just look like spheres, which can be implicitly rep-
// resented as a point + radius. usually tools that generate polygons should be
// constructed so they do not output polygons with too many verts.
// Note: polygons in cute_c2 are all *convex*.
#define C2_MAX_POLYGON_VERTS 8

// 2d vector
typedef struct c2v
{
	float x;
	float y;
} c2v;

// 2d rotation composed of cos/sin pair
typedef struct c2r
{
	float c;
	float s;
} c2r;

// 2d rotation matrix
typedef struct c2m
{
	c2v x;
	c2v y;
} c2m;

// 2d transformation "x"
// These are used especially for c2Poly when a c2Poly is passed to a function.
// Since polygons are prime for "instancing" a c2x transform can be used to
// transform a polygon from local space to world space. In functions that take
// a c2x pointer (like c2PolytoPoly), these pointers can be NULL, which represents
// an identity transformation and assumes the verts inside of c2Poly are already
// in world space.
typedef struct c2x
{
	c2v p;
	c2r r;
} c2x;

// 2d halfspace (aka plane, aka line)
typedef struct c2h
{
	c2v n;   // normal, normalized
	float d; // distance to origin from plane, or ax + by = d
} c2h;

typedef struct c2Circle
{
	c2v p;
	float r;
} c2Circle;

typedef struct c2AABB
{
	c2v min;
	c2v max;
} c2AABB;

// a capsule is defined as a line segment (from a to b) and radius r
typedef struct c2Capsule
{
	c2v a;
	c2v b;
	float r;
} c2Capsule;

typedef struct c2Poly
{
	int count;
	c2v verts[C2_MAX_POLYGON_VERTS];
	c2v norms[C2_MAX_POLYGON_VERTS];
} c2Poly;

// IMPORTANT:
// Many algorithms in this file are sensitive to the magnitude of the
// ray direction (c2Ray::d). It is highly recommended to normalize the
// ray direction and use t to specify a distance. Please see this link
// for an in-depth explanation: https://github.com/RandyGaul/cute_headers/issues/30
typedef struct c2Ray
{
	c2v p;   // position
	c2v d;   // direction (normalized)
	float t; // distance along d from position p to find endpoint of ray
} c2Ray;

typedef struct c2Raycast
{
	float t; // time of impact
	c2v n;   // normal of surface at impact (unit length)
} c2Raycast;

// position of impact p = ray.p + ray.d * raycast.t
#define c2Impact(ray, t) c2Add(ray.p, c2Mulvs(ray.d, t))

// contains all information necessary to resolve a collision, or in other words
// this is the information needed to separate shapes that are colliding. Doing
// the resolution step is *not* included in cute_c2.
typedef struct c2Manifold
{
	int count;
	float depths[2];
	c2v contact_points[2];

	// always points from shape A to shape B (first and second shapes passed into
	// any of the c2***to***Manifold functions)
	c2v n;
} c2Manifold;

// This define allows exporting/importing of the header to a dynamic library.
// Here's an example.
// #define CUTE_C2_API extern "C" __declspec(dllexport)
#if !defined(CUTE_C2_API)
#	define CUTE_C2_API
#endif

// boolean collision detection
// these versions are faster than the manifold versions, but only give a YES/NO result
CUTE_C2_API int c2CircletoCircle(c2Circle A, c2Circle B);
CUTE_C2_API int c2CircletoAABB(c2Circle A, c2AABB B);
CUTE_C2_API int c2CircletoCapsule(c2Circle A, c2Capsule B);
CUTE_C2_API int c2AABBtoAABB(c2AABB A, c2AABB B);
CUTE_C2_API int c2AABBtoCapsule(c2AABB A, c2Capsule B);
CUTE_C2_API int c2CapsuletoCapsule(c2Capsule A, c2Capsule B);
CUTE_C2_API int c2CircletoPoly(c2Circle A, const c2Poly* B, const c2x* bx);
CUTE_C2_API int c2AABBtoPoly(c2AABB A, const c2Poly* B, const c2x* bx);
CUTE_C2_API int c2CapsuletoPoly(c2Capsule A, const c2Poly* B, const c2x* bx);
CUTE_C2_API int c2PolytoPoly(const c2Poly* A, const c2x* ax, const c2Poly* B, const c2x* bx);

// ray operations
// output is placed into the c2Raycast struct, which represents the hit location
// of the ray. the out param contains no meaningful information if these funcs
// return 0
CUTE_C2_API int c2RaytoCircle(c2Ray A, c2Circle B, c2Raycast* out);
CUTE_C2_API int c2RaytoAABB(c2Ray A, c2AABB B, c2Raycast* out);
CUTE_C2_API int c2RaytoCapsule(c2Ray A, c2Capsule B, c2Raycast* out);
CUTE_C2_API int c2RaytoPoly(c2Ray A, const c2Poly* B, const c2x* bx_ptr, c2Raycast* out);

// manifold generation
// these functions are (generally) slower than the boolean versions, but will compute one
// or two points that represent the plane of contact. This information is
// is usually needed to resolve and prevent shapes from colliding. If no coll
// ision occured the count member of the manifold struct is set to 0.
CUTE_C2_API void c2CircletoCircleManifold(c2Circle A, c2Circle B, c2Manifold* m);
CUTE_C2_API void c2CircletoAABBManifold(c2Circle A, c2AABB B, c2Manifold* m);
CUTE_C2_API void c2CircletoCapsuleManifold(c2Circle A, c2Capsule B, c2Manifold* m);
CUTE_C2_API void c2AABBtoAABBManifold(c2AABB A, c2AABB B, c2Manifold* m);
CUTE_C2_API void c2AABBtoCapsuleManifold(c2AABB A, c2Capsule B, c2Manifold* m);
CUTE_C2_API void c2CapsuletoCapsuleManifold(c2Capsule A, c2Capsule B, c2Manifold* m);
CUTE_C2_API void c2CircletoPolyManifold(c2Circle A, const c2Poly* B, const c2x* bx, c2Manifold* m);
CUTE_C2_API void c2AABBtoPolyManifold(c2AABB A, const c2Poly* B, const c2x* bx, c2Manifold* m);
CUTE_C2_API void c2CapsuletoPolyManifold(c2Capsule A, const c2Poly* B, const c2x* bx, c2Manifold* m);
CUTE_C2_API void c2PolytoPolyManifold(const c2Poly* A, const c2x* ax, const c2Poly* B, const c2x* bx, c2Manifold* m);

typedef enum
{
	C2_TYPE_NONE,
	C2_TYPE_CIRCLE,
	C2_TYPE_AABB,
	C2_TYPE_CAPSULE,
	C2_TYPE_POLY
} C2_TYPE;

// This struct is only for advanced usage of the c2GJK function. See comments inside of the
// c2GJK function for more details.
typedef struct c2GJKCache
{
	float metric;
	int count;
	int iA[3];
	int iB[3];
	float div;
} c2GJKCache;

// This is an advanced function, intended to be used by people who know what they're doing.
//
// Runs the GJK algorithm to find closest points, returns distance between closest points.
// outA and outB can be NULL, in this case only distance is returned. ax_ptr and bx_ptr
// can be NULL, and represent local to world transformations for shapes A and B respectively.
// use_radius will apply radii for capsules and circles (if set to false, spheres are
// treated as points and capsules are treated as line segments i.e. rays). The cache parameter
// should be NULL, as it is only for advanced usage (unless you know what you're doing, then
// go ahead and use it). iterations is an optional parameter.
//
// IMPORTANT NOTE:
// The GJK function is sensitive to large shapes, since it internally will compute signed area
// values. `c2GJK` is called throughout cute c2 in many ways, so try to make sure all of your
// collision shapes are not gigantic. For example, try to keep the volume of all your shapes
// less than 100.0f. If you need large shapes, you should use tiny collision geometry for all
// cute c2 function, and simply render the geometry larger on-screen by scaling it up.
CUTE_C2_API float c2GJK(const void* A, C2_TYPE typeA, const c2x* ax_ptr, const void* B, C2_TYPE typeB, const c2x* bx_ptr, c2v* outA, c2v* outB, int use_radius, int* iterations, c2GJKCache* cache);

// This is an advanced function, intended to be used by people who know what they're doing.
//
// Computes the time of impact from shape A and shape B. The velocity of each shape is provided
// by vA and vB respectively. The shapes are *not* allowed to rotate over time. The velocity is
// assumed to represent the change in motion from time 0 to time 1, and so the return value will
// be a number from 0 to 1. To move each shape to the colliding configuration, multiply vA and vB
// each by the return value. ax_ptr and bx_ptr are optional parameters to transforms for each shape,
// and are typically used for polygon shapes to transform from model to world space. Set these to
// NULL to represent identity transforms. iterations is an optional parameter. use_radius
// will apply radii for capsules and circles (if set to false, spheres are treated as points and
// capsules are treated as line segments i.e. rays).
//
// IMPORTANT NOTE:
// The c2TOI function can be used to implement a "swept character controller", but it can be
// difficult to do so. Say we compute a time of impact with `c2TOI` and move the shapes to the
// time of impact, and adjust the velocity by zeroing out the velocity along the surface normal.
// If we then call `c2TOI` again, it will fail since the shapes will be considered to start in
// a colliding configuration. There are many styles of tricks to get around this problem, and
// all of them involve giving the next call to `c2TOI` some breathing room. It is recommended
// to use some variation of the following algorithm:
//
// 1. Call c2TOI.
// 2. Move the shapes to the TOI.
// 3. Slightly inflate the size of one, or both, of the shapes so they will be intersecting.
//    The purpose is to make the shapes numerically intersecting, but not visually intersecting.
//    Another option is to call c2TOI with slightly deflated shapes.
//    See the function `c2Inflate` for some more details.
// 4. Compute the collision manifold between the inflated shapes (for example, use c2PolytoPolyManifold).
// 5. Gently push the shapes apart. This will give the next call to c2TOI some breathing room.
CUTE_C2_API float c2TOI(const void* A, C2_TYPE typeA, const c2x* ax_ptr, c2v vA, const void* B, C2_TYPE typeB, const c2x* bx_ptr, c2v vB, int use_radius, int* iterations);

// Inflating a shape.
//
// This is useful to numerically grow or shrink a polytope. For example, when calling
// a time of impact function it can be good to use a slightly smaller shape. Then, once
// both shapes are moved to the time of impact a collision manifold can be made from the
// slightly larger (and now overlapping) shapes.
//
// IMPORTANT NOTE
// Inflating a shape with sharp corners can cause those corners to move dramatically.
// Deflating a shape can avoid this problem, but deflating a very small shape can invert
// the planes and result in something that is no longer convex. Make sure to pick an
// appropriately small skin factor, for example 1.0e-6f.
CUTE_C2_API void c2Inflate(void* shape, C2_TYPE type, float skin_factor);

// Computes 2D convex hull. Will not do anything if less than two verts supplied. If
// more than C2_MAX_POLYGON_VERTS are supplied extras are ignored.
CUTE_C2_API int c2Hull(c2v* verts, int count);
CUTE_C2_API void c2Norms(c2v* verts, c2v* norms, int count);

// runs c2Hull and c2Norms, assumes p->verts and p->count are both set to valid values
CUTE_C2_API void c2MakePoly(c2Poly* p);

// Generic collision detection routines, useful for games that want to use some poly-
// morphism to write more generic-styled code. Internally calls various above functions.
// For AABBs/Circles/Capsules ax and bx are ignored. For polys ax and bx can define
// model to world transformations (for polys only), or be NULL for identity transforms.
CUTE_C2_API int c2Collided(const void* A, const c2x* ax, C2_TYPE typeA, const void* B, const c2x* bx, C2_TYPE typeB);
CUTE_C2_API void c2Collide(const void* A, const c2x* ax, C2_TYPE typeA, const void* B, const c2x* bx, C2_TYPE typeB, c2Manifold* m);
CUTE_C2_API int c2CastRay(c2Ray A, const void* B, const c2x* bx, C2_TYPE typeB, c2Raycast* out);

#ifdef _MSC_VER
	#define C2_INLINE __forceinline
#else
	#define C2_INLINE inline __attribute__((always_inline))
#endif

// adjust these primitives as seen fit
#include <string.h> // memcpy
#include <math.h>
#define c2Sin(radians) sinf(radians)
#define c2Cos(radians) cosf(radians)
#define c2Sqrt(a) sqrtf(a)
#define c2Min(a, b) ((a) < (b) ? (a) : (b))
#define c2Max(a, b) ((a) > (b) ? (a) : (b))
#define c2Abs(a) ((a) < 0 ? -(a) : (a))
#define c2Clamp(a, lo, hi) c2Max(lo, c2Min(a, hi))
C2_INLINE void c2SinCos(float radians, float* s, float* c) { *c = c2Cos(radians); *s = c2Sin(radians); }
#define c2Sign(a) (a < 0 ? -1.0f : 1.0f)

// The rest of the functions in the header-only portion are all for internal use
// and use the author's personal naming conventions. It is recommended to use one's
// own math library instead of the one embedded here in cute_c2, but for those
// curious or interested in trying it out here's the details:

// The Mul functions are used to perform multiplication. x stands for transform,
// v stands for vector, s stands for scalar, r stands for rotation, h stands for
// halfspace and T stands for transpose.For example c2MulxvT stands for "multiply
// a transform with a vector, and transpose the transform".

// vector ops
C2_INLINE c2v c2V(float x, float y) { c2v a; a.x = x; a.y = y; return a; }
C2_INLINE c2v c2Add(c2v a, c2v b) { a.x += b.x; a.y += b.y; return a; }
C2_INLINE c2v c2Sub(c2v a, c2v b) { a.x -= b.x; a.y -= b.y; return a; }
C2_INLINE float c2Dot(c2v a, c2v b) { return a.x * b.x + a.y * b.y; }
C2_INLINE c2v c2Mulvs(c2v a, float b) { a.x *= b; a.y *= b; return a; }
C2_INLINE c2v c2Mulvv(c2v a, c2v b) { a.x *= b.x; a.y *= b.y; return a; }
C2_INLINE c2v c2Div(c2v a, float b) { return c2Mulvs(a, 1.0f / b); }
C2_INLINE c2v c2Skew(c2v a) { c2v b; b.x = -a.y; b.y = a.x; return b; }
C2_INLINE c2v c2CCW90(c2v a) { c2v b; b.x = a.y; b.y = -a.x; return b; }
C2_INLINE float c2Det2(c2v a, c2v b) { return a.x * b.y - a.y * b.x; }
C2_INLINE c2v c2Minv(c2v a, c2v b) { return c2V(c2Min(a.x, b.x), c2Min(a.y, b.y)); }
C2_INLINE c2v c2Maxv(c2v a, c2v b) { return c2V(c2Max(a.x, b.x), c2Max(a.y, b.y)); }
C2_INLINE c2v c2Clampv(c2v a, c2v lo, c2v hi) { return c2Maxv(lo, c2Minv(a, hi)); }
C2_INLINE c2v c2Absv(c2v a) { return c2V(c2Abs(a.x), c2Abs(a.y)); }
C2_INLINE float c2Hmin(c2v a) { return c2Min(a.x, a.y); }
C2_INLINE float c2Hmax(c2v a) { return c2Max(a.x, a.y); }
C2_INLINE float c2Len(c2v a) { return c2Sqrt(c2Dot(a, a)); }
C2_INLINE c2v c2Norm(c2v a) { return c2Div(a, c2Len(a)); }
C2_INLINE c2v c2SafeNorm(c2v a) { float sq = c2Dot(a, a); return sq ? c2Div(a, c2Len(a)) : c2V(0, 0); }
C2_INLINE c2v c2Neg(c2v a) { return c2V(-a.x, -a.y); }
C2_INLINE c2v c2Lerp(c2v a, c2v b, float t) { return c2Add(a, c2Mulvs(c2Sub(b, a), t)); }
C2_INLINE int c2Parallel(c2v a, c2v b, float kTol)
{
	float k = c2Len(a) / c2Len(b);
	b = c2Mulvs(b, k);
	if (c2Abs(a.x - b.x) < kTol && c2Abs(a.y - b.y) < kTol) return 1;
	return 0;
}

// rotation ops
C2_INLINE c2r c2Rot(float radians) { c2r r; c2SinCos(radians, &r.s, &r.c); return r; }
C2_INLINE c2r c2RotIdentity() { c2r r; r.c = 1.0f; r.s = 0; return r; }
C2_INLINE c2v c2RotX(c2r r) { return c2V(r.c, r.s); }
C2_INLINE c2v c2RotY(c2r r) { return c2V(-r.s, r.c); }
C2_INLINE c2v c2Mulrv(c2r a, c2v b)  { return c2V(a.c * b.x - a.s * b.y,  a.s * b.x + a.c * b.y); }
C2_INLINE c2v c2MulrvT(c2r a, c2v b) { return c2V(a.c * b.x + a.s * b.y, -a.s * b.x + a.c * b.y); }
C2_INLINE c2r c2Mulrr(c2r a, c2r b)  { c2r c; c.c = a.c * b.c - a.s * b.s; c.s = a.s * b.c + a.c * b.s; return c; }
C2_INLINE c2r c2MulrrT(c2r a, c2r b) { c2r c; c.c = a.c * b.c + a.s * b.s; c.s = a.c * b.s - a.s * b.c; return c; }

C2_INLINE c2v c2Mulmv(c2m a, c2v b) { c2v c; c.x = a.x.x * b.x + a.y.x * b.y; c.y = a.x.y * b.x + a.y.y * b.y; return c; }
C2_INLINE c2v c2MulmvT(c2m a, c2v b) { c2v c; c.x = a.x.x * b.x + a.x.y * b.y; c.y = a.y.x * b.x + a.y.y * b.y; return c; }
C2_INLINE c2m c2Mulmm(c2m a, c2m b)  { c2m c; c.x = c2Mulmv(a, b.x);  c.y = c2Mulmv(a, b.y); return c; }
C2_INLINE c2m c2MulmmT(c2m a, c2m b) { c2m c; c.x = c2MulmvT(a, b.x); c.y = c2MulmvT(a, b.y); return c; }

// transform ops
C2_INLINE c2x c2xIdentity() { c2x x; x.p = c2V(0, 0); x.r = c2RotIdentity(); return x; }
C2_INLINE c2v c2Mulxv(c2x a, c2v b) { return c2Add(c2Mulrv(a.r, b), a.p); }
C2_INLINE c2v c2MulxvT(c2x a, c2v b) { return c2MulrvT(a.r, c2Sub(b, a.p)); }
C2_INLINE c2x c2Mulxx(c2x a, c2x b) { c2x c; c.r = c2Mulrr(a.r, b.r); c.p = c2Add(c2Mulrv(a.r, b.p), a.p); return c; }
C2_INLINE c2x c2MulxxT(c2x a, c2x b) { c2x c; c.r = c2MulrrT(a.r, b.r); c.p = c2MulrvT(a.r, c2Sub(b.p, a.p)); return c; }
C2_INLINE c2x c2Transform(c2v p, float radians) { c2x x; x.r = c2Rot(radians); x.p = p; return x; }

// halfspace ops
C2_INLINE c2v c2Origin(c2h h) { return c2Mulvs(h.n, h.d); }
C2_INLINE float c2Dist(c2h h, c2v p) { return c2Dot(h.n, p) - h.d; }
C2_INLINE c2v c2Project(c2h h, c2v p) { return c2Sub(p, c2Mulvs(h.n, c2Dist(h, p))); }
C2_INLINE c2h c2Mulxh(c2x a, c2h b) { c2h c; c.n = c2Mulrv(a.r, b.n); c.d = c2Dot(c2Mulxv(a, c2Origin(b)), c.n); return c; }
C2_INLINE c2h c2MulxhT(c2x a, c2h b) { c2h c; c.n = c2MulrvT(a.r, b.n); c.d = c2Dot(c2MulxvT(a, c2Origin(b)), c.n); return c; }
C2_INLINE c2v c2Intersect(c2v a, c2v b, float da, float db) { return c2Add(a, c2Mulvs(c2Sub(b, a), (da / (da - db)))); }

C2_INLINE void c2BBVerts(c2v* out, c2AABB* bb)
{
	out[0] = bb->min;
	out[1] = c2V(bb->max.x, bb->min.y);
	out[2] = bb->max;
	out[3] = c2V(bb->min.x, bb->max.y);
}

#define CUTE_C2_H
#endif

#ifdef CUTE_C2_IMPLEMENTATION
#ifndef CUTE_C2_IMPLEMENTATION_ONCE
#define CUTE_C2_IMPLEMENTATION_ONCE

int c2Collided(const void* A, const c2x* ax, C2_TYPE typeA, const void* B, const c2x* bx, C2_TYPE typeB)
{
	switch (typeA)
	{
	case C2_TYPE_CIRCLE:
		switch (typeB)
		{
		case C2_TYPE_CIRCLE:  return c2CircletoCircle(*(c2Circle*)A, *(c2Circle*)B);
		case C2_TYPE_AABB:    return c2CircletoAABB(*(c2Circle*)A, *(c2AABB*)B);
		case C2_TYPE_CAPSULE: return c2CircletoCapsule(*(c2Circle*)A, *(c2Capsule*)B);
		case C2_TYPE_POLY:    return c2CircletoPoly(*(c2Circle*)A, (const c2Poly*)B, bx);
		default:         return 0;
		}
		break;

	case C2_TYPE_AABB:
		switch (typeB)
		{
		case C2_TYPE_CIRCLE:  return c2CircletoAABB(*(c2Circle*)B, *(c2AABB*)A);
		case C2_TYPE_AABB:    return c2AABBtoAABB(*(c2AABB*)A, *(c2AABB*)B);
		case C2_TYPE_CAPSULE: return c2AABBtoCapsule(*(c2AABB*)A, *(c2Capsule*)B);
		case C2_TYPE_POLY:    return c2AABBtoPoly(*(c2AABB*)A, (const c2Poly*)B, bx);
		default:         return 0;
		}
		break;

	case C2_TYPE_CAPSULE:
		switch (typeB)
		{
		case C2_TYPE_CIRCLE:  return c2CircletoCapsule(*(c2Circle*)B, *(c2Capsule*)A);
		case C2_TYPE_AABB:    return c2AABBtoCapsule(*(c2AABB*)B, *(c2Capsule*)A);
		case C2_TYPE_CAPSULE: return c2CapsuletoCapsule(*(c2Capsule*)A, *(c2Capsule*)B);
		case C2_TYPE_POLY:    return c2CapsuletoPoly(*(c2Capsule*)A, (const c2Poly*)B, bx);
		default:         return 0;
		}
		break;

	case C2_TYPE_POLY:
		switch (typeB)
		{
		case C2_TYPE_CIRCLE:  return c2CircletoPoly(*(c2Circle*)B, (const c2Poly*)A, ax);
		case C2_TYPE_AABB:    return c2AABBtoPoly(*(c2AABB*)B, (const c2Poly*)A, ax);
		case C2_TYPE_CAPSULE: return c2CapsuletoPoly(*(c2Capsule*)B, (const c2Poly*)A, ax);
		case C2_TYPE_POLY:    return c2PolytoPoly((const c2Poly*)A, ax, (const c2Poly*)B, bx);
		default:         return 0;
		}
		break;

	default:
		return 0;
	}
}

void c2Collide(const void* A, const c2x* ax, C2_TYPE typeA, const void* B, const c2x* bx, C2_TYPE typeB, c2Manifold* m)
{
	m->count = 0;

	switch (typeA)
	{
	case C2_TYPE_CIRCLE:
		switch (typeB)
		{
		case C2_TYPE_CIRCLE:  c2CircletoCircleManifold(*(c2Circle*)A, *(c2Circle*)B, m); break;
		case C2_TYPE_AABB:    c2CircletoAABBManifold(*(c2Circle*)A, *(c2AABB*)B, m); break;
		case C2_TYPE_CAPSULE: c2CircletoCapsuleManifold(*(c2Circle*)A, *(c2Capsule*)B, m); break;
		case C2_TYPE_POLY:    c2CircletoPolyManifold(*(c2Circle*)A, (const c2Poly*)B, bx, m); break;
		}
		break;

	case C2_TYPE_AABB:
		switch (typeB)
		{
		case C2_TYPE_CIRCLE:  c2CircletoAABBManifold(*(c2Circle*)B, *(c2AABB*)A, m); m->n = c2Neg(m->n); break;
		case C2_TYPE_AABB:    c2AABBtoAABBManifold(*(c2AABB*)A, *(c2AABB*)B, m); break;
		case C2_TYPE_CAPSULE: c2AABBtoCapsuleManifold(*(c2AABB*)A, *(c2Capsule*)B, m); break;
		case C2_TYPE_POLY:    c2AABBtoPolyManifold(*(c2AABB*)A, (const c2Poly*)B, bx, m); break;
		}
		break;

	case C2_TYPE_CAPSULE:
		switch (typeB)
		{
		case C2_TYPE_CIRCLE:  c2CircletoCapsuleManifold(*(c2Circle*)B, *(c2Capsule*)A, m); m->n = c2Neg(m->n); break;
		case C2_TYPE_AABB:    c2AABBtoCapsuleManifold(*(c2AABB*)B, *(c2Capsule*)A, m); m->n = c2Neg(m->n); break;
		case C2_TYPE_CAPSULE: c2CapsuletoCapsuleManifold(*(c2Capsule*)A, *(c2Capsule*)B, m); break;
		case C2_TYPE_POLY:    c2CapsuletoPolyManifold(*(c2Capsule*)A, (const c2Poly*)B, bx, m); break;
		}
		break;

	case C2_TYPE_POLY:
		switch (typeB)
		{
		case C2_TYPE_CIRCLE:  c2CircletoPolyManifold(*(c2Circle*)B, (const c2Poly*)A, ax, m); m->n = c2Neg(m->n); break;
		case C2_TYPE_AABB:    c2AABBtoPolyManifold(*(c2AABB*)B, (const c2Poly*)A, ax, m); m->n = c2Neg(m->n); break;
		case C2_TYPE_CAPSULE: c2CapsuletoPolyManifold(*(c2Capsule*)B, (const c2Poly*)A, ax, m); m->n = c2Neg(m->n); break;
		case C2_TYPE_POLY:    c2PolytoPolyManifold((const c2Poly*)A, ax, (const c2Poly*)B, bx, m); break;
		}
		break;
	}
}

int c2CastRay(c2Ray A, const void* B, const c2x* bx, C2_TYPE typeB, c2Raycast* out)
{
	switch (typeB)
	{
	case C2_TYPE_CIRCLE:  return c2RaytoCircle(A, *(c2Circle*)B, out);
	case C2_TYPE_AABB:    return c2RaytoAABB(A, *(c2AABB*)B, out);
	case C2_TYPE_CAPSULE: return c2RaytoCapsule(A, *(c2Capsule*)B, out);
	case C2_TYPE_POLY:    return c2RaytoPoly(A, (const c2Poly*)B, bx, out);
	}

	return 0;
}

#define C2_GJK_ITERS 20

typedef struct
{
	float radius;
	int count;
	c2v verts[C2_MAX_POLYGON_VERTS];
} c2Proxy;

typedef struct
{
	c2v sA;
	c2v sB;
	c2v p;
	float u;
	int iA;
	int iB;
} c2sv;

typedef struct
{
	c2sv a, b, c, d;
	float div;
	int count;
} c2Simplex;

static C2_INLINE void c2MakeProxy(const void* shape, C2_TYPE type, c2Proxy* p)
{
	switch (type)
	{
	case C2_TYPE_CIRCLE:
	{
		c2Circle* c = (c2Circle*)shape;
		p->radius = c->r;
		p->count = 1;
		p->verts[0] = c->p;
	}	break;

	case C2_TYPE_AABB:
	{
		c2AABB* bb = (c2AABB*)shape;
		p->radius = 0;
		p->count = 4;
		c2BBVerts(p->verts, bb);
	}	break;

	case C2_TYPE_CAPSULE:
	{
		c2Capsule* c = (c2Capsule*)shape;
		p->radius = c->r;
		p->count = 2;
		p->verts[0] = c->a;
		p->verts[1] = c->b;
	}	break;

	case C2_TYPE_POLY:
	{
		c2Poly* poly = (c2Poly*)shape;
		p->radius = 0;
		p->count = poly->count;
		for (int i = 0; i < p->count; ++i) p->verts[i] = poly->verts[i];
	}	break;
	}
}

static C2_INLINE int c2Support(const c2v* verts, int count, c2v d)
{
	int imax = 0;
	float dmax = c2Dot(verts[0], d);

	for (int i = 1; i < count; ++i)
	{
		float dot = c2Dot(verts[i], d);
		if (dot > dmax)
		{
			imax = i;
			dmax = dot;
		}
	}

	return imax;
}

#define C2_BARY(n, x) c2Mulvs(s->n.x, (den * s->n.u))
#define C2_BARY2(x) c2Add(C2_BARY(a, x), C2_BARY(b, x))
#define C2_BARY3(x) c2Add(c2Add(C2_BARY(a, x), C2_BARY(b, x)), C2_BARY(c, x))

static C2_INLINE c2v c2L(c2Simplex* s)
{
	float den = 1.0f / s->div;
	switch (s->count)
	{
	case 1: return s->a.p;
	case 2: return C2_BARY2(p);
	case 3: return C2_BARY3(p);
	default: return c2V(0, 0);
	}
}

static C2_INLINE void c2Witness(c2Simplex* s, c2v* a, c2v* b)
{
	float den = 1.0f / s->div;
	switch (s->count)
	{
	case 1: *a = s->a.sA; *b = s->a.sB; break;
	case 2: *a = C2_BARY2(sA); *b = C2_BARY2(sB); break;
	case 3: *a = C2_BARY3(sA); *b = C2_BARY3(sB); break;
	default: *a = c2V(0, 0); *b = c2V(0, 0);
	}
}

static C2_INLINE c2v c2D(c2Simplex* s)
{
	switch (s->count)
	{
	case 1: return c2Neg(s->a.p);
	case 2:
	{
		c2v ab = c2Sub(s->b.p, s->a.p);
		if (c2Det2(ab, c2Neg(s->a.p)) > 0) return c2Skew(ab);
		return c2CCW90(ab);
	}
	case 3:
	default: return c2V(0, 0);
	}
}

static C2_INLINE void c22(c2Simplex* s)
{
	c2v a = s->a.p;
	c2v b = s->b.p;
	float u = c2Dot(b, c2Sub(b, a));
	float v = c2Dot(a, c2Sub(a, b));

	if (v <= 0)
	{
		s->a.u = 1.0f;
		s->div = 1.0f;
		s->count = 1;
	}

	else if (u <= 0)
	{
		s->a = s->b;
		s->a.u = 1.0f;
		s->div = 1.0f;
		s->count = 1;
	}

	else
	{
		s->a.u = u;
		s->b.u = v;
		s->div = u + v;
		s->count = 2;
	}
}

static C2_INLINE void c23(c2Simplex* s)
{
	c2v a = s->a.p;
	c2v b = s->b.p;
	c2v c = s->c.p;

	float uAB = c2Dot(b, c2Sub(b, a));
	float vAB = c2Dot(a, c2Sub(a, b));
	float uBC = c2Dot(c, c2Sub(c, b));
	float vBC = c2Dot(b, c2Sub(b, c));
	float uCA = c2Dot(a, c2Sub(a, c));
	float vCA = c2Dot(c, c2Sub(c, a));
	float area = c2Det2(c2Sub(b, a), c2Sub(c, a));
	float uABC = c2Det2(b, c) * area;
	float vABC = c2Det2(c, a) * area;
	float wABC = c2Det2(a, b) * area;

	if (vAB <= 0 && uCA <= 0)
	{
		s->a.u = 1.0f;
		s->div = 1.0f;
		s->count = 1;
	}

	else if (uAB <= 0 && vBC <= 0)
	{
		s->a = s->b;
		s->a.u = 1.0f;
		s->div = 1.0f;
		s->count = 1;
	}

	else if (uBC <= 0 && vCA <= 0)
	{
		s->a = s->c;
		s->a.u = 1.0f;
		s->div = 1.0f;
		s->count = 1;
	}

	else if (uAB > 0 && vAB > 0 && wABC <= 0)
	{
		s->a.u = uAB;
		s->b.u = vAB;
		s->div = uAB + vAB;
		s->count = 2;
	}

	else if (uBC > 0 && vBC > 0 && uABC <= 0)
	{
		s->a = s->b;
		s->b = s->c;
		s->a.u = uBC;
		s->b.u = vBC;
		s->div = uBC + vBC;
		s->count = 2;
	}

	else if (uCA > 0 && vCA > 0 && vABC <= 0)
	{
		s->b = s->a;
		s->a = s->c;
		s->a.u = uCA;
		s->b.u = vCA;
		s->div = uCA + vCA;
		s->count = 2;
	}

	else
	{
		s->a.u = uABC;
		s->b.u = vABC;
		s->c.u = wABC;
		s->div = uABC + vABC + wABC;
		s->count = 3;
	}
}

#include <float.h>

static C2_INLINE float c2GJKSimplexMetric(c2Simplex* s)
{
	switch (s->count)
	{
	default: // fall through
	case 1:  return 0;
	case 2:  return c2Len(c2Sub(s->b.p, s->a.p));
	case 3:  return c2Det2(c2Sub(s->b.p, s->a.p), c2Sub(s->c.p, s->a.p));
	}
}

// Please see http://box2d.org/downloads/ under GDC 2010 for Erin's demo code
// and PDF slides for documentation on the GJK algorithm. This function is mostly
// from Erin's version from his online resources.
float c2GJK(const void* A, C2_TYPE typeA, const c2x* ax_ptr, const void* B, C2_TYPE typeB, const c2x* bx_ptr, c2v* outA, c2v* outB, int use_radius, int* iterations, c2GJKCache* cache)
{
	c2x ax;
	c2x bx;
	if (!ax_ptr) ax = c2xIdentity();
	else ax = *ax_ptr;
	if (!bx_ptr) bx = c2xIdentity();
	else bx = *bx_ptr;

	c2Proxy pA;
	c2Proxy pB;
	c2MakeProxy(A, typeA, &pA);
	c2MakeProxy(B, typeB, &pB);

	c2Simplex s;
	c2sv* verts = &s.a;

	// Metric and caching system as designed by E. Catto in Box2D for his conservative advancment/bilateral
	// advancement algorithim implementations. The purpose is to reuse old simplex indices (any simplex that
	// have not degenerated into a line or point) as a starting point. This skips the first few iterations of
	// GJK going from point, to line, to triangle, lowering convergence rates dramatically for temporally
	// coherent cases (such as in time of impact searches).
	int cache_was_read = 0;
	if (cache)
	{
		int cache_was_good = !!cache->count;

		if (cache_was_good)
		{
			for (int i = 0; i < cache->count; ++i)
			{
				int iA = cache->iA[i];
				int iB = cache->iB[i];
				c2v sA = c2Mulxv(ax, pA.verts[iA]);
				c2v sB = c2Mulxv(bx, pB.verts[iB]);
				c2sv* v = verts + i;
				v->iA = iA;
				v->sA = sA;
				v->iB = iB;
				v->sB = sB;
				v->p = c2Sub(v->sB, v->sA);
				v->u = 0;
			}
			s.count = cache->count;
			s.div = cache->div;

			float metric_old = cache->metric;
			float metric = c2GJKSimplexMetric(&s);

			float min_metric = metric < metric_old ? metric : metric_old;
			float max_metric = metric > metric_old ? metric : metric_old;

			if (!(min_metric < max_metric * 2.0f && metric < -1.0e8f)) cache_was_read = 1;
		}
	}

	if (!cache_was_read)
	{
		s.a.iA = 0;
		s.a.iB = 0;
		s.a.sA = c2Mulxv(ax, pA.verts[0]);
		s.a.sB = c2Mulxv(bx, pB.verts[0]);
		s.a.p = c2Sub(s.a.sB, s.a.sA);
		s.a.u = 1.0f;
		s.div = 1.0f;
		s.count = 1;
	}

	int saveA[3], saveB[3];
	int save_count = 0;
	float d0 = FLT_MAX;
	float d1 = FLT_MAX;
	int iter = 0;
	int hit = 0;
	while (iter < C2_GJK_ITERS)
	{
		save_count = s.count;
		for (int i = 0; i < save_count; ++i)
		{
			saveA[i] = verts[i].iA;
			saveB[i] = verts[i].iB;
		}
		
		switch (s.count)
		{
		case 1: break;
		case 2: c22(&s); break;
		case 3: c23(&s); break;
		}

		if (s.count == 3)
		{
			hit = 1;
			break;
		}

		c2v p = c2L(&s);
		d1 = c2Dot(p, p);

		if (d1 > d0) break;
		d0 = d1;

		c2v d = c2D(&s);
		if (c2Dot(d, d) < FLT_EPSILON * FLT_EPSILON) break;

		int iA = c2Support(pA.verts, pA.count, c2MulrvT(ax.r, c2Neg(d)));
		c2v sA = c2Mulxv(ax, pA.verts[iA]);
		int iB = c2Support(pB.verts, pB.count, c2MulrvT(bx.r, d));
		c2v sB = c2Mulxv(bx, pB.verts[iB]);

		c2sv* v = verts + s.count;
		v->iA = iA;
		v->sA = sA;
		v->iB = iB;
		v->sB = sB;
		v->p = c2Sub(v->sB, v->sA);

		int dup = 0;
		for (int i = 0; i < save_count; ++i)
		{
			if (iA == saveA[i] && iB == saveB[i])
			{
				dup = 1;
				break;
			}
		}
		if (dup) break;

		++s.count;
		++iter;
	}

	c2v a, b;
	c2Witness(&s, &a, &b);
	float dist = c2Len(c2Sub(a, b));

	if (hit)
	{
		a = b;
		dist = 0;
	}

	else if (use_radius)
	{
		float rA = pA.radius;
		float rB = pB.radius;

		if (dist > rA + rB && dist > FLT_EPSILON)
		{
			dist -= rA + rB;
			c2v n = c2Norm(c2Sub(b, a));
			a = c2Add(a, c2Mulvs(n, rA));
			b = c2Sub(b, c2Mulvs(n, rB));
			if (a.x == b.x && a.y == b.y) dist = 0;
		}

		else
		{
			c2v p = c2Mulvs(c2Add(a, b), 0.5f);
			a = p;
			b = p;
			dist = 0;
		}
	}

	if (cache)
	{
		cache->metric = c2GJKSimplexMetric(&s);
		cache->count = s.count;
		for (int i = 0; i < s.count; ++i)
		{
			c2sv* v = verts + i;
			cache->iA[i] = v->iA;
			cache->iB[i] = v->iB;
		}
		cache->div = s.div;
	}

	if (outA) *outA = a;
	if (outB) *outB = b;
	if (iterations) *iterations = iter;
	return dist;
}

static C2_INLINE float c2Step(float t, const void* A, C2_TYPE typeA, const c2x* ax_ptr, c2v vA, c2v* a, const void* B, C2_TYPE typeB, const c2x* bx_ptr, c2v vB, c2v* b, int use_radius, c2GJKCache* cache)
{
	c2x ax = *ax_ptr;
	c2x bx = *bx_ptr;
	ax.p = c2Add(ax.p, c2Mulvs(vA, t));
	bx.p = c2Add(bx.p, c2Mulvs(vB, t));
	float d = c2GJK(A, typeA, &ax, B, typeB, &bx, a, b, use_radius, NULL, cache);
	return d;
}

float c2TOI(const void* A, C2_TYPE typeA, const c2x* ax_ptr, c2v vA, const void* B, C2_TYPE typeB, const c2x* bx_ptr, c2v vB, int use_radius, int* iterations)
{
	float t = 0;
	c2x ax;
	c2x bx;
	if (!ax_ptr) ax = c2xIdentity();
	else ax = *ax_ptr;
	if (!bx_ptr) bx = c2xIdentity();
	else bx = *bx_ptr;
	c2v a, b;
	c2GJKCache cache;
	cache.count = 0;
	float d = c2Step(t, A, typeA, &ax, vA, &a, B, typeB, &bx, vB, &b, use_radius, &cache);
	c2v v = c2Sub(vB, vA);

	int iters = 0;
	float eps = 1.0e-6f;
	while (d > eps && t < 1)
	{
		++iters;
		float velocity_bound = c2Abs(c2Dot(c2Norm(c2Sub(b, a)), v));
		if (!velocity_bound) return 1;
		float delta = d / velocity_bound;
		float t0 = t;
		float t1 = t + delta;
		if (t0 == t1) break;
		t = t1;
		d = c2Step(t, A, typeA, &ax, vA, &a, B, typeB, &bx, vB, &b, use_radius, &cache);
	}

	t = t >= 1 ? 1 : t;
	if (iterations) *iterations = iters;

	return t;
}

int c2Hull(c2v* verts, int count)
{
	if (count <= 2) return 0;
	count = c2Min(C2_MAX_POLYGON_VERTS, count);

	int right = 0;
	float xmax = verts[0].x;
	for (int i = 1; i < count; ++i)
	{
		float x = verts[i].x;
		if (x > xmax)
		{
			xmax = x;
			right = i;
		}

		else if (x == xmax)
		if (verts[i].y < verts[right].y) right = i;
	}

	int hull[C2_MAX_POLYGON_VERTS];
	int out_count = 0;
	int index = right;

	while (1)
	{
		hull[out_count] = index;
		int next = 0;

		for (int i = 1; i < count; ++i)
		{
			if (next == index)
			{
				next = i;
				continue;
			}

			c2v e1 = c2Sub(verts[next], verts[hull[out_count]]);
			c2v e2 = c2Sub(verts[i], verts[hull[out_count]]);
			float c = c2Det2(e1, e2);
			if(c < 0) next = i;
			if (c == 0 && c2Dot(e2, e2) > c2Dot(e1, e1)) next = i;
		}

		++out_count;
		index = next;
		if (next == right) break;
	}

	c2v hull_verts[C2_MAX_POLYGON_VERTS];
	for (int i = 0; i < out_count; ++i) hull_verts[i] = verts[hull[i]];
	memcpy(verts, hull_verts, sizeof(c2v) * out_count);
	return out_count;
}

void c2Norms(c2v* verts, c2v* norms, int count)
{
	for (int  i = 0; i < count; ++i)
	{
		int a = i;
		int b = i + 1 < count ? i + 1 : 0;
		c2v e = c2Sub(verts[b], verts[a]);
		norms[i] = c2Norm(c2CCW90(e));
	}
}

void c2MakePoly(c2Poly* p)
{
	p->count = c2Hull(p->verts, p->count);
	c2Norms(p->verts, p->norms, p->count);
}

c2Poly c2Dual(c2Poly poly, float skin_factor)
{
	c2Poly dual;
	dual.count = poly.count;

	// Each plane maps to a point by involution (the mapping is its own inverse) by dividing
	// the plane normal by its offset factor.
	// plane = a * x + b * y - d
	// dual = { a / d, b / d }
	for (int i = 0; i < poly.count; ++i) {
		c2v n = poly.norms[i];
		float d = c2Dot(n, poly.verts[i]) - skin_factor;
		if (d == 0) dual.verts[i] = c2V(0, 0);
		else dual.verts[i] = c2Div(n, d);
	}

	// Instead of canonically building the convex hull, can simply take advantage of how
	// the vertices are still in proper CCW order, so only the normals must be recomputed.
	c2Norms(dual.verts, dual.norms, dual.count);

	return dual;
}

// Inflating a polytope, idea by Dirk Gregorius ~ 2015. Works in both 2D and 3D.
// Reference: Halfspace intersection with Qhull by Brad Barber
//            http://www.geom.uiuc.edu/graphics/pix/Special_Topics/Computational_Geometry/half.html
//
// Algorithm steps:
// 1. Find a point within the input poly.
// 2. Center this point onto the origin.
// 3. Adjust the planes by a skin factor.
// 4. Compute the dual vert of each plane. Each plane becomes a vertex.
//    c2v dual(c2h plane) { return c2V(plane.n.x / plane.d, plane.n.y / plane.d) }
// 5. Compute the convex hull of the dual verts. This is called the dual.
// 6. Compute the dual of the dual, this will be the poly to return.
// 7. Translate the poly away from the origin by the center point from step 2.
// 8. Return the inflated poly.
c2Poly c2InflatePoly(c2Poly poly, float skin_factor)
{
	c2v average = poly.verts[0];
	for (int i = 1; i < poly.count; ++i) {
		average = c2Add(average, poly.verts[i]);
	}
	average = c2Div(average, (float)poly.count);

	for (int i = 0; i < poly.count; ++i) {
		poly.verts[i] = c2Sub(poly.verts[i], average);
	}

	c2Poly dual = c2Dual(poly, skin_factor);
	poly = c2Dual(dual, 0);

	for (int i = 0; i < poly.count; ++i) {
		poly.verts[i] = c2Add(poly.verts[i], average);
	}

	return poly;
}

void c2Inflate(void* shape, C2_TYPE type, float skin_factor)
{
	switch (type)
	{
	case C2_TYPE_CIRCLE:
	{
		c2Circle* circle = (c2Circle*)shape;
		circle->r += skin_factor;
	}	break;

	case C2_TYPE_AABB:
	{
		c2AABB* bb = (c2AABB*)shape;
		c2v factor = c2V(skin_factor, skin_factor);
		bb->min = c2Sub(bb->min, factor);
		bb->max = c2Add(bb->max, factor);
	}	break;

	case C2_TYPE_CAPSULE:
	{
		c2Capsule* capsule = (c2Capsule*)shape;
		capsule->r += skin_factor;
	}	break;

	case C2_TYPE_POLY:
	{
		c2Poly* poly = (c2Poly*)shape;
		*poly = c2InflatePoly(*poly, skin_factor);
	}	break;
	}
}

int c2CircletoCircle(c2Circle A, c2Circle B)
{
	c2v c = c2Sub(B.p, A.p);
	float d2 = c2Dot(c, c);
	float r2 = A.r + B.r;
	r2 = r2 * r2;
	return d2 < r2;
}

int c2CircletoAABB(c2Circle A, c2AABB B)
{
	c2v L = c2Clampv(A.p, B.min, B.max);
	c2v ab = c2Sub(A.p, L);
	float d2 = c2Dot(ab, ab);
	float r2 = A.r * A.r;
	return d2 < r2;
}

int c2AABBtoAABB(c2AABB A, c2AABB B)
{
	int d0 = B.max.x < A.min.x;
	int d1 = A.max.x < B.min.x;
	int d2 = B.max.y < A.min.y;
	int d3 = A.max.y < B.min.y;
	return !(d0 | d1 | d2 | d3);
}

int c2AABBtoPoint(c2AABB A, c2v B)
{
	int d0 = B.x < A.min.x;
	int d1 = B.y < A.min.y;
	int d2 = B.x > A.max.x;
	int d3 = B.y > A.max.y;
	return !(d0 | d1 | d2 | d3);
}

int c2CircleToPoint(c2Circle A, c2v B)
{
	c2v n = c2Sub(A.p, B);
	float d2 = c2Dot(n, n);
	return d2 < A.r * A.r;
}

// see: http://www.randygaul.net/2014/07/23/distance-point-to-line-segment/
int c2CircletoCapsule(c2Circle A, c2Capsule B)
{
	c2v n = c2Sub(B.b, B.a);
	c2v ap = c2Sub(A.p, B.a);
	float da = c2Dot(ap, n);
	float d2;

	if (da < 0) d2 = c2Dot(ap, ap);
	else
	{
		float db = c2Dot(c2Sub(A.p, B.b), n);
		if (db < 0)
		{
			c2v e = c2Sub(ap, c2Mulvs(n, (da / c2Dot(n, n))));
			d2 = c2Dot(e, e);
		}
		else
		{
			c2v bp = c2Sub(A.p, B.b);
			d2 = c2Dot(bp, bp);
		}
	}

	float r = A.r + B.r;
	return d2 < r * r;
}

int c2AABBtoCapsule(c2AABB A, c2Capsule B)
{
	if (c2GJK(&A, C2_TYPE_AABB, 0, &B, C2_TYPE_CAPSULE, 0, 0, 0, 1, 0, 0)) return 0;
	return 1;
}

int c2CapsuletoCapsule(c2Capsule A, c2Capsule B)
{
	if (c2GJK(&A, C2_TYPE_CAPSULE, 0, &B, C2_TYPE_CAPSULE, 0, 0, 0, 1, 0, 0)) return 0;
	return 1;
}

int c2CircletoPoly(c2Circle A, const c2Poly* B, const c2x* bx)
{
	if (c2GJK(&A, C2_TYPE_CIRCLE, 0, B, C2_TYPE_POLY, bx, 0, 0, 1, 0, 0)) return 0;
	return 1;
}

int c2AABBtoPoly(c2AABB A, const c2Poly* B, const c2x* bx)
{
	if (c2GJK(&A, C2_TYPE_AABB, 0, B, C2_TYPE_POLY, bx, 0, 0, 1, 0, 0)) return 0;
	return 1;
}

int c2CapsuletoPoly(c2Capsule A, const c2Poly* B, const c2x* bx)
{
	if (c2GJK(&A, C2_TYPE_CAPSULE, 0, B, C2_TYPE_POLY, bx, 0, 0, 1, 0, 0)) return 0;
	return 1;
}

int c2PolytoPoly(const c2Poly* A, const c2x* ax, const c2Poly* B, const c2x* bx)
{
	if (c2GJK(A, C2_TYPE_POLY, ax, B, C2_TYPE_POLY, bx, 0, 0, 1, 0, 0)) return 0;
	return 1;
}

int c2RaytoCircle(c2Ray A, c2Circle B, c2Raycast* out)
{
	c2v p = B.p;
	c2v m = c2Sub(A.p, p);
	float c = c2Dot(m, m) - B.r * B.r;
	float b = c2Dot(m, A.d);
	float disc = b * b - c;
	if (disc < 0) return 0;

	float t = -b - c2Sqrt(disc);
	if (t >= 0 && t <= A.t)
	{
		out->t = t;
		c2v impact = c2Impact(A, t);
		out->n = c2Norm(c2Sub(impact, p));
		return 1;
	}
	return 0;
}

static inline float c2SignedDistPointToPlane_OneDimensional(float p, float n, float d)
{
	return p * n - d * n;
}

static inline float c2RayToPlane_OneDimensional(float da, float db)
{
	if (da < 0) return 0; // Ray started behind plane.
	else if (da * db >= 0) return 1.0f; // Ray starts and ends on the same of the plane.
	else // Ray starts and ends on opposite sides of the plane (or directly on the plane).
	{
		float d = da - db;
		if (d != 0) return da / d;
		else return 0; // Special case for super tiny ray, or AABB.
	}
}

int c2RaytoAABB(c2Ray A, c2AABB B, c2Raycast* out)
{
	c2v p0 = A.p;
	c2v p1 = c2Impact(A, A.t);
	c2AABB a_box;
	a_box.min = c2Minv(p0, p1);
	a_box.max = c2Maxv(p0, p1);

	// Test B's axes.
	if (!c2AABBtoAABB(a_box, B)) return 0;

	// Test the ray's axes (along the segment's normal).
	c2v ab = c2Sub(p1, p0);
	c2v n = c2Skew(ab);
	c2v abs_n = c2Absv(n);
	c2v half_extents = c2Mulvs(c2Sub(B.max, B.min), 0.5f);
	c2v center_of_b_box = c2Mulvs(c2Add(B.min, B.max), 0.5f);
	float d = c2Abs(c2Dot(n, c2Sub(p0, center_of_b_box))) - c2Dot(abs_n, half_extents);
	if (d > 0) return 0;

	// Calculate intermediate values up-front.
	// This should play well with superscalar architecture.
	float da0 = c2SignedDistPointToPlane_OneDimensional(p0.x, -1.0f, B.min.x);
	float db0 = c2SignedDistPointToPlane_OneDimensional(p1.x, -1.0f, B.min.x);
	float da1 = c2SignedDistPointToPlane_OneDimensional(p0.x,  1.0f, B.max.x);
	float db1 = c2SignedDistPointToPlane_OneDimensional(p1.x,  1.0f, B.max.x);
	float da2 = c2SignedDistPointToPlane_OneDimensional(p0.y, -1.0f, B.min.y);
	float db2 = c2SignedDistPointToPlane_OneDimensional(p1.y, -1.0f, B.min.y);
	float da3 = c2SignedDistPointToPlane_OneDimensional(p0.y,  1.0f, B.max.y);
	float db3 = c2SignedDistPointToPlane_OneDimensional(p1.y,  1.0f, B.max.y);
	float t0 = c2RayToPlane_OneDimensional(da0, db0);
	float t1 = c2RayToPlane_OneDimensional(da1, db1);
	float t2 = c2RayToPlane_OneDimensional(da2, db2);
	float t3 = c2RayToPlane_OneDimensional(da3, db3);

	// Calculate hit predicate, no branching.
	int hit0 = t0 < 1.0f;
	int hit1 = t1 < 1.0f;
	int hit2 = t2 < 1.0f;
	int hit3 = t3 < 1.0f;
	int hit = hit0 | hit1 | hit2 | hit3;

	if (hit)
	{
		// Remap t's within 0-1 range, where >= 1 is treated as 0.
		t0 = (float)hit0 * t0;
		t1 = (float)hit1 * t1;
		t2 = (float)hit2 * t2;
		t3 = (float)hit3 * t3;

		// Sort output by finding largest t to deduce the normal.
		if (t0 >= t1 && t0 >= t2 && t0 >= t3)
		{
			out->t = t0 * A.t;
			out->n = c2V(-1, 0);
		}
		
		else if (t1 >= t0 && t1 >= t2 && t1 >= t3)
		{
			out->t = t1 * A.t;
			out->n = c2V(1, 0);
		}
		
		else if (t2 >= t0 && t2 >= t1 && t2 >= t3)
		{
			out->t = t2 * A.t;
			out->n = c2V(0, -1);
		}
		
		else
		{
			out->t = t3 * A.t;
			out->n = c2V(0, 1);
		}

		return 1;
	} else return 0; // This can still numerically happen.
}

int c2RaytoCapsule(c2Ray A, c2Capsule B, c2Raycast* out)
{
	c2m M;
	M.y = c2Norm(c2Sub(B.b, B.a));
	M.x = c2CCW90(M.y);

	// rotate capsule to origin, along Y axis
	// rotate the ray same way
	c2v cap_n = c2Sub(B.b, B.a);
	c2v yBb = c2MulmvT(M, cap_n);
	c2v yAp = c2MulmvT(M, c2Sub(A.p, B.a));
	c2v yAd = c2MulmvT(M, A.d);
	c2v yAe = c2Add(yAp, c2Mulvs(yAd, A.t));

	c2AABB capsule_bb;
	capsule_bb.min = c2V(-B.r, 0);
	capsule_bb.max = c2V(B.r, yBb.y);

	out->n = c2Norm(cap_n);
	out->t = 0;

	// check and see if ray starts within the capsule
	if (c2AABBtoPoint(capsule_bb, yAp)) {
		return 1;
	} else {
		c2Circle capsule_a;
		c2Circle capsule_b;
		capsule_a.p = B.a;
		capsule_a.r = B.r;
		capsule_b.p = B.b;
		capsule_b.r = B.r;

		if (c2CircleToPoint(capsule_a, A.p)) {
			return 1;
		} else if (c2CircleToPoint(capsule_b, A.p)) {
			return 1;
		}
	}

	if (yAe.x * yAp.x < 0 || c2Min(c2Abs(yAe.x), c2Abs(yAp.x)) < B.r)
	{
		c2Circle Ca, Cb;
		Ca.p = B.a;
		Ca.r = B.r;
		Cb.p = B.b;
		Cb.r = B.r;

		// ray starts inside capsule prism -- must hit one of the semi-circles
		if (c2Abs(yAp.x) < B.r) {
			if (yAp.y < 0) return c2RaytoCircle(A, Ca, out);
			else return c2RaytoCircle(A, Cb, out);
		}

		// hit the capsule prism
		else
		{
			float c = yAp.x > 0 ? B.r : -B.r;
			float d = (yAe.x - yAp.x);
			float t = (c - yAp.x) / d;
			float y = yAp.y + (yAe.y - yAp.y) * t;
			if (y <= 0) return c2RaytoCircle(A, Ca, out);
			if (y >= yBb.y) return c2RaytoCircle(A, Cb, out);
			else {
				out->n = c > 0 ? M.x : c2Skew(M.y);
				out->t = t * A.t;
				return 1;
			}
		}
	}

	return 0;
}

int c2RaytoPoly(c2Ray A, const c2Poly* B, const c2x* bx_ptr, c2Raycast* out)
{
	c2x bx = bx_ptr ? *bx_ptr : c2xIdentity();
	c2v p = c2MulxvT(bx, A.p);
	c2v d = c2MulrvT(bx.r, A.d);
	float lo = 0;
	float hi = A.t;
	int index = ~0;

	// test ray to each plane, tracking lo/hi times of intersection
	for (int i = 0; i < B->count; ++i)
	{
		float num = c2Dot(B->norms[i], c2Sub(B->verts[i], p));
		float den = c2Dot(B->norms[i], d);
		if (den == 0 && num < 0) return 0;
		else
		{
			if (den < 0 && num < lo * den)
			{
				lo = num / den;
				index = i;
			}
			else if (den > 0 && num < hi * den) hi = num / den;
		}
		if (hi < lo) return 0;
	}

	if (index != ~0)
	{
		out->t = lo;
		out->n = c2Mulrv(bx.r, B->norms[index]);
		return 1;
	}

	return 0;
}

void c2CircletoCircleManifold(c2Circle A, c2Circle B, c2Manifold* m)
{
	m->count = 0;
	c2v d = c2Sub(B.p, A.p);
	float d2 = c2Dot(d, d);
	float r = A.r + B.r;
	if (d2 < r * r)
	{
		float l = c2Sqrt(d2);
		c2v n = l != 0 ? c2Mulvs(d, 1.0f / l) : c2V(0, 1.0f);
		m->count = 1;
		m->depths[0] = r - l;
		m->contact_points[0] = c2Sub(B.p, c2Mulvs(n, B.r));
		m->n = n;
	}
}

void c2CircletoAABBManifold(c2Circle A, c2AABB B, c2Manifold* m)
{
	m->count = 0;
	c2v L = c2Clampv(A.p, B.min, B.max);
	c2v ab = c2Sub(L, A.p);
	float d2 = c2Dot(ab, ab);
	float r2 = A.r * A.r;
	if (d2 < r2)
	{
		// shallow (center of circle not inside of AABB)
		if (d2 != 0)
		{
			float d = c2Sqrt(d2);
			c2v n = c2Norm(ab);
			m->count = 1;
			m->depths[0] = A.r - d;
			m->contact_points[0] = c2Add(A.p, c2Mulvs(n, d));
			m->n = n;
		}

		// deep (center of circle inside of AABB)
		// clamp circle's center to edge of AABB, then form the manifold
		else
		{
			c2v mid = c2Mulvs(c2Add(B.min, B.max), 0.5f);
			c2v e = c2Mulvs(c2Sub(B.max, B.min), 0.5f);
			c2v d = c2Sub(A.p, mid);
			c2v abs_d = c2Absv(d);

			float x_overlap = e.x - abs_d.x;
			float y_overlap = e.y - abs_d.y;

			float depth;
			c2v n;

			if (x_overlap < y_overlap)
			{
				depth = x_overlap;
				n = c2V(1.0f, 0);
				n = c2Mulvs(n, d.x < 0 ? 1.0f : -1.0f);
			}

			else
			{
				depth = y_overlap;
				n = c2V(0, 1.0f);
				n = c2Mulvs(n, d.y < 0 ? 1.0f : -1.0f);
			}

			m->count = 1;
			m->depths[0] = A.r + depth;
			m->contact_points[0] = c2Sub(A.p, c2Mulvs(n, depth));
			m->n = n;
		}
	}
}

void c2CircletoCapsuleManifold(c2Circle A, c2Capsule B, c2Manifold* m)
{
	m->count = 0;
	c2v a, b;
	float r = A.r + B.r;
	float d = c2GJK(&A, C2_TYPE_CIRCLE, 0, &B, C2_TYPE_CAPSULE, 0, &a, &b, 0, 0, 0);
	if (d < r)
	{
		c2v n;
		if (d == 0) n = c2Norm(c2Skew(c2Sub(B.b, B.a)));
		else n = c2Norm(c2Sub(b, a));

		m->count = 1;
		m->depths[0] = r - d;
		m->contact_points[0] = c2Sub(b, c2Mulvs(n, B.r));
		m->n = n;
	}
}

void c2AABBtoAABBManifold(c2AABB A, c2AABB B, c2Manifold* m)
{
	m->count = 0;
	c2v mid_a = c2Mulvs(c2Add(A.min, A.max), 0.5f);
	c2v mid_b = c2Mulvs(c2Add(B.min, B.max), 0.5f);
	c2v eA = c2Absv(c2Mulvs(c2Sub(A.max, A.min), 0.5f));
	c2v eB = c2Absv(c2Mulvs(c2Sub(B.max, B.min), 0.5f));
	c2v d = c2Sub(mid_b, mid_a);

	// calc overlap on x and y axes
	float dx = eA.x + eB.x - c2Abs(d.x);
	if (dx < 0) return;
	float dy = eA.y + eB.y - c2Abs(d.y);
	if (dy < 0) return;

	c2v n;
	float depth;
	c2v p;

	// x axis overlap is smaller
	if (dx < dy)
	{
		depth = dx;
		if (d.x < 0)
		{
			n = c2V(-1.0f, 0);
			p = c2Sub(mid_a, c2V(eA.x, 0));
		}
		else
		{
			n = c2V(1.0f, 0);
			p = c2Add(mid_a, c2V(eA.x, 0));
		}
	}

	// y axis overlap is smaller
	else
	{
		depth = dy;
		if (d.y < 0)
		{
			n = c2V(0, -1.0f);
			p = c2Sub(mid_a, c2V(0, eA.y));
		}
		else
		{
			n = c2V(0, 1.0f);
			p = c2Add(mid_a, c2V(0, eA.y));
		}
	}

	m->count = 1;
	m->contact_points[0] = p;
	m->depths[0] = depth;
	m->n = n;
}

void c2AABBtoCapsuleManifold(c2AABB A, c2Capsule B, c2Manifold* m)
{
	m->count = 0;
	c2Poly p;
	c2BBVerts(p.verts, &A);
	p.count = 4;
	c2Norms(p.verts, p.norms, 4);
	c2CapsuletoPolyManifold(B, &p, 0, m);
	m->n = c2Neg(m->n);
}

void c2CapsuletoCapsuleManifold(c2Capsule A, c2Capsule B, c2Manifold* m)
{
	m->count = 0;
	c2v a, b;
	float r = A.r + B.r;
	float d = c2GJK(&A, C2_TYPE_CAPSULE, 0, &B, C2_TYPE_CAPSULE, 0, &a, &b, 0, 0, 0);
	if (d < r)
	{
		c2v n;
		if (d == 0) n = c2Norm(c2Skew(c2Sub(A.b, A.a)));
		else n = c2Norm(c2Sub(b, a));

		m->count = 1;
		m->depths[0] = r - d;
		m->contact_points[0] = c2Sub(b, c2Mulvs(n, B.r));
		m->n = n;
	}
}

static C2_INLINE c2h c2PlaneAt(const c2Poly* p, const int i)
{
	c2h h;
	h.n = p->norms[i];
	h.d = c2Dot(p->norms[i], p->verts[i]);
	return h;
}

void c2CircletoPolyManifold(c2Circle A, const c2Poly* B, const c2x* bx_tr, c2Manifold* m)
{
	m->count = 0;
	c2v a, b;
	float d = c2GJK(&A, C2_TYPE_CIRCLE, 0, B, C2_TYPE_POLY, bx_tr, &a, &b, 0, 0, 0);

	// shallow, the circle center did not hit the polygon
	// just use a and b from GJK to define the collision
	if (d != 0)
	{
		c2v n = c2Sub(b, a);
		float l = c2Dot(n, n);
		if (l < A.r * A.r)
		{
			l = c2Sqrt(l);
			m->count = 1;
			m->contact_points[0] = b;
			m->depths[0] = A.r - l;
			m->n = c2Mulvs(n, 1.0f / l);
		}
	}

	// Circle center is inside the polygon
	// find the face closest to circle center to form manifold
	else
	{
		c2x bx = bx_tr ? *bx_tr : c2xIdentity();
		float sep = -FLT_MAX;
		int index = ~0;
		c2v local = c2MulxvT(bx, A.p);

		for (int i = 0; i < B->count; ++i)
		{
			c2h h = c2PlaneAt(B, i);
			d = c2Dist(h, local);
			if (d > A.r) return;
			if (d > sep)
			{
				sep = d;
				index = i;
			}
		}

		c2h h = c2PlaneAt(B, index);
		c2v p = c2Project(h, local);
		m->count = 1;
		m->contact_points[0] = c2Mulxv(bx, p);
		m->depths[0] = A.r - sep;
		m->n = c2Neg(c2Mulrv(bx.r, B->norms[index]));
	}
}

// Forms a c2Poly and uses c2PolytoPolyManifold
void c2AABBtoPolyManifold(c2AABB A, const c2Poly* B, const c2x* bx, c2Manifold* m)
{
	m->count = 0;
	c2Poly p;
	c2BBVerts(p.verts, &A);
	p.count = 4;
	c2Norms(p.verts, p.norms, 4);
	c2PolytoPolyManifold(&p, 0, B, bx, m);
}

// clip a segment to a plane
static int c2Clip(c2v* seg, c2h h)
{
	c2v out[2];
	int sp = 0;
	float d0, d1;
	if ((d0 = c2Dist(h, seg[0])) < 0) out[sp++] = seg[0];
	if ((d1 = c2Dist(h, seg[1])) < 0) out[sp++] = seg[1];
	if (d0 == 0 && d1 == 0) {
		out[sp++] = seg[0];
		out[sp++] = seg[1];
	} else if (d0 * d1 <= 0) out[sp++] = c2Intersect(seg[0], seg[1], d0, d1);
	seg[0] = out[0]; seg[1] = out[1];
	return sp;
}

#ifdef _MSC_VER
	#pragma warning(push)
	#pragma warning(disable:4204) // nonstandard extension used: non-constant aggregate initializer
#endif

static int c2SidePlanes(c2v* seg, c2v ra, c2v rb, c2h* h)
{
	c2v in = c2Norm(c2Sub(rb, ra));
	c2h left = { c2Neg(in), c2Dot(c2Neg(in), ra) };
	c2h right = { in, c2Dot(in, rb) };
	if (c2Clip(seg, left) < 2) return 0;
	if (c2Clip(seg, right) < 2) return 0;
	if (h) {
		h->n = c2CCW90(in);
		h->d = c2Dot(c2CCW90(in), ra);
	}
	return 1;
}

// clip a segment to the "side planes" of another segment.
// side planes are planes orthogonal to a segment and attached to the
// endpoints of the segment
static int c2SidePlanesFromPoly(c2v* seg, c2x x, const c2Poly* p, int e, c2h* h)
{
	c2v ra = c2Mulxv(x, p->verts[e]);
	c2v rb = c2Mulxv(x, p->verts[e + 1 == p->count ? 0 : e + 1]);
	return c2SidePlanes(seg, ra, rb, h);
}

static void c2KeepDeep(c2v* seg, c2h h, c2Manifold* m)
{
	int cp = 0;
	for (int i = 0; i < 2; ++i)
	{
		c2v p = seg[i];
		float d = c2Dist(h, p);
		if (d <= 0)
		{
			m->contact_points[cp] = p;
			m->depths[cp] = -d;
			++cp;
		}
	}
	m->count = cp;
	m->n = h.n;
}

static C2_INLINE c2v c2CapsuleSupport(c2Capsule A, c2v dir)
{
	float da = c2Dot(A.a, dir);
	float db = c2Dot(A.b, dir);
	if (da > db) return c2Add(A.a, c2Mulvs(dir, A.r));
	else return c2Add(A.b, c2Mulvs(dir, A.r));
}

static void c2AntinormalFace(c2Capsule cap, const c2Poly* p, c2x x, int* face_out, c2v* n_out)
{
	float sep = -FLT_MAX;
	int index = ~0;
	c2v n = c2V(0, 0);
	for (int i = 0; i < p->count; ++i)
	{
		c2h h = c2Mulxh(x, c2PlaneAt(p, i));
		c2v n0 = c2Neg(h.n);
		c2v s = c2CapsuleSupport(cap, n0);
		float d = c2Dist(h, s);
		if (d > sep)
		{
			sep = d;
			index = i;
			n = n0;
		}
	}
	*face_out = index;
	*n_out = n;
}

static void c2Incident(c2v* incident, const c2Poly* ip, c2x ix, c2v rn_in_incident_space)
{
	int index = ~0;
	float min_dot = FLT_MAX;
	for (int i = 0; i < ip->count; ++i)
	{
		float dot = c2Dot(rn_in_incident_space, ip->norms[i]);
		if (dot < min_dot)
		{
			min_dot = dot;
			index = i;
		}
	}
	incident[0] = c2Mulxv(ix, ip->verts[index]);
	incident[1] = c2Mulxv(ix, ip->verts[index + 1 == ip->count ? 0 : index + 1]);
}

void c2CapsuletoPolyManifold(c2Capsule A, const c2Poly* B, const c2x* bx_ptr, c2Manifold* m)
{
	m->count = 0;
	c2v a, b;
	float d = c2GJK(&A, C2_TYPE_CAPSULE, 0, B, C2_TYPE_POLY, bx_ptr, &a, &b, 0, 0, 0);

	// deep, treat as segment to poly collision
	if (d < 1.0e-6f)
	{
		c2x bx = bx_ptr ? *bx_ptr : c2xIdentity();
		c2Capsule A_in_B;
		A_in_B.a = c2MulxvT(bx, A.a);
		A_in_B.b = c2MulxvT(bx, A.b);
		c2v ab = c2Norm(c2Sub(A_in_B.a, A_in_B.b));

		// test capsule axes
		c2h ab_h0;
		ab_h0.n = c2CCW90(ab);
		ab_h0.d = c2Dot(A_in_B.a, ab_h0.n);
		int v0 = c2Support(B->verts, B->count, c2Neg(ab_h0.n));
		float s0 = c2Dist(ab_h0, B->verts[v0]);

		c2h ab_h1;
		ab_h1.n = c2Skew(ab);
		ab_h1.d = c2Dot(A_in_B.a, ab_h1.n);
		int v1 = c2Support(B->verts, B->count, c2Neg(ab_h1.n));
		float s1 = c2Dist(ab_h1, B->verts[v1]);

		// test poly axes
		int index = ~0;
		float sep = -FLT_MAX;
		int code = 0;
		for (int i = 0; i < B->count; ++i)
		{
			c2h h = c2PlaneAt(B, i);
			float da = c2Dot(A_in_B.a, c2Neg(h.n));
			float db = c2Dot(A_in_B.b, c2Neg(h.n));
			float d;
			if (da > db) d = c2Dist(h, A_in_B.a);
			else d = c2Dist(h, A_in_B.b);
			if (d > sep)
			{
				sep = d;
				index = i;
			}
		}

		// track axis of minimum separation
		if (s0 > sep) {
			sep = s0;
			index = v0;
			code = 1;
		}

		if (s1 > sep) {
			sep = s1;
			index = v1;
			code = 2;
		}

		switch (code)
		{
		case 0: // poly face
		{
			c2v seg[2] = { A.a, A.b };
			c2h h;
			if (!c2SidePlanesFromPoly(seg, bx, B, index, &h)) return;
			c2KeepDeep(seg, h, m);
			m->n = c2Neg(m->n);
		}	break;

		case 1: // side 0 of capsule segment
		{
			c2v incident[2];
			c2Incident(incident, B, bx, ab_h0.n);
			c2h h;
			if (!c2SidePlanes(incident, A_in_B.b, A_in_B.a, &h)) return;
			c2KeepDeep(incident, h, m);
		}	break;

		case 2: // side 1 of capsule segment
		{
			c2v incident[2];
			c2Incident(incident, B, bx, ab_h1.n);
			c2h h;
			if (!c2SidePlanes(incident, A_in_B.a, A_in_B.b, &h)) return;
			c2KeepDeep(incident, h, m);
		}	break;

		default:
			// should never happen.
			return;
		}

		for (int i = 0; i < m->count; ++i) m->depths[i] += A.r;
	}

	// shallow, use GJK results a and b to define manifold
	else if (d < A.r)
	{
		m->count = 1;
		m->n = c2Norm(c2Sub(b, a));
		m->contact_points[0] = c2Add(a, c2Mulvs(m->n, A.r));
		m->depths[0] = A.r - d;
	}
}

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

static float c2CheckFaces(const c2Poly* A, c2x ax, const c2Poly* B, c2x bx, int* face_index)
{
	c2x b_in_a = c2MulxxT(ax, bx);
	c2x a_in_b = c2MulxxT(bx, ax);
	float sep = -FLT_MAX;
	int index = ~0;

	for (int i = 0; i < A->count; ++i)
	{
		c2h h = c2PlaneAt(A, i);
		int idx = c2Support(B->verts, B->count, c2Mulrv(a_in_b.r, c2Neg(h.n)));
		c2v p = c2Mulxv(b_in_a, B->verts[idx]);
		float d = c2Dist(h, p);
		if (d > sep)
		{
			sep = d;
			index = i;
		}
	}

	*face_index = index;
	return sep;
}

// Please see Dirk Gregorius's 2013 GDC lecture on the Separating Axis Theorem
// for a full-algorithm overview. The short description is:
	// Test A against faces of B, test B against faces of A
	// Define the reference and incident shapes (denoted by r and i respectively)
	// Define the reference face as the axis of minimum penetration
	// Find the incident face, which is most anti-normal face
	// Clip incident face to reference face side planes
	// Keep all points behind the reference face
void c2PolytoPolyManifold(const c2Poly* A, const c2x* ax_ptr, const c2Poly* B, const c2x* bx_ptr, c2Manifold* m)
{
	m->count = 0;
	c2x ax = ax_ptr ? *ax_ptr : c2xIdentity();
	c2x bx = bx_ptr ? *bx_ptr : c2xIdentity();
	int ea, eb;
	float sa, sb;
	if ((sa = c2CheckFaces(A, ax, B, bx, &ea)) >= 0) return;
	if ((sb = c2CheckFaces(B, bx, A, ax, &eb)) >= 0) return;

	const c2Poly* rp,* ip;
	c2x rx, ix;
	int re;
	float kRelTol = 0.95f, kAbsTol = 0.01f;
	int flip;
	if (sa * kRelTol > sb + kAbsTol)
	{
		rp = A; rx = ax;
		ip = B; ix = bx;
		re = ea;
		flip = 0;
	}
	else
	{
		rp = B; rx = bx;
		ip = A; ix = ax;
		re = eb;
		flip = 1;
	}

	c2v incident[2];
	c2Incident(incident, ip, ix, c2MulrvT(ix.r, c2Mulrv(rx.r, rp->norms[re])));
	c2h rh;
	if (!c2SidePlanesFromPoly(incident, rx, rp, re, &rh)) return;
	c2KeepDeep(incident, rh, m);
	if (flip) m->n = c2Neg(m->n);
}

#endif // CUTE_C2_IMPLEMENTATION_ONCE
#endif // CUTE_C2_IMPLEMENTATION

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
