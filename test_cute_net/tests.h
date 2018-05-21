int g_AnyFailed;

#define CHECK(X) do { if (!(X)) { printf("FAILED (line %d): %s\n", __LINE__, #X); g_AnyFailed = 1; } } while (0)

int Check(PacketA a, PacketA b)
{
	return a.a == b.a &&
		a.b == b.b &&
		a.c == b.c &&
		a.d == b.d;
}

void WritePacketA(cn_buffer_t* buffer, void* data)
{
	PacketA* p = (PacketA*)data;
	cn_write_i32(buffer, p->a, -5, 5);
	cn_write_i32(buffer, p->b, -10, 10);
	cn_write_f32(buffer, p->c);
	cn_write_f64(buffer, p->d);
	cn_flush(buffer);
}

int ReadPacketA(cn_buffer_t* buffer, void* data)
{
	PacketA* p = (PacketA*)data;
	cn_read_i32(buffer, &p->a, -5, 5);
	cn_read_i32(buffer, &p->b, -10, 10);
	cn_read_f32(buffer, &p->c);
	cn_read_f64(buffer, &p->d);
	return 1;
}

int MeasureWritePacketA()
{
	return cn_bits_required(-5, 5)
		+ cn_bits_required(-10, 10)
		+ 4
		+ 8;
}

void Sender_internal()
{
	packet.a = 5;
	packet.b = 10;
	packet.c = 0.12f;
	packet.d = 102.0912932f;

	uint32_t memory[32];
	cn_buffer_t buffer = cn_make_buffer(memory, 32);
	WritePacketA(&buffer, &packet);

	cn_send_internal(server_socket, server_address, buffer.words, cn_size(&buffer));
}

int Reciever_internal()
{
	uint32_t memory[32];
	cn_address_t address;
	int bytes_recieved = cn_recieve_internal(server_socket, &address, memory, 32 * 4);

	if (bytes_recieved)
	{
		cn_buffer_t buffer = cn_make_buffer(memory, 32);
		PacketA p;
		CHECK(ReadPacketA(&buffer, &p));
		CHECK(Check(packet, p));
	}

	return 1;
}

void TestSendAndRecieveInternal()
{
	for (int i = 0; i < 5; ++i)
	{
		Sender_internal();
		Sleep(1);
		Reciever_internal();
	}
}

void Sender()
{
	packet.a = 5;
	packet.b = 10;
	packet.c = 0.12f;
	packet.d = 102.0912932f;

	cn_send(&server, PT_PACKETA, &packet);
}

int Reciever()
{
	PacketA p;
	cn_address_t from;
	int packet_type;
	uint32_t words[CUTE_NET_MTU_WORDCOUNT];

	int bytes = cn_peak_internal(&server, &from, words);
	if (bytes)
	{
		int header_was_ok = cn_read_packet_header(&server, words, bytes, &packet_type, 0);

		if (header_was_ok)
		{
			int serialize_was_ok = cn_get_packet_data_internal(&server, words, &p, packet_type);
			CHECK(serialize_was_ok);
			CHECK(Check(packet, p));
			return 1;
		}
	}

	return 0;
}

void TestSendRecieveAck()
{
	for (int i = 0; i < 5; ++i)
	{
		Sender();
		Sleep(1);
		Reciever();
	}

	Sender();
	Sender();
	Sender();
	Sleep(1);
	Reciever();
	Reciever();
	Reciever();
}

void SoakBasicAcks()
{
	cn_sim_def_t sim;
	sim.latency = 250;
	sim.jitter = 50;
	sim.drop = 99;
	sim.corruption = 1;
	sim.duplicates = 5;
	sim.duplicates_min = 1;
	sim.duplicates_max = 3;
	sim.pool_size = 1024;
	cn_add_sim(ctx, &sim);

	float time = 0;

	while (1)
	{
		if (GetAsyncKeyState(VK_ESCAPE))
			break;

		time += ct_time();

		if (time < dt) continue;
		while (time > dt)
			time -= dt;

		Sender();
		while (Reciever());
		cn_flush_sim(ctx);
	}
}

