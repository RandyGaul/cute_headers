#include "cute_math.h"

#include <float.h>
#include <stdio.h>
#define CHECK( X ) do { if ( !(X) ) { printf( "FAILED (line %d): %s\n", __LINE__, #X ); } } while ( 0 )

// see http://www.codersnotes.com/notes/maths-lib-2016/
bool RayBox( v3 rayOrg, v3 invDir, v3 bbmin, v3 bbmax, vfloat &hitT )
{
	v3 d0 = (bbmin - rayOrg) * invDir;
	v3 d1 = (bbmax - rayOrg) * invDir;

	v3 v0 = min(d0, d1);
	v3 v1 = max(d0, d1);

	vfloat tmin = hmax(v0);
	vfloat tmax = hmin(v1);

	bool hit = (tmax >= 0) && (tmax >= tmin) && (tmin <= hitT);
	if (hit)
		hitT = tmin;
	return hit;
}

int TM_CDECL main( )
{
	v3 a = v3( 1.0f, 2.0f, 3.0f );
	vfloat b = dot( a, a );
	CHECK( all( b == vfloat( 14 ) ) );

	v3 c = v3( b.to_float() );
	c = setx( c, 1 );
	c = sety( c, 2 );
	c = setz( c, 3 );
	CHECK( all( a == c ) );

	vfloat x = getx( c );
	vfloat y = gety( c );
	vfloat z = getz( c );
	CHECK( x == 1.0f );
	CHECK( y == 2.0f );
	CHECK( z == 3.0f );

	a = splatx( c );
	CHECK( all( a == v3( 1 ) ) );
	a = splaty( c );
	CHECK( all( a == v3( 2 ) ) );
	a = splatz( c );
	CHECK( all( a == v3( 3 ) ) );

	a = v3( 5, 7, -11 );
	v3 d = cross( a, c );
	CHECK( all( d == v3( 43, -26, 3 ) ) );

	m3 m = rows( v3( 1, 2, 3 ), v3( 4, 5, 6 ), v3( 7, 8, 9 ) );
	m = transpose( m );
	CHECK( all( m.x == v3( 1, 4, 7 ) ) );
	CHECK( all( m.y == v3( 2, 5, 8 ) ) );
	CHECK( all( m.z == v3( 3, 6, 9 ) ) );

	v3 testorg = v3( -10, 0, 0 );
	v3 testdir = v3( 1, 0, 0 );
	v3 testbbmin = v3( -4, -4, -4 );
	v3 testbbmax = v3( +4, +4, +4 );
	vfloat hitT = vfloat( FLT_MAX );
	bool hit = RayBox( testorg, v3( 1, 1, 1 ) / testdir, testbbmin, testbbmax, hitT );
	printf( "hit box? %s, at t = %f\n", hit ? "yes" : "no", hitT );

	halfspace plane;
	plane.n = v3( 1, 0, 0 );
	plane.d = vfloat( 5 );
	v3 a0 = v3( 0, 2, 2 );
	v3 b0 = v3( 10, -2, -2 );
	v3 c0 = intersect( a0, b0, distance( plane, a0 ), distance( plane, b0 ) );
	printf( "segment hit plane at { %f, %f, %f }\n", getx( c0 ), gety( c0 ), getz( c0 ) );

	return 0;
}
