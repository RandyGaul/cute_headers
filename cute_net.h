/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_net.h - v0.0
*/

/*
	docs, blah
*/

// TODO
// make asserts compile out in release
// audit the asserts and add in release mode if statements

#if !defined(CUTE_NET_H)

#define CUTE_NET_WINDOWS	1
#define CUTE_NET_MAC		2
#define CUTE_NET_UNIX		3

#if defined(_WIN32)
	#define CUTE_NET_PLATFORM CUTE_NET_WINDOWS
#elif defined(__APPLE__)
	#define CUTE_NET_PLATFORM CUTE_NET_MAC
#else
	#define CUTE_NET_PLATFORM CUTE_NET_UNIX
#endif

const char* cn_error_reason;

#define CUTE_NET_RELIABLE_BYTE_COUNT 256
#define CUTE_NET_RELIABLE_WORD_COUNT (CUTE_NET_RELIABLE_BYTE_COUNT / sizeof(uint32_t))

// clang is a bitch and aggressively deletes *(int*) = 0
#if CUTE_NET_PLATFORM == CUTE_NET_MAC && defined(__clang__)
	#define CUTE_NET_ASSERT_INTERNAL __builtin_trap()
#else
	#define CUTE_NET_ASSERT_INTERNAL *(int*)0 = 0
#endif

#define CUTE_NET_CHECK(X, Y) do { if (!(X)) { cn_error_reason = Y; return 0; } } while (0)
#define CUTE_NET_ASSERT(X) do { if (!(X)) CUTE_NET_ASSERT_INTERNAL; } while (0)
#define CUTE_NET_ALIGN(X, Y) ((((size_t)X) + ((Y) - 1)) & ~((Y) - 1))
#define CUTE_NET_MAX_ADDRESS_LEN 256
#define CUTE_NET_PROTOCOL_ID 0xC883FC1D
#define CUTE_NET_MTU 1200
#define CUTE_NET_MTU_WORDCOUNT (CUTE_NET_MTU / sizeof(uint32_t))
#define CUTE_NET_PACKET_TYPE_BYTES 4
#define CUTE_NET_CRC_BYTES 4
#define CUTE_NET_PACKET_DATA_MAX_SIZE 1024
#define CUTE_NET_MIN(a, b) ((a) < (b) ? a : b)
#define CUTE_NET_MAX(a, b) ((a) > (b) ? a : b)
#define CUTE_NET_INT16_MAX ((uint16_t)32768)
#define CUTE_NET_UINT16_MAX ((uint16_t)~0)

// TODO: create macros to detect platform and setup as necessary
#define CUTE_NET_BIG_ENDIAN 0

#if CUTE_NET_BIG_ENDIAN
	#define cn_endian(a) a = cn_swap_internal(a)
#else
	#define cn_endian(a) a
#endif

#if CUTE_NET_PLATFORM == CUTE_NET_WINDOWS

	#define NOMINMAX
	#define _WINSOCK_DEPRECATED_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS FUCK_YOU
	#define CUTE_NET_SNPRINTF _snprintf
	#include <winsock2.h>   // socket
	#include <ws2tcpip.h>   // WSA stuff
	#pragma comment(lib, "ws2_32.lib")

#elif CUTE_NET_PLATFORM == CUTE_NET_MAC || CUTE_NET_PLATFORM == CUTE_NET_UNIX

	#include <sys/socket.h> // socket
	#include <fcntl.h>      // fcntl
	#include <arpa/inet.h>  // inet_pton
	#include <unistd.h>     // close
	#include <errno.h>

#endif

// TODO
// add in preprocessor stuff to use stb_sprintf
#include <stdio.h>  // printf (debug only), sprintf
#include <stdint.h>
#include <string.h>	// memcpy, memset
#include <stdlib.h>	// atoi

#if 1
#define CUTE_NET_DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
#define CUTE_NET_DEBUG_PRINT(...)
#endif

uint16_t cn_swap_internal(uint16_t a)
{
	return ((a & 0x00FF) << 8)
		 | ((a & 0xFF00) >> 8);
}

int16_t cn_swap_internal(int16_t a)
{
	return ((a & 0x00FF) << 8)
		 | ((a & 0xFF00) >> 8);
}

uint32_t cn_swap_internal(uint32_t a)
{
	return ((a & 0x000000FF) << 24)
		 | ((a & 0x0000FF00) << 8)
		 | ((a & 0x00FF0000) >> 8)
		 | ((a & 0xFF000000) >> 24);
}

int32_t cn_swap_internal(int32_t a)
{
	return ((a & 0x000000FF) << 24)
		 | ((a & 0x0000FF00) << 8)
		 | ((a & 0x00FF0000) >> 8)
		 | ((a & 0xFF000000) >> 24);
}

union cn_u32_f32_t
{
	uint32_t uval;
	float fval;
};

union cn_u64_f64_t
{
	uint64_t uval;
	double fval;
};

float cn_swap_internal(float a)
{
	cn_u32_f32_t u;
	u.fval = a;
	u.uval = cn_swap_internal(u.uval);
	return u.fval;
}

uint32_t cn_pop_count(uint32_t x)
{
	uint32_t a = x - ((x >> 1) & 0x55555555);
	uint32_t b = (((a >> 2) & 0x33333333) + (a & 0x33333333));
	uint32_t c = (((b >> 4) + b) & 0x0f0f0f0f);
	uint32_t d = c + (c >> 8);
	uint32_t e = d + (d >> 16);
	uint32_t f = e & 0x0000003f;
	return f;
}

uint32_t cn_log2(uint32_t x)
{
	uint32_t a = x | (x >> 1);
	uint32_t b = a | (a >> 2);
	uint32_t c = b | (b >> 4);
	uint32_t d = c | (c >> 8);
	uint32_t e = d | (d >> 16);
	uint32_t f = e >> 1;
	return cn_pop_count(f);
}

uint32_t cn_bits_required(uint32_t min, uint32_t max)
{
	return (min == max) ? 0 : cn_log2(max - min) + 1;
}

struct cn_buffer_t
{
	uint64_t bits;
	uint32_t count;

	uint32_t* words;
	uint32_t word_index;
	int32_t bits_left;
	int32_t bits_total;
};

cn_buffer_t cn_make_buffer(uint32_t* words, uint32_t word_count)
{
	cn_buffer_t buffer;
	buffer.bits = 0;
	buffer.count = 0;
	buffer.words = words;
	buffer.word_index = 0;
	buffer.bits_left = word_count * sizeof(uint32_t) * 8;
	buffer.bits_total = buffer.bits_left;
	return buffer;
}

size_t cn_size(cn_buffer_t* buffer)
{
	return CUTE_NET_ALIGN(buffer->bits_total - buffer->bits_left, 32) / 8;
}

int cn_would_overflow(cn_buffer_t* buffer, uint32_t num_bits)
{
	return buffer->bits_left - (int32_t)num_bits < 0;
}

uint32_t cn_read_bits_internal(cn_buffer_t* buffer, uint32_t num_bits_to_read)
{
	CUTE_NET_ASSERT(num_bits_to_read <= 32);
	CUTE_NET_ASSERT(num_bits_to_read > 0);
	CUTE_NET_ASSERT(buffer->bits_left > 0);
	CUTE_NET_ASSERT(buffer->count <= 64);
	CUTE_NET_ASSERT(!cn_would_overflow(buffer, num_bits_to_read));

	if (buffer->count < num_bits_to_read)
	{
		buffer->bits |= (uint64_t)(cn_endian(buffer->words[buffer->word_index])) << buffer->count;
		buffer->count += 32;
		buffer->word_index += 1;
	}

	CUTE_NET_ASSERT(buffer->count >= num_bits_to_read);

	uint32_t bits = buffer->bits & (((uint64_t)1 << num_bits_to_read) - 1);
	buffer->bits >>= num_bits_to_read;
	buffer->count -= num_bits_to_read;
	buffer->bits_left -= num_bits_to_read;
	return bits;
}

void cn_write_bits(cn_buffer_t* buffer, uint32_t value, uint32_t num_bits_to_write)
{
	CUTE_NET_ASSERT(buffer);
	CUTE_NET_ASSERT(num_bits_to_write <= 32);
	CUTE_NET_ASSERT(buffer->bits_left > 0);
	CUTE_NET_ASSERT(buffer->count <= 32);
	CUTE_NET_ASSERT(!cn_would_overflow(buffer, num_bits_to_write));

	buffer->bits |= (uint64_t)(value & (((uint64_t)1 << num_bits_to_write) - 1)) << buffer->count;
	buffer->count += num_bits_to_write;
	buffer->bits_left -= num_bits_to_write;

	if (buffer->count >= 32)
	{
		buffer->words[buffer->word_index] = cn_endian((uint32_t)(buffer->bits & ((uint32_t)~0)));
		buffer->bits >>= 32;
		buffer->count -= 32;
		buffer->word_index += 1;
	}
}

void cn_flush(cn_buffer_t* buffer)
{
	CUTE_NET_ASSERT(buffer->count <= 32);

	if (buffer->count)
	{
		buffer->words[buffer->word_index] = cn_endian((uint32_t)(buffer->bits & ((uint32_t)~0)));
	}
}

