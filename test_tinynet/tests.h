int g_AnyFailed;

#define CHECK( X ) do { if ( !(X) ) { printf( "FAILED (line %d): %s\n", __LINE__, #X ); g_AnyFailed = 1; } } while ( 0 )

int Check( PacketA a, PacketA b )
{
	return a.a == b.a &&
		a.b == b.b &&
		a.c == b.c &&
		a.d == b.d;
}

void WritePacketA( tnBuffer* buffer, void* data )
{
	PacketA* p = (PacketA*)data;
	tnWriteI32( buffer, p->a, -5, 5 );
	tnWriteI32( buffer, p->b, -10, 10 );
	tnWriteF32( buffer, p->c );
	tnWriteF64( buffer, p->d );
	tnFlush( buffer );
}

int ReadPacketA( tnBuffer* buffer, void* data )
{
	PacketA* p = (PacketA*)data;
	tnReadI32( buffer, &p->a, -5, 5 );
	tnReadI32( buffer, &p->b, -10, 10 );
	tnReadF32( buffer, &p->c );
	tnReadF64( buffer, &p->d );
	return 1;
}

int MeasureWritePacketA( )
{
	return tnBitsRequired( -5, 5 )
		+ tnBitsRequired( -10, 10 )
		+ 4
		+ 8;
}

void Sender_internal( )
{
	packet.a = 5;
	packet.b = 10;
	packet.c = 0.12f;
	packet.d = 102.0912932f;

	uint32_t memory[ 32 ];
	tnBuffer buffer = tnMakeBuffer( memory, 32 );
	WritePacketA( &buffer, &packet );

	tnSendData_internal( server_socket, server_address, buffer.words, tnSize( &buffer ) );
}

int Reciever_internal( )
{
	uint32_t memory[ 32 ];
	tnAddress address;
	int bytes_recieved = tnRecievePacket_internal( server_socket, &address, memory, 32 * 4 );

	if ( bytes_recieved )
	{
		tnBuffer buffer = tnMakeBuffer( memory, 32 );
		PacketA p;
		CHECK( ReadPacketA( &buffer, &p ) );
		CHECK( Check( packet, p ) );
	}

	return 1;
}

void TestSendAndRecieveInternal( )
{
	for ( int i = 0; i < 5; ++i )
	{
		Sender_internal( );
		Sleep( 1 );
		Reciever_internal( );
	}
}

void Sender( )
{
	packet.a = 5;
	packet.b = 10;
	packet.c = 0.12f;
	packet.d = 102.0912932f;

	tnSend( &server, PT_PACKETA, &packet );
}

int Reciever( )
{
	PacketA p;
	tnAddress from;
	int packet_type;
	int packet_size_bytes;

	tnPeak_internal( &server, &from, &packet_type, &packet_size_bytes );

	if ( server.has_packet )
	{
		int serialize_was_ok = tnGetPacketData_internal( &server, &p, packet_type );
		CHECK( serialize_was_ok );
		CHECK( Check( packet, p ) );
		return 1;
	}

	return 0;
}

void TestSendRecieveAck( )
{
	for ( int i = 0; i < 5; ++i )
	{
		Sender( );
		Sleep( 1 );
		Reciever( );
	}

	Sender( );
	Sender( );
	Sender( );
	Sleep( 1 );
	Reciever( );
	Reciever( );
	Reciever( );
}

void SoakBasicAcks( )
{
	tnNetSimDef sim;
	sim.latency = 250;
	sim.jitter = 50;
	sim.drop = 99;
	sim.corruption = 1;
	sim.duplicates = 5;
	sim.duplicates_min = 1;
	sim.duplicates_max = 3;
	sim.pool_size = 1024;
	tnAddNetSim( ctx, &sim );

	float time = 0;

	while ( 1 )
	{
		if ( GetAsyncKeyState( VK_ESCAPE ) )
			break;

		time += ttTime( );

		if ( time < dt ) continue;
		while ( time > dt )
			time -= dt;

		Sender( );
		while ( Reciever( ) );
		tnTick( ctx, time );
	}
}