inline void DoTests()
{
	CHECK(cn_bits_required(0, CUTE_NET_PACKET_TYPE_COUNT) == CUTE_NET_PACKET_TYPE_BITS_REQUIRED);
	CHECK(cn_bits_required(0, CUTE_NET_MAX_RELIABLES) == CUTE_NET_MAX_RELIABLES_BITS_REQUIRED);

	CHECK(cn_swap_internal(0x1234ABCD) == 0xCDAB3412);
	CHECK(cn_swap_internal((int32_t)0x1234ABCD) == 0xCDAB3412);

	CHECK(cn_pop_count(3) == 2);
	CHECK(cn_pop_count(~0) == 32);
	CHECK(cn_pop_count(0xA7AE0F6F) == (2 + 3 + 2 + 3 + 0 + 4 + 2 + 4));

	CHECK(cn_log2(32) == 5);
	CHECK(cn_log2(154) == 7);
	CHECK(cn_log2(0x7AC31E3) == 26);
	CHECK(cn_log2(0x8AC51E5) == 27);
	
	CHECK(cn_bits_required(0, 256) == 9);
	CHECK(cn_bits_required(0, 255) == 8);
	CHECK(cn_bits_required(0, 100) == 7);
	CHECK(cn_bits_required(0, 63) == 6);
	CHECK(cn_bits_required(0, 5) == 3);
	CHECK(cn_bits_required(0, ~0) == 32);
	CHECK(cn_bits_required(256, 256 + 256) == 9);
	CHECK(cn_bits_required(255, 255 + 255) == 8);
	CHECK(cn_bits_required(100, 100 + 100) == 7);
	CHECK(cn_bits_required(63, 63 + 63) == 6);
	CHECK(cn_bits_required(5, 5 + 5) == 3);

	{
		char poem[] = {
			0x74,0x68,0x65,0x20,0x73,0x70,0x69,0x64,0x65,0x72,0x0D,0x0A,0x63,0x72,0x61,0x77,
			0x6C,0x65,0x64,0x20,0x75,0x70,0x0D,0x0A,0x74,0x68,0x65,0x20,0x77,0x65,0x62,0x20,
			0x3A,0x29,0x00,0x00
		};
		const size_t word_count = (sizeof(poem) / sizeof(*poem)) / sizeof(uint32_t);
		uint32_t* words = (uint32_t*)poem;

		cn_buffer_t bits = cn_make_buffer(words, word_count);
		int i = 0;
		while (bits.bits_left)
		{
			char c = cn_read_bits_internal(&bits, 8);
			CHECK(c == poem[i++]);
		}
		CHECK(cn_would_overflow(&bits, 1));

		bits = cn_make_buffer(words, word_count);
		i = 0;
		while (bits.bits_left)
		{
			uint32_t w = cn_read_bits_internal(&bits, 8 * 4);
			CHECK(w == words[i++]);
		}

		uint32_t buffer[word_count];
		bits = cn_make_buffer(buffer, word_count);
		i = 0;
		while (bits.bits_left)
			cn_write_bits(&bits, words[i++], 4 * 8);
		for (i = 0; i < word_count; ++i)
			CHECK(buffer[i] == words[i]);

		uint32_t word = 0;
		bits = cn_make_buffer(&word, 1);
		cn_write_bits(&bits, 0x000ABCDE, 20);
		CHECK(word == 0);
		cn_flush(&bits);
		CHECK(word == 0x000ABCDE);
		CHECK(!cn_would_overflow(&bits, 12));
		CHECK(cn_would_overflow(&bits, 13));

		bits = cn_make_buffer(&word, 1);
		cn_write_bits(&bits, ~0, 16);
		CHECK(cn_would_overflow(&bits, 17));

		bits = cn_make_buffer(&word, 1);
		cn_read_bits_internal(&bits, 16);
		CHECK(cn_would_overflow(&bits, 17));
	}

	CHECK(cn_crc32("123456789", strlen("123456789"), 0) == 0xCBF43926);

	cn_address_t addr = cn_make_address("127.0.0.1");
	CHECK(addr.ipv4 == htonl(0x7F000001));
	addr = cn_make_address("::1");
	for (int i = 0; i < 7; ++i) CHECK(!addr.ipv6[i]);
	CHECK(addr.ipv6[7] == htons(0x0001));

	addr = cn_make_address("127.0.0.1:1337");
	CHECK(addr.ipv4 == htonl(0x7F000001));
	CHECK(addr.port == 1337);
	addr = cn_make_address("[::1]:2");
	for (int i = 0; i < 7; ++i) CHECK(!addr.ipv6[i]);
	CHECK(addr.ipv6[7] == htons(0x0001));
	CHECK(addr.port == 2);

	if (!g_AnyFailed)
		printf("All test cases passed!\n");

	TestSendAndRecieveInternal();
	TestSendRecieveAck();

	cn_sequence_buffer_t buffer;
	cn_sequence_buffer_t* seq_buf = &buffer;
	struct tnSequenceData { int acked; };
	cn_make_sequence_buffer(seq_buf, sizeof(tnSequenceData));
	CHECK(!cn_get_sequence_data(seq_buf, 0));
	tnSequenceData* data = (tnSequenceData*)cn_insert_sequence(seq_buf, 0);
	data->acked = 0;
	data = (tnSequenceData*)cn_get_sequence_data(seq_buf, 0);
	CHECK(data);
	CHECK(!data->acked);
	cn_free_sequence_buffer(seq_buf);
	cn_make_sequence_buffer(seq_buf, sizeof(tnSequenceData));
	for (int i = 0; i < CUTE_NET_UINT16_MAX; ++i)
	{
		tnSequenceData* data = (tnSequenceData*)cn_insert_sequence(seq_buf, i);
		CHECK(data);
	}
	for (int i = 0; i < CUTE_NET_UINT16_MAX - CUTE_NET_SEQUENCE_BUFFER_SIZE; ++i)
	{
		tnSequenceData* data = (tnSequenceData*)cn_get_sequence_data(seq_buf, i);
		CHECK(!data);
	}
	for (int i = CUTE_NET_UINT16_MAX - CUTE_NET_SEQUENCE_BUFFER_SIZE; i < CUTE_NET_UINT16_MAX; ++i)
	{
		tnSequenceData* data = (tnSequenceData*)cn_insert_sequence(seq_buf, i);
		CHECK(data);
	}
	cn_free_sequence_buffer(seq_buf);
	cn_make_sequence_buffer(seq_buf, sizeof(tnSequenceData));
	for (int i = 0, j = 0; i < 32; ++i, j = !j)
	{
		if (j)
		{
			tnSequenceData* data = (tnSequenceData*)cn_insert_sequence(seq_buf, i);
			data->acked = 1;
		}
	}
	uint16_t ack;
	uint32_t ack_bits;
	seq_buf->sequence = 33;
	cn_make_ack(seq_buf, &ack, &ack_bits);
	for (int i = 0, j = 0; i < 32; ++i, j = !j)
	{
		if (j) CHECK(ack_bits & (1 << i));
		else CHECK(!(ack_bits & (1 << i)));
	}
	CHECK(!cn_get_sequence_data(seq_buf, 33));
	CHECK(!((tnSequenceData*)cn_insert_sequence(seq_buf, 33))->acked);
	cn_free_sequence_buffer(seq_buf);
}