static const uint32_t cn_crc32_table[256] = {
	0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
	0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
	0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
	0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
	0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
	0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
	0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
	0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
	0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
	0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
	0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
	0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
	0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
	0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
	0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
	0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
	0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
	0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
	0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
	0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
	0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
	0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
	0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
	0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
	0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
	0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
	0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
	0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
	0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
	0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
	0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
	0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};

uint32_t cn_crc32(const void* memory, size_t bytes, uint32_t crc32)
{
	uint8_t* buffer = (uint8_t*)memory;
	crc32 = ~crc32;
	for (size_t i = 0; i < bytes; ++i) 
		crc32 = (crc32 >> 8) ^ cn_crc32_table[(crc32 ^ buffer[i]) & 0xFF];
	return ~crc32;
}

enum cn_address_type
{
	CUTE_NET_ADDRESS_NONE,
	CUTE_NET_ADDRESS_IPV4,
	CUTE_NET_ADDRESS_IPV6
};

struct cn_address_t
{
	cn_address_type type;
	uint16_t port;

	union
	{
		uint32_t ipv4;
		uint16_t ipv6[8];
	};
};

cn_address_t cn_make_address(uint32_t address, int16_t port)
{
	cn_address_t addr;
	addr.type = CUTE_NET_ADDRESS_IPV4;
	addr.port = port;
	addr.ipv4 = htonl(address);
	return addr;
}

cn_address_t cn_make_address(int16_t port)
{
	cn_address_t addr;
	addr.type = CUTE_NET_ADDRESS_IPV4;
	addr.port = port;
	addr.ipv4 = htonl(INADDR_ANY);
	return addr;
}

cn_address_t cn_make_address(uint8_t a, uint8_t b, uint8_t c, uint8_t d, int16_t port)
{
	uint32_t ipv4 = (uint32_t)a | (uint32_t)b << 8 | (uint32_t)c << 16 | (uint32_t)d << 24;
	return cn_make_address(ipv4, port);
}

cn_address_t cn_make_address(sockaddr_storage* sockaddr)
{
	CUTE_NET_ASSERT(sockaddr);
	cn_address_t addr;

	switch (sockaddr->ss_family)
	{
	case AF_INET:
	{
		sockaddr_in* addr_ipv4 = (sockaddr_in*)sockaddr;
		addr.type = CUTE_NET_ADDRESS_IPV4;
		addr.port = ntohs(addr_ipv4->sin_port);
		addr.ipv4 = addr_ipv4->sin_addr.s_addr;
	}	break;

	case AF_INET6:
	{
		sockaddr_in6* addr_ipv6 = (sockaddr_in6*)sockaddr;
		addr.type = CUTE_NET_ADDRESS_IPV6;
		addr.port = ntohs(addr_ipv6->sin6_port);
		memcpy(addr.ipv6, &addr_ipv6->sin6_addr, 16);
	}	break;

	default: CUTE_NET_ASSERT(0);
	}

	return addr;
}

cn_address_t cn_make_address(const char* string)
{
	CUTE_NET_ASSERT(string);
	char memory[CUTE_NET_MAX_ADDRESS_LEN];
	strncpy(memory, string, CUTE_NET_MAX_ADDRESS_LEN - 1);
	memory[CUTE_NET_MAX_ADDRESS_LEN - 1] = 0;
	char* buffer = memory;
	cn_address_t address;
	address.type = CUTE_NET_ADDRESS_NONE;
	address.port = 0;

	// ipv6 first
	// handle [address]:port format first
	// then try inet_pton
	if (*buffer == '[')
	{
		buffer += 1;
		char* search = buffer;
		char c;
		while ((c = *search++))
		{
			if (c == ']')
			{
				if (*search == ':')
				{
					address.port = (uint16_t)atoi(search + 1);
					search[-1] = 0;
					break;
				}
			}
		}
	}

	in6_addr sockaddr6;
	if (inet_pton(AF_INET6, buffer, &sockaddr6) == 1)
	{
		memcpy(address.ipv6, &sockaddr6, 16);
		address.type = CUTE_NET_ADDRESS_IPV6;
		return address;
	}

	// now try ipv4
	// first handle format of "address:port"
	// then try inet_pton
	char* search = buffer;
	char c;
	while ((c = *search++))
	{
		if (c == ':')
		{
			address.port = (uint16_t)atoi(search);
			search[-1] = 0;
			break;
		}
	}

	sockaddr_in sockaddr4;
	if (inet_pton(AF_INET, buffer, &sockaddr4.sin_addr) == 1)
	{
		address.type = CUTE_NET_ADDRESS_IPV4;
		address.ipv4 = sockaddr4.sin_addr.s_addr;
		return address;
	}

	return address;
}

void cn_address_tString(cn_address_t address, char* buffer, int max_buffer_bytes)
{
	switch (address.type)
	{
	case CUTE_NET_ADDRESS_IPV4:
	{
		uint8_t a = address.ipv4 & 0xFF;
		uint8_t b = (address.ipv4 >> 8) & 0xFF;
		uint8_t c = (address.ipv4 >> 16) & 0xFF;
		uint8_t d = (address.ipv4 >> 24) & 0xFF;
		if (address.port) CUTE_NET_SNPRINTF(buffer, max_buffer_bytes, "%d.%d.%d.%d:%d", a, b, c, d, address.port);
		else CUTE_NET_SNPRINTF(buffer, max_buffer_bytes, "%d.%d.%d.%d", a, b, c, d);
	}	break;

	case CUTE_NET_ADDRESS_IPV6:
	{
		if (address.port)
		{
			char inet6_addrstr[INET6_ADDRSTRLEN];
			inet_ntop(AF_INET6, (void*)address.ipv6, inet6_addrstr, INET6_ADDRSTRLEN);
			CUTE_NET_SNPRINTF(buffer, max_buffer_bytes, "[%s]:%d", inet6_addrstr, address.port);
		}
		else inet_ntop(AF_INET6, (void*)address.ipv6, buffer, max_buffer_bytes);
	}	break;

	default: CUTE_NET_ASSERT(0);
	}
}

int cn_address_tEqu(cn_address_t a, cn_address_t b)
{
	if (a.type != b.type) return 0;
	if (a.port != b.port) return 0;
	switch (a.type)
	{
	case CUTE_NET_ADDRESS_IPV4: if (a.ipv4 != b.ipv4) return 0; break;
	case CUTE_NET_ADDRESS_IPV6: if (memcmp(a.ipv6, b.ipv6, sizeof(a.ipv6))) return 0; break;
	default: CUTE_NET_ASSERT(0);
	}
	return 1;
}

#if CUTE_NET_PLATFORM == CUTE_NET_WINDOWS
	typedef SOCKET cn_socket_handle_t;
#else
	typedef int cn_socket_handle_t;
#endif

enum cn_socket_tError
{
	CUTE_NET_SOCKET_ERROR_NONE,
	CUTE_NET_SOCKET_ERROR_MAKE_FAILED,
	CUTE_NET_SOCKET_ERROR_SET_NON_BLOCKING_FAILED,
	CUTE_NET_SOCKET_ERROR_SETSOCKOPT_IPV6_ONLY_FAILED,
	CUTE_NET_SOCKET_ERROR_SETSOCKOPT_RCVBUF_FAILED,
	CUTE_NET_SOCKET_ERROR_SETSOCKOPT_SNDBUF_FAILED,
	CUTE_NET_SOCKET_ERROR_BIND_IPV4_FAILED,
	CUTE_NET_SOCKET_ERROR_BIND_IPV6_FAILED,
	CUTE_NET_SOCKET_ERROR_GETSOCKNAME_IPV4_FAILED,
	CUTE_NET_SOCKET_ERROR_GETSOCKNAME_IPV6_FAILED
};

struct cn_socket_t
{
	cn_socket_handle_t handle;
	cn_address_t address;
	cn_socket_tError error_code;
};

