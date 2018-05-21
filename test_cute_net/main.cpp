#define CUTE_NET_IMPLEMENTATION
#include "../cute_net.h"

#define CUTE_TIME_IMPLEMENTATION
#include "../cute_time.h"

#if CUTE_NET_PLATFORM != CUTE_NET_WINDOWS

	// meh. nothing convenient enough to actually implement this on non-Windows
	#define GetAsyncKeyState(...) 0

	#include <time.h>
	
	int Sleep(uint32_t milliseconds)
	{
		struct timespec spec;
		spec.tv_sec = 0;
		spec.tv_nsec = milliseconds * 1000000;
		return nanosleep(&spec, 0);
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
cn_context_t* ctx;
cn_address_t server_address;
cn_socket_t server_socket;
cn_address_t client_address;
cn_socket_t client_socket;
cn_transport_t server;
cn_transport_t client;

#include "tests.h"

void PeakCheck(cn_transport_t* transport)
{
	PacketA p;
	cn_address_t from;
	int packet_type;
	uint32_t words[ CUTE_NET_MTU_WORDCOUNT ];

	int bytes = cn_peak_internal(transport, &from, words);
	if (bytes)
	{
		int header_was_ok = cn_read_packet_header(transport, words, bytes, &packet_type, 0);
		CHECK(header_was_ok);

		if (header_was_ok)
		{
			int serialize_was_ok = cn_get_packet_data_internal(transport, words, &p, packet_type);
			CHECK(serialize_was_ok);
			CHECK(Check(packet, p));
		}
	}
}

int main(void)
{
	ctx = cn_init(PT_COUNT);

	server_address = cn_make_address("[::1]:1500");
	client_address = cn_make_address("[::1]:1501");
	server_socket = cn_make_socket(server_address, 1024 * 1024, 1);
	client_socket = cn_make_socket(client_address, 1024 * 1024, 1);

	cn_register(ctx, PT_PACKETA, WritePacketA, ReadPacketA, MeasureWritePacketA, sizeof(PacketA));
	cn_make_transport(&server, ctx, server_socket, client_address, "server");
	cn_make_transport(&client, ctx, client_socket, server_address, "client");

	cn_spawn_worker_thread(&server);
	//cn_spawn_worker_thread(&client);

	//DoTests();

#if 0
	tnNetSimDef sim;
	sim.latency = 400;
	sim.jitter = 0;
	sim.drop = 0;
	sim.corruption = 0;
	sim.duplicates = 0;
	sim.duplicates_min = 0;
	sim.duplicates_max = 0;
	sim.pool_size = 1024;
	tnAddNetSim(ctx, &sim);
#endif

	packet.a = 5;
	packet.b = 10;
	packet.c = 0.12f;
	packet.d = 102.0912932f;

	while (1)
	{
		if (GetAsyncKeyState(VK_ESCAPE))
			break;
		
		for (int i = 0; i < 1; ++i)
		{
			cn_reliable(&server, PT_PACKETA, &packet);
			cn_reliable(&client, PT_PACKETA, &packet);
		}
		cn_send(&server, 0, 0);
		cn_send(&client, 0, 0);

		char buffer[ CUTE_NET_PACKET_DATA_MAX_SIZE ];
		int type = ~0;
		cn_address_t from;
		while (cn_get_packet(&server, &from, &type, buffer))
			;
		while (cn_get_packet(&client, &from, &type, buffer))
			;

		PacketA p;
		while (cn_get_reliable(&server, &type, &p))
			CHECK(Check(packet, p));
		while (cn_get_reliable(&client, &type, &p))
			CHECK(Check(packet, p));

		// RTT is bugged??
		// golly gee wtf

		float dt = ct_time();
		cn_flush_sim(ctx);
		printf("dt: %f (milliseconds), rtt: %d, ping: %d\n", dt * 1000.0f, server.round_trip_time_millis, 0);

		cn_sleep(16);
	}

	cn_free_transport(&server);
	cn_free_transport(&client);
	cn_shutdown(ctx);

	return 0;
}
