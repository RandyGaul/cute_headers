#include "tinymath.h"

#include <float.h>
#include <stdio.h>
#define CHECK( X ) do { if ( !(X) ) { printf( "FAILED (line %d): %s\n", __LINE__, #X ); } } while ( 0 )

// see http://www.codersnotes.com/notes/maths-lib-2016/
bool RayBox( v3 rayOrg, v3 invDir, v3 bbmin, v3 bbmax, float &hitT )
{
	v3 d0 = (bbmin - rayOrg) * invDir;
	v3 d1 = (bbmax - rayOrg) * invDir;

	v3 v0 = min(d0, d1);
	v3 v1 = max(d0, d1);

	float tmin = hmax(v0);
	float tmax = hmin(v1);

	bool hit = (tmax >= 0) && (tmax >= tmin) && (tmin <= hitT);
	if (hit)
		hitT = tmin;
	return hit;
}

int TM_CDECL main( )
{
	v3 a = v3( 1.0f, 2.0f, 3.0f );
	v3 b = dot( a, a );
	CHECK( all( b == v3( 14 ) ) );

	b = setx( b, 1 );
	b = sety( b, 2 );
	b = setz( b, 3 );
	CHECK( all( a == b ) );

	float x = getx( b );
	float y = gety( b );
	float z = getz( b );
	CHECK( x == 1.0f );
	CHECK( y == 2.0f );
	CHECK( z == 3.0f );

	a = splatx( b );
	CHECK( all( a == v3( 1 ) ) );
	a = splaty( b );
	CHECK( all( a == v3( 2 ) ) );
	a = splatz( b );
	CHECK( all( a == v3( 3 ) ) );

	a = v3( 5, 7, -11 );
	v3 c = cross( a, b );
	CHECK( all( c == v3( 43, -26, 3 ) ) );

	m3 m = rows( v3( 1, 2, 3 ), v3( 4, 5, 6 ), v3( 7, 8, 9 ) );
	m = transpose( m );
	CHECK( all( m.x == v3( 1, 4, 7 ) ) );
	CHECK( all( m.y == v3( 2, 5, 8 ) ) );
	CHECK( all( m.z == v3( 3, 6, 9 ) ) );

	v3 testorg = v3( -10, 0, 0 );
	v3 testdir = v3( 1, 0, 0 );
	v3 testbbmin = v3( -4, -4, -4 );
	v3 testbbmax = v3( +4, +4, +4 );
	float hitT = FLT_MAX;
	bool hit = RayBox( testorg, v3( 1, 1, 1 ) / testdir, testbbmin, testbbmax, hitT );
	printf( "hit box? %s, at t = %f\n", hit ? "yes" : "no", hitT );

	halfspace plane;
	plane.n = v3( 1, 0, 0 );
	plane.d = v3( 5 );
	a = v3( 0, 2, 2 );
	b = v3( 10, -2, -2 );
	c = intersect( a, b, distance( plane, a ), distance( plane, b ) );
	printf( "segment hit plane at { %f, %f, %f }\n", getx( c ), gety( c ), getz( c ) );

	return 0;
}