inline void DoTests( )
{
	CHECK( tnBitsRequired( 0, TN_PACKET_TYPE_COUNT ) == TN_PACKET_TYPE_BITS_REQUIRED );
	CHECK( tnBitsRequired( 0, TN_MAX_RELIABLES ) == TN_MAX_RELIABLES_BITS_REQUIRED );

	CHECK( tnSWAP_INTERNAL( 0x1234ABCD ) == 0xCDAB3412 );
	CHECK( tnSWAP_INTERNAL( (int32_t)0x1234ABCD ) == 0xCDAB3412 );

	CHECK( tnPopCount( 3 ) == 2 );
	CHECK( tnPopCount( ~0 ) == 32 );
	CHECK( tnPopCount( 0xA7AE0F6F ) == (2 + 3 + 2 + 3 + 0 + 4 + 2 + 4) );

	CHECK( tnLog2( 32 ) == 5 );
	CHECK( tnLog2( 154 ) == 7 );
	CHECK( tnLog2( 0x7AC31E3 ) == 26 );
	CHECK( tnLog2( 0x8AC51E5 ) == 27 );
	
	CHECK( tnBitsRequired( 0, 256 ) == 9 );
	CHECK( tnBitsRequired( 0, 255 ) == 8 );
	CHECK( tnBitsRequired( 0, 100 ) == 7 );
	CHECK( tnBitsRequired( 0, 63 ) == 6 );
	CHECK( tnBitsRequired( 0, 5 ) == 3 );
	CHECK( tnBitsRequired( 0, ~0 ) == 32 );
	CHECK( tnBitsRequired( 256, 256 + 256 ) == 9 );
	CHECK( tnBitsRequired( 255, 255 + 255 ) == 8 );
	CHECK( tnBitsRequired( 100, 100 + 100 ) == 7 );
	CHECK( tnBitsRequired( 63, 63 + 63 ) == 6 );
	CHECK( tnBitsRequired( 5, 5 + 5 ) == 3 );

	{
		char poem[] = {
			0x74,0x68,0x65,0x20,0x73,0x70,0x69,0x64,0x65,0x72,0x0D,0x0A,0x63,0x72,0x61,0x77,
			0x6C,0x65,0x64,0x20,0x75,0x70,0x0D,0x0A,0x74,0x68,0x65,0x20,0x77,0x65,0x62,0x20,
			0x3A,0x29,0x00,0x00
		};
		const size_t word_count = (sizeof( poem ) / sizeof( *poem )) / sizeof( uint32_t );
		uint32_t* words = (uint32_t*)poem;

		tnBuffer bits = tnMakeBuffer( words, word_count );
		int i = 0;
		while ( bits.bits_left )
		{
			char c = tnReadBits_internal( &bits, 8 );
			CHECK( c == poem[ i++ ] );
		}
		CHECK( tnWouldOverflow( &bits, 1 ) );

		bits = tnMakeBuffer( words, word_count );
		i = 0;
		while ( bits.bits_left )
		{
			uint32_t w = tnReadBits_internal( &bits, 8 * 4 );
			CHECK( w == words[ i++ ] );
		}

		uint32_t buffer[ word_count ];
		bits = tnMakeBuffer( buffer, word_count );
		i = 0;
		while ( bits.bits_left )
			tnWriteBits( &bits, words[ i++ ], 4 * 8 );
		for ( i = 0; i < word_count; ++i )
			CHECK( buffer[ i ] == words[ i ] );

		uint32_t word = 0;
		bits = tnMakeBuffer( &word, 1 );
		tnWriteBits( &bits, 0x000ABCDE, 20 );
		CHECK( word == 0 );
		tnFlush( &bits );
		CHECK( word == 0x000ABCDE );
		CHECK( !tnWouldOverflow( &bits, 12 ) );
		CHECK( tnWouldOverflow( &bits, 13 ) );

		bits = tnMakeBuffer( &word, 1 );
		tnWriteBits( &bits, ~0, 16 );
		CHECK( tnWouldOverflow( &bits, 17 ) );

		bits = tnMakeBuffer( &word, 1 );
		tnReadBits_internal( &bits, 16 );
		CHECK( tnWouldOverflow( &bits, 17 ) );
	}

	CHECK( tnCRC32( "123456789", strlen( "123456789" ), 0 ) == 0xCBF43926 );

	tnAddress addr = tnMakeAddress( "127.0.0.1" );
	CHECK( addr.ipv4 == htonl( 0x7F000001 ) );
	addr = tnMakeAddress( "::1" );
	for ( int i = 0; i < 7; ++i ) CHECK( !addr.ipv6[ i ] );
	CHECK( addr.ipv6[ 7 ] == htons( 0x0001 ) );

	addr = tnMakeAddress( "127.0.0.1:1337" );
	CHECK( addr.ipv4 == htonl( 0x7F000001 ) );
	CHECK( addr.port == 1337 );
	addr = tnMakeAddress( "[::1]:2" );
	for ( int i = 0; i < 7; ++i ) CHECK( !addr.ipv6[ i ] );
	CHECK( addr.ipv6[ 7 ] == htons( 0x0001 ) );
	CHECK( addr.port == 2 );

	if ( !g_AnyFailed )
		printf( "All test cases passed!\n" );

	TestSendAndRecieveInternal( );
	TestSendRecieveAck( );

	tnSequenceBuffer buffer;
	tnSequenceBuffer* seq_buf = &buffer;
	struct tnSequenceData { int acked; };
	tnMakeSequenceBuffer( seq_buf, sizeof( tnSequenceData ) );
	CHECK( !tnGetSequenceData( seq_buf, 0 ) );
	tnSequenceData* data = (tnSequenceData*)tnInsertSequence( seq_buf, 0 );
	data->acked = 0;
	data = (tnSequenceData*)tnGetSequenceData( seq_buf, 0 );
	CHECK( data );
	CHECK( !data->acked );
	tnFreeSequenceBuffer( seq_buf );
	tnMakeSequenceBuffer( seq_buf, sizeof( tnSequenceData ) );
	for ( int i = 0; i < TN_UINT16_MAX; ++i )
	{
		tnSequenceData* data = (tnSequenceData*)tnInsertSequence( seq_buf, i );
		CHECK( data );
	}
	for ( int i = 0; i < TN_UINT16_MAX - TN_SEQUENCE_BUFFER_SIZE; ++i )
	{
		tnSequenceData* data = (tnSequenceData*)tnGetSequenceData( seq_buf, i );
		CHECK( !data );
	}
	for ( int i = TN_UINT16_MAX - TN_SEQUENCE_BUFFER_SIZE; i < TN_UINT16_MAX; ++i )
	{
		tnSequenceData* data = (tnSequenceData*)tnInsertSequence( seq_buf, i );
		CHECK( data );
	}
	tnFreeSequenceBuffer( seq_buf );
	tnMakeSequenceBuffer( seq_buf, sizeof( tnSequenceData ) );
	for ( int i = 0, j = 0; i < 32; ++i, j = !j )
	{
		if ( j )
		{
			tnSequenceData* data = (tnSequenceData*)tnInsertSequence( seq_buf, i );
			data->acked = 1;
		}
	}
	uint16_t ack;
	uint32_t ack_bits;
	seq_buf->sequence = 33;
	tnMakeAck( seq_buf, &ack, &ack_bits );
	for ( int i = 0, j = 0; i < 32; ++i, j = !j )
	{
		if ( j ) CHECK( ack_bits & (1 << i) );
		else CHECK( !(ack_bits & (1 << i)) );
	}
	CHECK( !tnGetSequenceData( seq_buf, 33 ) );
	CHECK( !((tnSequenceData*)tnInsertSequence( seq_buf, 33 ))->acked );
	tnFreeSequenceBuffer( seq_buf );
}
