#include <stdio.h>

typedef sid unsigned;

int main( )
{
	sid id = SID( "hello" );

	switch ( id )
	{
		case SID( "nope" ): printf( "nope" ); break;
		case SID( "hello" ): printf( "yep" ); break;
		case SID( "peekaboo" ): printf( "nope" ); break;
	}

	return 0;
}
