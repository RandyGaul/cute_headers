#define TN_IMPLEMENTATION
#include "tinynet.h"

// These functions are intended be called from a single thread only. In a
// multi-threaded environment make sure to call Time from the main thread only.
// Also try to set a thread affinity for the main thread to avoid core swaps.
// This can help prevent annoying bios bugs etc. from giving weird time-warp
// effects as the main thread gets passed from one core to another. This shouldn't
// be a problem, but it's a good practice to setup the affinity. Calling these
// functions on multiple threads multiple times will grant a heft performance
// loss in the form of false sharing due to CPU cache synchronization across
// multiple cores.

#if TN_PLATFORM == TN_WINDOWS

	float Time( )
	{
		static int first = 1;
		static LARGE_INTEGER prev;
		static double factor;

		LARGE_INTEGER now;
		QueryPerformanceCounter( &now );

		if ( first )
		{
			first = 0;
			prev = now;
			LARGE_INTEGER freq;
			QueryPerformanceFrequency( &freq );
			factor = 1.0 / (double)freq.QuadPart;
		}

		float elapsed = (float)((double)(now.QuadPart - prev.QuadPart) * factor);
		prev = now;
		return elapsed;
	}

#elif TN_PLATFORM == TN_MAC

	#include <mach/mach_time.h>

	float Time( )
	{
		static int first = 1;
		static uint64_t prev = 0;
		static double factor;
	
		if ( first )
		{
			first = 0;
			mach_timebase_info_data_t info;
			mach_timebase_info( &info );
			factor = ((double)info.numer / (double)info.denom) * 1000000000.0;
			prev = mach_absolute_time( );
			return 0;
		}
	
		uint64_t now = mach_absolute_time( );
		float elapsed = (float)((double)(now - prev) * factor);
		prev = now;
		return elapsed;
	}

#else

	#include <time.h>
	
	float Time( )
	{
		static int first = 1;
		struct timespec prev;
		
		if ( first )
		{
			first = 0;
			clock_gettime( CLOCK_MONOTONIC, &prev );
			return 0;
		}
		
		struct timespec now;
		clock_gettime( CLOCK_MONOTONIC, &now );
		float elapsed = (float)((double)(now.tv_nsec - prev.tv_nsec) * 1000000000.0);
		prev = now;
		return elapsed;
	}

#endif

#if TN_PLATFORM != TN_WINDOWS

	// meh. nothing convenient enough to actually implement this on non-Windows
	#define GetAsyncKeyState( ... ) 0

	#include <time.h>
	
	int Sleep( uint32_t milliseconds )
	{
		struct timespec spec;
		spec.tv_sec = 0;
		spec.tv_nsec = milliseconds * 1000000;
		return nanosleep( &spec, 0 );
	}

#endif

struct PacketA
{
	int a;
	int b;
	float c;
	double d;
};

enum PacketTypes
{
	PT_PACKET_NONE,
	PT_PACKETA,

	PT_COUNT,
};

PacketA packet;

float dt = 1.0f / 60.0f;
tnContext* ctx;
tnAddress server_address;
tnSocket server_socket;
tnAddress client_address;
tnSocket client_socket;
tnTransport server;
tnTransport client;

#include "tests.h"

void PeakCheck( tnTransport* transport )
{
	PacketA p;
	tnAddress from;
	int packet_type;
	int packet_size_bytes;

	tnPeak_internal( transport, &from, &packet_type, &packet_size_bytes );

	if ( transport->has_packet )
	{
		int serialize_was_ok = tnGetPacketData_internal( transport, &p, packet_type );
		CHECK( serialize_was_ok );
		CHECK( Check( packet, p ) );
	}
}

int main( void )
{
	ctx = tnInit( PT_COUNT, dt );

	server_address = tnMakeAddress( "[::1]:1500" );
	client_address = tnMakeAddress( "[::1]:1501" );
	server_socket = tnMakeSocket( server_address, 1024 * 1024 );
	client_socket = tnMakeSocket( client_address, 1024 * 1024 );

	tnRegister( ctx, PT_PACKETA, WritePacketA, ReadPacketA, MeasureWritePacketA, sizeof( PacketA ) );
	tnMakeTransport( &server, ctx, server_socket, client_address, "server" );
	tnMakeTransport( &client, ctx, client_socket, server_address, "client" );

	//DoTests( );

#if 1
	tnNetSimDef sim;
	sim.latency = 250;
	sim.jitter = 50;
	sim.drop = 99;
	sim.corruption = 0;
	sim.duplicates = 0;
	sim.duplicates_min = 0;
	sim.duplicates_max = 0;
	sim.pool_size = 1024;
	tnAddNetSim( ctx, &sim );
#endif

	packet.a = 5;
	packet.b = 10;
	packet.c = 0.12f;
	packet.d = 102.0912932f;

	float time = 0;

	while ( 1 )
	{
		if ( GetAsyncKeyState( VK_ESCAPE ) )
			break;

		time += Time( );

		//if ( time < dt ) continue;
		while ( time > dt )
			time -= dt;

		for ( int i = 0; i < TN_SEQUENCE_BUFFER_SIZE * 10; ++i )
		{
			tnReliable( &server, PT_PACKETA, &packet );
			tnReliable( &client, PT_PACKETA, &packet );
		}
		tnSend( &server, 0, 0 );
		tnSend( &client, 0, 0 );

		tnTick( ctx );

		char buffer[ TN_PACKET_DATA_MAX_SIZE ];
		int type;
		tnAddress from;
		tnGetPacket( &server, &from, &type, buffer );
		tnGetPacket( &client, &from, &type, buffer );

		PacketA p;
		while ( tnGetReliable( &server, &type, &p ) )
			CHECK( Check( packet, p ) );
		while ( tnGetReliable( &client, &type, &p ) )
			CHECK( Check( packet, p ) );
	}

	tnFreeTransport( &server );
	tnFreeTransport( &client );
	tnShutdown( ctx );

	return 0;
}