cn_socket_t cn_make_socket(cn_address_t address, int buffer_size, int true_for_nonblocking)
{
	cn_socket_t socket;
	socket.error_code = CUTE_NET_SOCKET_ERROR_NONE;
	socket.handle = ::socket(address.type == CUTE_NET_ADDRESS_IPV6 ? AF_INET6 : AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	#if CUTE_NET_PLATFORM == CUTE_NET_WINDOWS
	if (socket.handle == INVALID_SOCKET)
	#else
	if (socket.handle <= 0)
	#endif
	{
		socket.error_code = CUTE_NET_SOCKET_ERROR_MAKE_FAILED;
		return socket;
	}

	// allow users to enforce ipv6 only
	// see: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738574(v=vs.85).aspx
	if (address.type == CUTE_NET_ADDRESS_IPV6)
	{
		int enable = 1;
		if (setsockopt(socket.handle, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&enable, sizeof(enable)))
		{
			socket.error_code = CUTE_NET_SOCKET_ERROR_SETSOCKOPT_IPV6_ONLY_FAILED;
			return socket;
		}
	}

	// set socket send/recieve buffer sizes to our chosen size
	if (setsockopt(socket.handle, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, sizeof(int)))
	{
		socket.error_code = CUTE_NET_SOCKET_ERROR_SETSOCKOPT_RCVBUF_FAILED;
		return socket;
	}

	if (setsockopt(socket.handle, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, sizeof(int)))
	{
		socket.error_code = CUTE_NET_SOCKET_ERROR_SETSOCKOPT_SNDBUF_FAILED;
		return socket;
	}

	// bind port
	switch (address.type)
	{
	case CUTE_NET_ADDRESS_IPV4:
	{
		sockaddr_in sock_address;
		sock_address.sin_family = AF_INET;
		sock_address.sin_addr.s_addr = address.ipv4;
		sock_address.sin_port = htons(address.port);

		if (bind(socket.handle, (const sockaddr*)&sock_address, sizeof(sock_address)) < 0)
		{
			socket.error_code = CUTE_NET_SOCKET_ERROR_BIND_IPV4_FAILED;
			return socket;
		}
	}	break;

	case CUTE_NET_ADDRESS_IPV6:
	{
		sockaddr_in6 sock_address;
		memset(&sock_address, 0, sizeof(sockaddr_in6));
		sock_address.sin6_family = AF_INET6;
		memcpy(&sock_address.sin6_addr, address.ipv6, sizeof(sock_address.sin6_addr));
		sock_address.sin6_port = htons(address.port);

		if (bind(socket.handle, (const sockaddr*)&sock_address, sizeof(sock_address)) < 0)
		{
			socket.error_code = CUTE_NET_SOCKET_ERROR_BIND_IPV6_FAILED;
			return socket;
		}
	}	break;

	default: CUTE_NET_ASSERT(0);
	}

	// handle auto-picked ports
	if (!address.port)
	{
		if (address.type == CUTE_NET_ADDRESS_IPV6)
		{
			struct sockaddr_in6 sin;
			socklen_t len = sizeof(sin);
			if (getsockname(socket.handle, (struct sockaddr*)&sin, &len) == -1)
			{
				socket.error_code = CUTE_NET_SOCKET_ERROR_GETSOCKNAME_IPV6_FAILED;
				return socket;
			}
			address.port = ntohs(sin.sin6_port);
		}

		else
		{
			struct sockaddr_in sin;
			socklen_t len = sizeof(sin);
			if (getsockname(socket.handle, (struct sockaddr*)&sin, &len) == -1)
			{
				socket.error_code = CUTE_NET_SOCKET_ERROR_GETSOCKNAME_IPV4_FAILED;
				return socket;
			}
			address.port = ntohs(sin.sin_port);
		}
	}

	socket.address = address;

	// set blocking/non-blocking io
	#if CUTE_NET_PLATFORM == CUTE_NET_MAC || CUTE_NET_PLATFORM == CUTE_NET_UNIX

		int nonBlocking = true_for_nonblocking;
		if (fcntl(socket.handle, F_SETFL, O_NONBLOCK, nonBlocking) == -1)
		{
			socket.error_code = CUTE_NET_SOCKET_ERROR_SET_NON_BLOCKING_FAILED;
			return socket;
		}

	#elif CUTE_NET_PLATFORM == CUTE_NET_WINDOWS

		DWORD nonBlocking = true_for_nonblocking;
		if (ioctlsocket(socket.handle, FIONBIO, &nonBlocking) != 0)
		{
			socket.error_code = CUTE_NET_SOCKET_ERROR_SET_NON_BLOCKING_FAILED;
			return socket;
		}

	#endif

	return socket;
}

void cn_close_socket(cn_socket_t* socket)
{
	if (socket->handle)
	{
		#if CUTE_NET_PLATFORM == CUTE_NET_MAC || CUTE_NET_PLATFORM == CUTE_NET_UNIX
		close(socket->handle);
		#elif CUTE_NET_PLATFORM == CUTE_NET_WINDOWS
		closesocket(socket->handle);
		#endif

		socket->handle = 0;
	}
}

typedef void (cn_write_t)(cn_buffer_t* buffer, void* data);
typedef int (cn_read_t)(cn_buffer_t* buffer, void* data);
typedef int (cn_measure_t)();

struct cn_vtable_t
{
	cn_write_t* Write;
	cn_read_t* Read;
	cn_measure_t* Measure;
	int runtime_size;
};

struct cn_sim_packet
{
	int size;
	int64_t delay;
	struct cn_transport_t* transport;
	cn_sim_packet* next;
	uint32_t words[CUTE_NET_MTU_WORDCOUNT];
};

struct cn_sim_t
{
	int latency;
	int jitter;
	int drop;
	int corruption;
	int duplicates;
	int duplicates_min;
	int duplicates_max;
	int pool_size;
	cn_sim_packet* packets;
	cn_sim_packet* free_list;
	cn_sim_packet* live_packets;
};

struct cn_sim_def_t
{
	int latency;        // milliseconds, delay before sending packets
	int jitter;         // milliseconds, random value/sign from 0-jitter
	int drop;           // percent chance, 0-100, of dropping an outgoing packet
	int corruption;     // percent chance, 0-100, of corrupting outgoing packets
	int duplicates;     // percent chance, 0-100, of duplicating outgoing packets
	int duplicates_min; // min of range of duplicate packet count
	int duplicates_max; // max of range of duplicate packet count
	int pool_size;      // num of entries for internal pool to buffer outgoing packets
};

// OPTIMIZE
// Can remove this? Modify sequence buffer to handle NO DATA somehow
// perhaps return (void*)1 if sequence exists, keep data pointer 0
struct cn_incoming_packet_data_t
{
};

#define CUTE_NET_MAX_RELIABLES 64
#define CUTE_NET_MAX_RELIABLES_BITS_REQUIRED 7
struct cn_outgoing_packet_data_t
{
	int acked;
	int64_t send_time;
	int count;
	uint16_t ids[CUTE_NET_MAX_RELIABLES];
};

struct cn_reliable_data_t
{
	int user_type;
	uint32_t data[CUTE_NET_RELIABLE_WORD_COUNT];
};

#define CUTE_NET_SEQUENCE_BUFFER_SIZE 256
struct cn_sequence_buffer_t
{
	uint16_t sequence;
	uint32_t buffer[CUTE_NET_SEQUENCE_BUFFER_SIZE];
	int stride;
	char* data;
};

void cn_make_sequence_buffer(cn_sequence_buffer_t* buffer, int stride)
{
	CUTE_NET_ASSERT(stride >= 0);
	buffer->sequence = 0;
	buffer->data = (char*)malloc(stride * CUTE_NET_SEQUENCE_BUFFER_SIZE);
	buffer->stride = stride;
	CUTE_NET_ASSERT(buffer->data);
	memset(buffer->data, 0, stride * CUTE_NET_SEQUENCE_BUFFER_SIZE);
	for (int i = 0; i < CUTE_NET_SEQUENCE_BUFFER_SIZE; ++i) buffer->buffer[i] = ~0;
}

void cn_free_sequence_buffer(cn_sequence_buffer_t* seq_buf)
{
	free(seq_buf->data);
	memset(seq_buf, 0, sizeof(cn_sequence_buffer_t));
}

void* cn_get_sequence_data(cn_sequence_buffer_t* seq_buf, uint16_t sequence)
{
	int index = sequence % CUTE_NET_SEQUENCE_BUFFER_SIZE;
	if (seq_buf->buffer[index] == sequence) return seq_buf->data + index * seq_buf->stride;
	else return 0;
}

int cn_sequence_exists(cn_sequence_buffer_t* seq_buf, uint16_t sequence)
{
	int index = sequence % CUTE_NET_SEQUENCE_BUFFER_SIZE;
	return seq_buf->buffer[index] != ~0;
}

void cn_sequence_remove(cn_sequence_buffer_t* seq_buf, uint16_t sequence)
{
	int index = sequence % CUTE_NET_SEQUENCE_BUFFER_SIZE;
	seq_buf->buffer[index] = ~0;
}

int cn_more_recent(uint16_t a, uint16_t b)
{
	int yes = (a > b) && (a - b <= CUTE_NET_INT16_MAX);
	int yes_wrap = (a < b) && (b - a  > CUTE_NET_INT16_MAX);
	return yes || yes_wrap;
}

int cn_less_recent(uint16_t a, uint16_t b)
{
	return cn_more_recent(b, a);
}

void cn_clear_entries(uint32_t* seq, int a, int b)
{
	if (b < a) b += CUTE_NET_UINT16_MAX;
	for (int i = a; i <= b; ++i) seq[i % CUTE_NET_SEQUENCE_BUFFER_SIZE] = ~0;
}

void* cn_insert_sequence(cn_sequence_buffer_t* seq_buf, uint16_t sequence)
{
 	if (cn_more_recent(sequence + 1, seq_buf->sequence))
	{
		cn_clear_entries(seq_buf->buffer, seq_buf->sequence, sequence);
		seq_buf->sequence = sequence + 1;
	}
	else if (cn_more_recent(seq_buf->sequence - CUTE_NET_SEQUENCE_BUFFER_SIZE, sequence)) return 0;
	int index = sequence % CUTE_NET_SEQUENCE_BUFFER_SIZE;
	seq_buf->buffer[index] = sequence;
	return seq_buf->data + index * seq_buf->stride;
}

void cn_make_ack(cn_sequence_buffer_t* seq, uint16_t* ack, uint32_t* ack_bits)
{
	uint16_t local = seq->sequence - 1;
	*ack = local;
	uint32_t bits = 0;

	for (int i = 0; i < 32; ++i)
	{
		uint16_t sequence = local - (uint16_t)i;
		if (cn_get_sequence_data(seq, sequence)) bits |= (1 << i);
	}

	*ack_bits = bits;
}

struct cn_context_t
{
	int vtable_count;
	cn_vtable_t* vtables;
	int use_sim;
	int running;
	cn_sim_t sim;
};

cn_vtable_t* cn_get_table(cn_context_t* ctx, int user_type)
{
	CUTE_NET_ASSERT(user_type >= 0);
	CUTE_NET_ASSERT(user_type < ctx->vtable_count);
	return ctx->vtables + user_type;
}

#if CUTE_NET_PLATFORM == CUTE_NET_WINDOWS

	typedef struct cn_platform_t
	{
		CRITICAL_SECTION critical_section;
		LARGE_INTEGER prev;
		LARGE_INTEGER freq;
	} cn_platform_t;

#elif CUTE_NET_PLATFORM == CUTE_NET_MAC

	typedef struct cn_platform_t
	{
		pthread_t thread;
		pthread_mutex_t mutex;
	} cn_platform_t;

#endif

enum cn_queue_packet_status_t
{
	CUTE_NET_QUEUE_EMPTY,
	CUTE_NET_QUEUE_NOT_PROCESSED,
	CUTE_NET_QUEUE_PROCESSED
};

typedef struct cn_queue_packet_t
{
	cn_queue_packet_status_t state;
	int64_t timestamp;
	int size;
	int user_type;
	cn_address_t from;
	uint32_t words[CUTE_NET_MTU_WORDCOUNT];
} cn_queue_packet_t;

#define CUTE_NET_QUEUE_CAPACITY 1024
typedef struct cn_queue_t
{
	int insert_count;
	int insert_index;
	int process_count;
	int process_index;
	int pop_index;
	cn_queue_packet_t packets[CUTE_NET_QUEUE_CAPACITY];
} cn_queue_t;

static int cn_pop(cn_queue_t* q, void* out, int64_t* ticks)
{
	if (q->insert_count == CUTE_NET_QUEUE_CAPACITY) return 0;
	cn_queue_packet_t* p = q->packets + q->pop_index;
	if (p->state != CUTE_NET_QUEUE_PROCESSED) return 0;
	memcpy(out, p->words, p->size);
	*ticks = p->timestamp;
	q->pop_index++;
	q->pop_index %= CUTE_NET_QUEUE_CAPACITY;
	q->insert_count++;
	return p->size;
}

static int cn_push(cn_queue_t* q, void* data, int size, cn_address_t from, int64_t ticks)
{
	if (size > CUTE_NET_MTU) return 0;
	if (!q->insert_count) return 0;
	CUTE_NET_ASSERT(q->insert_count > 0);
	CUTE_NET_ASSERT(q->insert_count <= CUTE_NET_QUEUE_CAPACITY);
	int index = q->insert_index++;
	q->insert_index %= CUTE_NET_QUEUE_CAPACITY;
	cn_queue_packet_t* p = q->packets + index;
	p->state = CUTE_NET_QUEUE_NOT_PROCESSED;
	p->timestamp = ticks;
	p->size = size;
	p->from = from;
	memcpy(p->words, data, size);
	q->insert_count--;
	q->process_count++;
	return 1;
}

static void cn_process_packet(cn_queue_packet_t* p)
{
	// decrypt
	// decompress
}

static int cn_process(cn_queue_t* q)
{
	int did_work = 0;
	while (q->process_count)
	{
		cn_queue_packet_t* p = q->packets + q->process_index;
		cn_process_packet(p);
		p->state = CUTE_NET_QUEUE_PROCESSED;
		q->process_count--;
		q->process_index++;
		q->process_index %= CUTE_NET_QUEUE_CAPACITY;
		did_work = 1;
	}
	return did_work;
}

struct cn_transport_t
{
	const char* debug_name;
	cn_context_t* ctx;
	cn_socket_t socket;
	cn_address_t to;
	cn_sequence_buffer_t incoming;
	cn_sequence_buffer_t outgoing;
	uint16_t reliable_next_incoming;
	uint16_t reliable_oldest_unacked;
	cn_sequence_buffer_t reliable_incoming;
	cn_sequence_buffer_t reliable_outgoing;
	int64_t round_trip_time;
	int round_trip_time_millis;

	// worker thread data
	int using_worker_thread;
	int sleep_milliseconds;
	cn_queue_t* q;

	// platform data
	cn_platform_t pd;
};

static void cn_add_queue(cn_transport_t* transport)
{
	cn_queue_t* q = (cn_queue_t*)malloc(sizeof(cn_queue_t));
	q->insert_count = CUTE_NET_QUEUE_CAPACITY;
	q->insert_index = 0;
	q->process_count = 0;
	q->process_index = 0;
	q->pop_index = 0;
	for (int i = 0; i < CUTE_NET_QUEUE_CAPACITY; ++i)
	{
		cn_queue_packet_t* p = q->packets + i;
		p->state = CUTE_NET_QUEUE_EMPTY;
	}
	transport->q = q;
}

int cn_do_work(cn_transport_t* transport);

#if CUTE_NET_PLATFORM == CUTE_NET_WINDOWS

	void cn_sleep(int milliseconds)
	{
		Sleep(milliseconds);
	}
	
	static void cn_lock(cn_transport_t* transport)
	{
		if (transport->using_worker_thread) EnterCriticalSection(&transport->pd.critical_section);
	}

	static void cn_unlock(cn_transport_t* transport)
	{
		if (transport->using_worker_thread) LeaveCriticalSection(&transport->pd.critical_section);
	}

	static DWORD WINAPI cn_worker_thread(LPVOID lpParameter)
	{
		cn_transport_t* transport = (cn_transport_t*)lpParameter;

		while (transport->ctx->running)
		{
			int should_continue = cn_do_work(transport);
			if (should_continue) continue;
			if (transport->sleep_milliseconds) cn_sleep(transport->sleep_milliseconds);
			else YieldProcessor();
		}

		transport->using_worker_thread = 0;
		return 0;
	}

	static int64_t cn_ticks(cn_transport_t* transport)
	{
		LARGE_INTEGER now;
		QueryPerformanceCounter(&now);
		return (int64_t)(now.QuadPart - transport->pd.prev.QuadPart);
	}

	static int64_t cn_milliseconds(cn_transport_t* transport, int64_t ticks)
	{
		return ticks / (transport->pd.freq.QuadPart / 1000);
	}

	static void cn_platform_tInit(cn_transport_t* transport)
	{
		QueryPerformanceCounter(&transport->pd.prev);
		QueryPerformanceFrequency(&transport->pd.freq);
	}

	static void cn_platform_tShutdown(cn_transport_t* transport)
	{
		if (transport->using_worker_thread) DeleteCriticalSection(&transport->pd.critical_section);
	}

	void cn_spawn_worker_thread(cn_transport_t* transport)
	{
		if (transport->using_worker_thread) return;
		transport->using_worker_thread = 1;
		InitializeCriticalSectionAndSpinCount(&transport->pd.critical_section, 0x00000400);
		cn_add_queue(transport);
		CreateThread(0, 0, cn_worker_thread, transport, 0, 0);
	}

#elif CUTE_NET_PLATFORM = CUTE_NET_MAC

	void cn_sleep(int milliseconds)
	{
		usleep(milliseconds * 1000);
	}

	static void cn_lock(cn_transport_t* transport)
	{
		if (transport->separate_thread) pthread_mutex_lock(&transport->mutex);
	}

	static void cn_unlock(cn_transport_t* transport)
	{
		if (transport->separate_thread) pthread_mutex_unlock(&transport->mutex);
	}

	static void* cn_worker_thread(void* udata)
	{
		cn_transport_t* ctx = (cn_transport_t*)udata;

		while (ctx->running)
		{
			if (ctx->sleep_milliseconds) cn_sleep(ctx->sleep_milliseconds);
			else pthread_yield_np();
		}

		ctx->using_worker_thread = 0;
		pthread_exit(0);
		return 0;
	}

	static void cn_platform_tInit(cn_transport_t* transport)
	{
	}

	static void cn_platform_tShutdown(cn_transport_t* transport)
	{
		if (transport->using_worker_thread) pthread_mutex_destroy(&transport->pd.mutex);
	}

	void cn_spawn_worker_thread(cn_transport_t* transport)
	{
		if (ctx->using_worker_thread) return;
		ctx->using_worker_thread = 1;
		pthread_mutex_init(&transport->mutex, 0);
		cn_add_queue(transport);
		pthread_create(&transport->thread, 0, cn_worker_thread, transport);
	}

#endif

void cn_make_transport(cn_transport_t* transport, cn_context_t* ctx, cn_socket_t socket, cn_address_t to, const char* debug_name)
{
	transport->debug_name = debug_name;
	transport->ctx = ctx;
	transport->socket = socket;
	transport->to = to;
	cn_make_sequence_buffer(&transport->incoming, sizeof(cn_incoming_packet_data_t));
	cn_make_sequence_buffer(&transport->outgoing, sizeof(cn_outgoing_packet_data_t));
	transport->reliable_next_incoming = 0;
	transport->reliable_oldest_unacked = 0;
	cn_make_sequence_buffer(&transport->reliable_incoming, sizeof(cn_reliable_data_t));
	cn_make_sequence_buffer(&transport->reliable_outgoing, sizeof(cn_reliable_data_t));
	transport->using_worker_thread = 0;
	transport->sleep_milliseconds = 0;
	transport->q = 0;
	cn_platform_tInit(transport);
}

void cn_free_transport(cn_transport_t* transport)
{
	cn_free_sequence_buffer(&transport->incoming);
	cn_free_sequence_buffer(&transport->outgoing);
	cn_free_sequence_buffer(&transport->reliable_incoming);
	cn_free_sequence_buffer(&transport->reliable_outgoing);
	memset(transport, 0, sizeof(cn_transport_t));

	if (transport->using_worker_thread)
	{
		cn_lock(transport);
		transport->ctx->running = 0;
		cn_unlock(transport);
	}

	while (transport->using_worker_thread) cn_sleep(1);

	cn_platform_tShutdown(transport);
}

void cn_assert_transport(cn_transport_t* transport)
{
	CUTE_NET_ASSERT(transport);
	CUTE_NET_ASSERT(transport->ctx);
}

void cn_write_stub_internal(cn_buffer_t* buffer, void* data) { (void)buffer; (void)data; }
int cn_read_stub_internal(cn_buffer_t* buffer, void* data) { (void)buffer; (void)data; return 1; }
int cn_measure_stub_internal() { return 0; }

// TODO
// refactor this
// should increment and return type index
int cn_register(cn_context_t* ctx, int type_index, cn_write_t write, cn_read_t read, cn_measure_t measure, int runtime_size)
{
	CUTE_NET_CHECK(type_index != 0, "cn_register abort: zero for type_index is reserved for internal use.");
	CUTE_NET_CHECK(type_index > 0, "cn_register abort: type_index invalid value.");
	CUTE_NET_CHECK(type_index < ctx->vtable_count, "cn_register abort: type_index invalid value.");

	cn_vtable_t table = { write, read, measure, runtime_size };
	ctx->vtables[type_index] = table;
	return 1;
}

cn_context_t* cn_init(int num_packet_types)
{
	#if CUTE_NET_PLATFORM == CUTE_NET_WINDOWS
	WSADATA WsaData;
	WSAStartup(MAKEWORD(2, 2), &WsaData);
	#endif

	int req = cn_bits_required(0, num_packet_types);
	CUTE_NET_CHECK(req < CUTE_NET_PACKET_TYPE_BYTES * 8, "Please make CUTE_NET_PACKET_TYPE_NUM_BITS larger.");

	cn_context_t* ctx = (cn_context_t*)malloc(sizeof(cn_context_t));
	ctx->vtable_count = num_packet_types;
	ctx->vtables = (cn_vtable_t*)calloc(1, sizeof(cn_vtable_t) * num_packet_types);
	ctx->use_sim = 0;
	cn_vtable_t stub = { cn_write_stub_internal, cn_read_stub_internal, cn_measure_stub_internal, 0 };
	ctx->vtables[0] = stub;
	ctx->running = 1;

	return ctx;
}

void cn_shutdown(cn_context_t* ctx)
{
	#if CUTE_NET_PLATFORM == CUTE_NET_WINDOWS
	WSACleanup();
	#endif

	free(ctx->vtables);
	if (ctx->use_sim) free(ctx->sim.packets);
	free(ctx);
}

enum cn_packet_type_internal_t
{
	CUTE_NET_PACKET_TYPE_NONE,
	CUTE_NET_PACKET_TYPE_UNRELIABLE,	// packet contained no reliable data
	CUTE_NET_PACKET_TYPE_RELIABLE,	// packet contained some reliable data
	CUTE_NET_PACKET_TYPE_SLICE,		// packet was a chunk slice
	CUTE_NET_PACKET_TYPE_COUNT,
};

// TODO: use this to compress packet header bytes?
#define CUTE_NET_PACKET_TYPE_BITS_REQUIRED 3

void cn_add_sim(cn_context_t* ctx, cn_sim_def_t* def)
{
	CUTE_NET_ASSERT(ctx);
	cn_sim_t* sim = &ctx->sim;
	sim->latency = def->latency;
	sim->jitter = def->jitter;
	sim->drop = def->drop;
	sim->corruption = def->corruption;
	sim->duplicates = def->duplicates;
	sim->duplicates_min = def->duplicates_min;
	sim->duplicates_max = def->duplicates_max;
	sim->pool_size = def->pool_size;
	CUTE_NET_ASSERT(sim->duplicates_min <= sim->duplicates_max);
	CUTE_NET_ASSERT(sim->duplicates_min >= 0);
	CUTE_NET_ASSERT(sim->duplicates_max >= 0);
	ctx->use_sim = 1;
	sim->packets = (cn_sim_packet*)malloc(sizeof(cn_sim_packet) * sim->pool_size);
	sim->free_list = sim->packets;
	for (int i = 0; i < sim->pool_size - 1; ++i)
		sim->free_list[i].next = sim->free_list + i + 1;
	sim->free_list[sim->pool_size - 1].next = 0;
	sim->live_packets = 0;
}

int cn_random_int(int a, int b)
{
	return a + rand() % (b - a + 1);
}

float cn_random_float(float a, float b)
{
	float x = (float)rand();
	x /= (float)RAND_MAX;
	return (b - a) * x + a;
}

int cn_send_internal(cn_socket_t socket, cn_address_t to, void* data, int bytes);

void cn_flush_sim(cn_context_t* ctx)
{
	CUTE_NET_ASSERT(ctx);
	if(!ctx->use_sim) return;
	cn_sim_t* sim = &ctx->sim;
	int dup, corrupt, drop_chance;
	cn_sim_packet** ptr = &sim->live_packets;

	while (*ptr)
	{
		cn_sim_packet* packet = *ptr;
		cn_transport_t* transport = packet->transport;
		if (packet->delay > cn_ticks(transport)) goto packet_next;

		// chance to skip packet
		drop_chance = cn_random_int(0, 100);
		if (drop_chance < sim->drop) goto packet_remove;

		// chance to flip random bit
		corrupt = cn_random_int(0, 100);
		if (corrupt < sim->corruption) corrupt = 1;
		else corrupt = 0;

		// dup chance, send copies
		dup = cn_random_int(0, 100);
		if (dup < sim->duplicates) dup = cn_random_int(sim->duplicates_min, sim->duplicates_max) + 1;
		else dup = 1;

		if (corrupt)
		{
			int byte = cn_random_int(0, packet->size - 1);
			int bit = cn_random_int(0, 8 - 1);
			*(((char*)(packet->words)) + byte) ^= 1 << bit;
		}

		for (int i = 0; i < dup; ++i)
			cn_send_internal(transport->socket, transport->to, packet->words, packet->size);

	packet_remove:
		*ptr = (*ptr)->next;
		packet->next = sim->free_list;
		sim->free_list = packet;
		continue;

	packet_next:
		if (*ptr) ptr = &(*ptr)->next;
	}
}

int cn_send_internal(cn_socket_t socket, cn_address_t to, void* data, int bytes)
{
	CUTE_NET_ASSERT(data);
	CUTE_NET_ASSERT(bytes > 0);
	CUTE_NET_ASSERT(to.type);
	CUTE_NET_ASSERT(socket.handle);

	int sent_bytes = 0;

	switch (to.type)
	{
	case CUTE_NET_ADDRESS_IPV4:
	{
		sockaddr_in socket_address;
		memset(&socket_address, 0, sizeof(socket_address));
		socket_address.sin_family = AF_INET;
		socket_address.sin_addr.s_addr = to.ipv4;
		socket_address.sin_port = htons(to.port);
		sent_bytes = sendto(socket.handle, (const char*)data, bytes, 0, (sockaddr*)&socket_address, sizeof(sockaddr_in));
	}	break;

	case CUTE_NET_ADDRESS_IPV6:
	{
		sockaddr_in6 socket_address;
		memset(&socket_address, 0, sizeof(socket_address));
		socket_address.sin6_family = AF_INET6;
		socket_address.sin6_port = htons(to.port);
		memcpy(&socket_address.sin6_addr, to.ipv6, sizeof(socket_address.sin6_addr));
		sent_bytes = sendto(socket.handle, (const char*)data, bytes, 0, (sockaddr*)&socket_address, sizeof(sockaddr_in6));
	}	break;

	default: CUTE_NET_ASSERT(0);
	}

	return sent_bytes == bytes;
}

int cn_recieve_internal(cn_socket_t socket, cn_address_t* from, void* data, int max_packet_size_bytes)
{
	CUTE_NET_ASSERT(data);
	CUTE_NET_ASSERT(socket.handle);
	CUTE_NET_ASSERT(max_packet_size_bytes > 0);

	sockaddr_storage sockaddr_from;
	socklen_t fromlen = sizeof(sockaddr_from);
	int bytes_recieved = recvfrom(socket.handle, (char*)data, max_packet_size_bytes, 0, (sockaddr*)&sockaddr_from, &fromlen);

	#if CUTE_NET_PLATFORM == CUTE_NET_WINDOWS

		if (bytes_recieved == SOCKET_ERROR)
		{
			int error = WSAGetLastError();
			if (error == WSAEWOULDBLOCK) return 0;
			CUTE_NET_DEBUG_PRINT("recvfrom failed with error %d\n", error);
			return 0;
		}

	#else

		if (bytes_recieved <= 0)
		{
			if (errno == EAGAIN) return 0;
			CUTE_NET_DEBUG_PRINT("recvfrom failed with error %d\n", errno);
			return 0;
		}

	#endif

	if (from) *from = cn_make_address(&sockaddr_from);
	CUTE_NET_ASSERT(bytes_recieved >= 0);
	return bytes_recieved;
}

void cn_write_i32(cn_buffer_t* buffer, int32_t val, int32_t min, int32_t max)
{
	CUTE_NET_ASSERT(min < max);
	CUTE_NET_ASSERT(val >= min);
	CUTE_NET_ASSERT(val <= max);
	int req = cn_bits_required(min, max);
	cn_write_bits(buffer, (uint32_t)(val - min), req);
}

void cn_write_u32(cn_buffer_t* buffer, uint32_t val)
{
	cn_write_bits(buffer, val, 32);
}

void cn_write_u64(cn_buffer_t* buffer, uint64_t val)
{
	cn_write_bits(buffer, val & 0xFFFFFFFF, 32);
	cn_write_bits(buffer, val >> 32, 32);
}

void cn_write_f32(cn_buffer_t* buffer, float val)
{
	cn_u32_f32_t u32f32;
	u32f32.fval = val;
	cn_write_u32(buffer, u32f32.uval);
}

void cn_write_f64(cn_buffer_t* buffer, double val)
{
	cn_u64_f64_t u64f64;
	u64f64.fval = val;
	cn_write_u64(buffer, u64f64.uval);
}

// OPTIMIZE
// Change this to write words at a time. Handle begin/end alignments
// carefully. This will be faster than doing 8 bit chunks. Should probably
// factor out a cn_write_tBytes function as well and then use that here.
void cn_write_string(cn_buffer_t* buffer, char* string, int buffer_size)
{
	CUTE_NET_ASSERT(string);
	int len = strlen(string);
	CUTE_NET_ASSERT(len + 1 < buffer_size);
	cn_write_i32(buffer, len, 0, buffer_size - 1);
	for (int i = 0; i < len; ++i)
		cn_write_bits(buffer, string[i], 8);
}

// OPTIMIZE
// remove above optimize, and apply it here as well
void cn_write_string(cn_buffer_t* buffer, char* string, int len_not_including_nul_byte, int buffer_size)
{
	CUTE_NET_ASSERT(string);
	int len = len_not_including_nul_byte;
	CUTE_NET_ASSERT(len + 1 < buffer_size);
	cn_write_i32(buffer, len, 0, buffer_size - 1);
	for (int i = 0; i < len; ++i)
		cn_write_bits(buffer, string[i], 8);
}

void cn_write_address(cn_buffer_t* buffer, cn_address_t address)
{
	char address_string[CUTE_NET_MAX_ADDRESS_LEN];
	cn_address_tString(address, address_string, CUTE_NET_MAX_ADDRESS_LEN);
	cn_write_string(buffer, address_string, CUTE_NET_MAX_ADDRESS_LEN);
}

#define cn_read_bits(buffer, uint_ptr, num_bits) \
	do { \
		CUTE_NET_ASSERT(buffer); \
		CUTE_NET_ASSERT(uint_ptr); \
		CUTE_NET_CHECK(!cn_would_overflow(buffer, num_bits), "Packet overflow reading bits."); \
		*(uint_ptr) = cn_read_bits_internal(buffer, num_bits); \
	} while (0)

#define cn_read_i16(buffer, uint_ptr) \
	do { \
		cn_read_bits(buffer, uint_ptr, 16); \
	} while (0)

#define cn_read_i32(buffer, int32_ptr, min, max) \
	do { \
		int32_t val; \
		CUTE_NET_CHECK(cn_read_i32_internal(buffer, &val, min, max), "Packet overflow during cn_read_t."); \
		CUTE_NET_CHECK(val >= (min) && val <= (max), "cn_read_i32 found out of bounds int32_t while reading packet."); \
		*(int32_ptr) = val; \
	} while (0)

int cn_read_i32_internal(cn_buffer_t* buffer, int32_t* val, int32_t min, int32_t max)
{
	CUTE_NET_ASSERT(buffer);
	CUTE_NET_ASSERT(val);
	CUTE_NET_ASSERT(min < max);
	int req = cn_bits_required(min, max);
	cn_read_bits(buffer, val, req);
	*val += min;
	return 1;
}

#define cn_read_u32(buffer, uint32_ptr) \
	do { \
		CUTE_NET_CHECK(cn_read_u32_internal(buffer, uint32_ptr), "Packet overflow reading uint32_t."); \
	} while (0)

int cn_read_u32_internal(cn_buffer_t* buffer, uint32_t* val)
{
	cn_read_bits(buffer, val, 32);
	return 1;
}

#define cn_read_u64(buffer, uint64_ptr) \
	do { \
		CUTE_NET_CHECK(cn_read_u64_internal(buffer, uint64_ptr), "Packet overflow reading uint64_t."); \
	} while (0)

int cn_read_u64_internal(cn_buffer_t* buffer, uint64_t* val)
{
	CUTE_NET_ASSERT(buffer);
	CUTE_NET_ASSERT(val);
	uint32_t a;
	uint32_t b;
	cn_read_u32(buffer, &a);
	cn_read_u32(buffer, &b);
	*val = ((uint64_t)b << 32) | a;
	return 1;
}

#define cn_read_f32(buffer, float_ptr) \
	do { \
		CUTE_NET_CHECK(cn_read_f32_internal(buffer, float_ptr), "Packet overflow reading float."); \
	} while (0)

int cn_read_f32_internal(cn_buffer_t* buffer, float* val)
{
	CUTE_NET_ASSERT(val);
	cn_u32_f32_t u32f32;
	cn_read_u32(buffer, &u32f32.uval);
	*val = u32f32.fval;
	return 1;
}

#define cn_read_f64(buffer, double_ptr) \
	do { \
		CUTE_NET_CHECK(cn_read_f64_internal(buffer, double_ptr), "Packet overflow reading double."); \
	} while (0)

int cn_read_f64_internal(cn_buffer_t* buffer, double* val)
{
	CUTE_NET_ASSERT(val);
	cn_u64_f64_t u64f64;
	cn_read_u64(buffer, &u64f64.uval);
	*val = u64f64.fval;
	return 1;
}

#define cn_read_string(buffer, buffer_ptr, buffer_size) \
	do { \
		CUTE_NET_CHECK(cn_read_string_internal(buffer, buffer_ptr, buffer_size), "Packet overflow reading string (byte array)."); \
	} while (0)

int cn_read_string_internal(cn_buffer_t* buffer, char* val, int buffer_size)
{
	int len;
	cn_read_i32(buffer, &len, 0, buffer_size - 1);
	for (int i = 0; i < len; ++i)
		cn_read_bits(buffer, val + i, 8);
	val[len] = 0;
	return 1;
}

#define cn_read_address(buffer, address_ptr) \
	do { \
		CUTE_NET_CHECK(cn_read_address_internal(buffer, address), "Packet read un-parsable address."); \
	} while (0)

int cn_read_address_internal(cn_buffer_t* buffer, cn_address_t* address)
{
	char address_string[CUTE_NET_MAX_ADDRESS_LEN];
	cn_read_string(buffer, address_string, CUTE_NET_MAX_ADDRESS_LEN);
	*address = cn_make_address(address_string);
	if (address->type == CUTE_NET_ADDRESS_NONE) return 0;
	return 1;
}

int cn_send_internal(cn_transport_t* transport, cn_packet_type_internal_t internal_packet_type, int user_type, void* data)
{
	cn_assert_transport(transport);
	CUTE_NET_ASSERT(internal_packet_type >= 0);
	CUTE_NET_ASSERT(internal_packet_type < CUTE_NET_PACKET_TYPE_COUNT);

	cn_context_t* ctx = transport->ctx;
	uint32_t words[CUTE_NET_MTU_WORDCOUNT];
	cn_buffer_t buffer = cn_make_buffer(words, CUTE_NET_MTU_WORDCOUNT);
	cn_buffer_t* b = &buffer;
	cn_vtable_t* table = cn_get_table(ctx, user_type);

	CUTE_NET_CHECK(table->Measure() < CUTE_NET_PACKET_DATA_MAX_SIZE, "cn_sendPacket aborted: size of this packet is too large to fit into the internal buffer.");
	cn_write_u32(b, CUTE_NET_PROTOCOL_ID);
	cn_write_bits(b, internal_packet_type, 16);
	cn_write_bits(b, user_type, 16);
	uint16_t packet_sequence = transport->outgoing.sequence;
	cn_sequence_buffer_t* outgoing = &transport->outgoing;

	switch (internal_packet_type)
	{
	case CUTE_NET_PACKET_TYPE_UNRELIABLE:
	case CUTE_NET_PACKET_TYPE_RELIABLE:
	{
		// Fiedler's ack algorithm
		// http://gafferongames.com/building-a-game-network-protocol/reliable-ordered-messages/
		uint16_t ack;
		uint32_t ack_bits;
		cn_make_ack(&transport->incoming, &ack, &ack_bits);

		cn_write_bits(b, packet_sequence, 16);
		cn_write_bits(b, ack, 16);
		cn_write_u32(b, ack_bits);

		cn_outgoing_packet_data_t* data = (cn_outgoing_packet_data_t*)cn_insert_sequence(outgoing, packet_sequence);
		CUTE_NET_ASSERT(data);
		data->acked = 0;
		data->send_time = cn_ticks(transport);
		printf("send_time %d\n", (int)data->send_time);
		data->count = 0;
	}	break;

	case CUTE_NET_PACKET_TYPE_SLICE:
		break;

	default:
		CUTE_NET_CHECK(0, "cn_sendPacket aborted: unidentified packet type.");
	}

	table->Write(b, data);

	// OPTIMIZE: use of cn_size here aligns to 32bit boundary. Can squeeze in more data possibly
	cn_sequence_buffer_t* reliable_out = &transport->reliable_outgoing;
	uint16_t reliable_id = transport->reliable_oldest_unacked;
	uint16_t reliable_last = reliable_out->sequence;
	cn_outgoing_packet_data_t* packet_data = (cn_outgoing_packet_data_t*)cn_get_sequence_data(outgoing, packet_sequence);
	int overhead_bits = 16 + 16;

	// Count up how many reliables we can fit, in priority order
	int count = 0;
	uint16_t ids[CUTE_NET_MAX_RELIABLES];

	int type, overhead;
	while (cn_more_recent(reliable_last, reliable_id))
	{
		cn_reliable_data_t* reliable = (cn_reliable_data_t*)cn_get_sequence_data(reliable_out, reliable_id);
		if (!reliable) goto next_reliable;
		type = reliable->user_type;
		table = cn_get_table(ctx, type);
		overhead = overhead_bits + table->Measure();

		if (!cn_would_overflow(b, overhead))
			ids[count++] = reliable_id;

		if (count == CUTE_NET_MAX_RELIABLES)
			break;

	next_reliable:
		++reliable_id;
	}

	// Place reliable data into the packet
	cn_write_bits(b, count, CUTE_NET_MAX_RELIABLES_BITS_REQUIRED);

	for (int i = 0; i < count; ++i)
	{
		cn_reliable_data_t* reliable = (cn_reliable_data_t*)cn_get_sequence_data(reliable_out, ids[i]);
		CUTE_NET_ASSERT(reliable);
		int type = reliable->user_type;
		cn_vtable_t* table = cn_get_table(ctx, type);
		cn_write_bits(b, ids[i], 16);
		cn_write_bits(b, type, 16);
		table->Write(b, reliable->data);
	}

	if (count)
	{
		packet_data->count = count;
		memcpy(packet_data->ids, ids, sizeof(uint16_t) * count);
	}

	cn_flush(b);

	//CUTE_NET_DEBUG_PRINT("SENDING %d .. %d FROM PACKET %d %s\n", ids[0] % CUTE_NET_SEQUENCE_BUFFER_SIZE, ids[count - 1] % CUTE_NET_SEQUENCE_BUFFER_SIZE, packet_sequence, transport->debug_name);

	// set crc
	uint32_t crc = cn_crc32(b->words + 1, cn_size(b) - 4, CUTE_NET_PROTOCOL_ID);
	b->words[0] = crc;

	// buffer packet for net sim, or send along immediately
	if (ctx->use_sim)
	{
		cn_sim_t* sim = &ctx->sim;
		int delay = sim->latency + cn_random_int(-sim->jitter, sim->jitter);
		cn_sim_packet* packet = sim->free_list;
		if (!packet)
		{
			CUTE_NET_DEBUG_PRINT("Packet pool full, dropping packet.\n");
			return 0;
		}
		packet->size = cn_size(b);
		packet->delay = delay;
		packet->transport = transport;
		sim->free_list = packet->next;
		packet->next = sim->live_packets;
		sim->live_packets = packet;
		memcpy(packet->words, words, packet->size);
		return 1;
	}
	else return cn_send_internal(transport->socket, transport->to, words, cn_size(b));
}

void cn_on_ack_internal(cn_transport_t* transport, uint16_t sequence, int64_t ticks)
{
	//CUTE_NET_DEBUG_PRINT("GOT ACK FOR PACKET %d %s\n", sequence, transport->debug_name);

	cn_sequence_buffer_t* reliable_out = &transport->reliable_outgoing;
	cn_outgoing_packet_data_t* data = (cn_outgoing_packet_data_t*)cn_get_sequence_data(&transport->outgoing, sequence);
	CUTE_NET_ASSERT(data); // TODO: when can this be false?

	// record round-trip time
	int64_t prev_time = data->send_time;
	int64_t now_time = ticks;
	int64_t this_rtt = now_time - prev_time;
	double a = (double)transport->round_trip_time;
	double b = (double)this_rtt;
	double c = (b - a) * 0.1 + a;
	transport->round_trip_time = this_rtt;// (int)(c + 0.5);
	transport->round_trip_time_millis = (int)cn_milliseconds(transport, transport->round_trip_time);

	// remove acked reliables
	int count = data->count;
	uint16_t* ids = data->ids;
	//CUTE_NET_DEBUG_PRINT("PACKET %d HAD %d .. %d RELIABLES %s\n", sequence, ids[0] % CUTE_NET_SEQUENCE_BUFFER_SIZE, ids[count - 1] % CUTE_NET_SEQUENCE_BUFFER_SIZE, transport->debug_name);
	for (int i = 0; i < count; ++i)
	{
		uint16_t id = ids[i];
		if (cn_get_sequence_data(reliable_out, id))
		{
			cn_sequence_remove(reliable_out, ids[i]);
			//CUTE_NET_DEBUG_PRINT("REMOVE SENT RELIABLE %d %s\n", ids[i] % CUTE_NET_SEQUENCE_BUFFER_SIZE, transport->debug_name);
		}
	}

	// update reliable_oldest_unacked
	uint16_t oldest = transport->reliable_oldest_unacked;
	uint16_t prev_oldest = oldest;
	uint16_t stop_at = reliable_out->sequence;

	while (1)
	{
		if (oldest == stop_at) break;
		if (cn_get_sequence_data(reliable_out, oldest)) break;

		++oldest;
	}

	CUTE_NET_ASSERT(!cn_more_recent(oldest, stop_at));
	//CUTE_NET_DEBUG_PRINT("NEXT TO SEND %d, USED TO BE %d, stop_at is %d %s\n", oldest % CUTE_NET_SEQUENCE_BUFFER_SIZE, prev_oldest % CUTE_NET_SEQUENCE_BUFFER_SIZE, stop_at % CUTE_NET_SEQUENCE_BUFFER_SIZE, transport->debug_name);
	transport->reliable_oldest_unacked = oldest;
}

// header seems to be 8 bytes
int cn_read_packet_header(cn_transport_t* transport, uint32_t* words, int bytes, int* user_type, int64_t ticks)
{
	CUTE_NET_ASSERT(user_type);
	*user_type = ~0;
	cn_buffer_t buffer = cn_make_buffer(words, CUTE_NET_MTU_WORDCOUNT);
	cn_buffer_t* b = &buffer;
	uint32_t recieved_crc;
	cn_read_u32(b, &recieved_crc);
	uint32_t crc = cn_crc32(words + 1, bytes - 4, CUTE_NET_PROTOCOL_ID);

	if (crc == recieved_crc)
	{
		cn_packet_type_internal_t internal_type;
		cn_read_i16(b, (uint32_t*)&internal_type);
		cn_read_i16(b, user_type);

		switch (internal_type)
		{
		case CUTE_NET_PACKET_TYPE_UNRELIABLE:
		case CUTE_NET_PACKET_TYPE_RELIABLE:
		{
			// Fiedler's ack algorithm
			// http://gafferongames.com/building-a-game-network-protocol/reliable-ordered-messages/
			uint16_t sequence;
			uint32_t ack;
			uint32_t ack_bits;
			cn_read_i16(b, &sequence);
			cn_read_i16(b, &ack);
			cn_read_u32(b, &ack_bits);

			//CUTE_NET_DEBUG_PRINT("GOT PACKET %d %s\n", sequence, transport->debug_name);

			cn_sequence_buffer_t* incoming = &transport->incoming;
			cn_sequence_buffer_t* outgoing = &transport->outgoing;
			cn_insert_sequence(incoming, sequence);

			cn_outgoing_packet_data_t* data = (cn_outgoing_packet_data_t*)cn_get_sequence_data(outgoing, sequence);

			for (uint16_t i = 0; i < 32; ++i)
			{
				if (ack_bits & (1 << i))
				{
					uint16_t index = ack - i;
					cn_outgoing_packet_data_t* data = (cn_outgoing_packet_data_t*)cn_get_sequence_data(outgoing, index);

					if (data && !data->acked)
					{
						data->acked = 1;
						cn_on_ack_internal(transport, index, ticks);
					}
				}
			}
		}	break;

		case CUTE_NET_PACKET_TYPE_SLICE:
			break;

		default:
			CUTE_NET_CHECK(0, "cn_sendPacket aborted: unidentified packet type.");
		}

		//*packet_size_bytes = bytes - 8;
	}

	else
	{
		CUTE_NET_DEBUG_PRINT("bad crc\n");
		CUTE_NET_CHECK(0, "cn_sendPacket aborted: bad crc.\n");
	}

	return 1;
}

int cn_peak_internal(cn_transport_t* transport, cn_address_t* from, uint32_t* words)
{
	cn_assert_transport(transport);
	return cn_recieve_internal(transport->socket, from, words, CUTE_NET_MTU);
}

int cn_get_packet_data_internal(cn_transport_t* transport, uint32_t* words, void* data, int user_type)
{
	cn_assert_transport(transport);
	CUTE_NET_ASSERT(data);

	// grab the packet data
	int packet_type;
	cn_buffer_t buffer = cn_make_buffer(words + 1, CUTE_NET_MTU_WORDCOUNT);
	cn_buffer_t* b = &buffer;
	cn_read_i16(b, &packet_type);
	int offset = 0;
	switch (packet_type)
	{
	case CUTE_NET_PACKET_TYPE_UNRELIABLE:
	case CUTE_NET_PACKET_TYPE_RELIABLE: offset = 4; break;
	case CUTE_NET_PACKET_TYPE_SLICE: CUTE_NET_CHECK(0, "not implemented.");
	default: CUTE_NET_CHECK(0, "cn_get_packet_data aborted: unknown packet type.");
	}
	buffer = cn_make_buffer(words + offset, CUTE_NET_MTU_WORDCOUNT);
	cn_vtable_t* table = cn_get_table(transport->ctx, user_type);
	CUTE_NET_CHECK(table->Read(b, data), "cn_get_packet_data aborted: failed to read packet data with user-provided read function.");

	uint16_t min_reliable = transport->reliable_next_incoming;
	uint16_t max_reliable = min_reliable + CUTE_NET_SEQUENCE_BUFFER_SIZE - 1;

	// load up any reliables from the packet
	cn_sequence_buffer_t* incoming = &transport->reliable_incoming;
	int count;
	cn_read_bits(b, &count, CUTE_NET_MAX_RELIABLES_BITS_REQUIRED);
	while (count--)
	{
		uint16_t id;
		int type;
		cn_read_i16(b, &id);
		cn_read_i16(b, &type);
		cn_vtable_t* table = cn_get_table(transport->ctx, type);
		cn_reliable_data_t* reliable;

		if (cn_less_recent(id, min_reliable)) goto skip;
		if (cn_more_recent(id, max_reliable)) goto skip;
		if (cn_get_sequence_data(incoming, id)) goto skip;

		//CUTE_NET_DEBUG_PRINT("ADD RELIABLE INCOMING %d, %s\n", id % CUTE_NET_SEQUENCE_BUFFER_SIZE, transport->debug_name);
		reliable = (cn_reliable_data_t*)cn_insert_sequence(incoming, id);
		CUTE_NET_ASSERT(reliable);
		reliable->user_type = type;
		CUTE_NET_CHECK(table->runtime_size < CUTE_NET_RELIABLE_BYTE_COUNT, "cn_get_packet_data_internal aborted: found reliable data too big to fit into CUTE_NET_RELIABLE_BYTE_COUNT sized buffer.");
		CUTE_NET_CHECK(table->Read(b, reliable->data), "cn_get_packet_data_internal aborted: failed to read reliable data from user-provided read function.");
		continue;

	skip:
		char burn[CUTE_NET_RELIABLE_BYTE_COUNT];
		CUTE_NET_CHECK(table->Read(b, burn), "cn_get_packet_data_internal aborted: failed to read reliable data from user-provided read function.");
	}

	return 1;
}

int cn_get_packet(cn_transport_t* transport, cn_address_t* from, int* user_type, void* buffer)
{
	*user_type = 0;
	int packet_type;
	uint32_t words[CUTE_NET_MTU_WORDCOUNT];
	int bytes = 0;
	int64_t ticks = 0;

	if (transport->using_worker_thread)
	{
		cn_lock(transport);
		bytes = cn_pop(transport->q, words, &ticks);
		cn_unlock(transport);
	}
	else
		bytes = cn_peak_internal(transport, from, words);

	if (bytes)
	{
		if (!cn_read_packet_header(transport, words, bytes, &packet_type, ticks)) return 0;
		*user_type = packet_type;
		return cn_get_packet_data_internal(transport, words, buffer, packet_type);
	}
	return 0;
}

int cn_send(cn_transport_t* transport, int user_type, void* data_payload)
{
	cn_assert_transport(transport);
	return cn_send_internal(transport, CUTE_NET_PACKET_TYPE_UNRELIABLE, user_type, data_payload);
}

int cn_reliable(cn_transport_t* transport, int user_type, void* data)
{
	cn_assert_transport(transport);
	cn_sequence_buffer_t* out = &transport->reliable_outgoing;
	//uint16_t min_reliable = transport->reliable_oldest_unacked;
	//uint16_t max_reliable = min_reliable + CUTE_NET_SEQUENCE_BUFFER_SIZE - 1;
	//if (cn_less_recent(out->sequence, min_reliable)) return 0;
	//if (cn_more_recent(out->sequence, max_reliable)) return 0;
	cn_vtable_t* table = cn_get_table(transport->ctx, user_type);
	CUTE_NET_CHECK(table->runtime_size < CUTE_NET_RELIABLE_BYTE_COUNT, "cn_reliable abort: user_type has a runtime size too large. Max is CUTE_NET_RELIABLE_BYTE_COUNT.");
	if (cn_sequence_exists(out, out->sequence))
	{
		//CUTE_NET_DEBUG_PRINT("RELIABLE OUT FULL AT %d %s\n", out->sequence % CUTE_NET_SEQUENCE_BUFFER_SIZE, transport->debug_name);
		return 0;
	}
	cn_reliable_data_t* reliable = (cn_reliable_data_t*)cn_insert_sequence(out, out->sequence);
	//CUTE_NET_DEBUG_PRINT("INC RELIABLE OUT %d %s\n", out->sequence % CUTE_NET_SEQUENCE_BUFFER_SIZE, transport->debug_name);
	reliable->user_type = user_type;
	memcpy(reliable->data, data, table->runtime_size);
	return 1;
}

int cn_get_reliable(cn_transport_t* transport, int* user_type, void* data)
{
	cn_sequence_buffer_t* incoming = &transport->reliable_incoming;
	uint16_t sequence = transport->reliable_next_incoming;
	cn_reliable_data_t* reliable = (cn_reliable_data_t*)cn_get_sequence_data(incoming, sequence);
	if (!reliable) return 0;
	int type = reliable->user_type;
	cn_vtable_t* table = cn_get_table(transport->ctx, type);
	memcpy(data, reliable->data, table->runtime_size);
	cn_sequence_remove(incoming, sequence);
	//CUTE_NET_DEBUG_PRINT("REMOVE INCOMING %d\t%s\n", sequence % CUTE_NET_SEQUENCE_BUFFER_SIZE, transport->debug_name);
	transport->reliable_next_incoming = sequence + 1;
	return 1;
}

int cn_do_work(cn_transport_t* transport)
{
	// grab packets
	cn_address_t from;
	uint32_t words[CUTE_NET_MTU_WORDCOUNT];
	int recieved_bytes = cn_peak_internal(transport, &from, words);
	if (recieved_bytes)
	{
		int64_t ticks = cn_ticks(transport);
		cn_lock(transport);
		int succeeded = cn_push(transport->q, words, recieved_bytes, from, ticks);
		cn_unlock(transport);
		printf("got time %d\n", (int)ticks);
		if (succeeded) return 1;
		else
		{
			// TODO
			// warning, we dropped a packet
			return 0;
		}
	}

	cn_lock(transport);
	int did_work = cn_process(transport->q);
	cn_unlock(transport);
	return did_work;
}

#define CUTE_NET_H
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
