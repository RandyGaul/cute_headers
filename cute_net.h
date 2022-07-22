/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_net.h - v1.00

	To create implementation (the function definitions)
		#define CUTE_NET_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file


	SUMMARY

		Cute net provides you a way to securely connect clients to servers, and
		implements all the machinery for sending both reliable-in-order and
		fire-and-forget (UDP style) packets. This is great for many kinds of games,
		ranging from games who just want TCP-style packets (such as a turn-based
		RPG), to fast paced platformer or fighting games.


	FEATURES

		* Reliable-in-order packets. These are packets that will be resent if they
		  aren't received, and will be received in the same order they are sent.
		* Fire-and-forget packets. These are basic UDP packets, useful for sending
		  temporal info like transforms for quick updates. They are not resent if
		  lost, and can arrive out of order.
		* Client and server abstractions.
		* State of the art connect tokens for security.
		* Support for large packets (fragmentation and reassembly).
		* (TODO - finish this, low priority, buggy) Bandwidth stats + throttle.


	CROSS PLATFORM

		Works on almost any platform. The main limitation is initialization of
		cryptographically secure random numbers, as implemented by libhydrogen.
		But, it should work pretty much everywhere. You can find the platforms
		supported via ctrl + f "#error Unsupported platform (libhydrogen)." without
		quotes. The platforms APIs are (yes, this covers Mac, iOS, Windows, Android
		and Linux):

			* riot
			* mbedtls
			* unix
			* wasi
			* windows
			* NRF
			* particle
			* ESP
			* AVR

		Please note emscripten (and browsers) are not supported, as Cute Net uses
		the UDP transport layer, not TCP.


	LIMITATIONS

		Cute net does not implement some high level multiplayer features, such as
		interpolation, rollback, or other similar features. Cute net is merely to
		setup secure connections between clients and servers, and facilitate
		packets through the connection.

		This is a pretty big library with a lot of code. It's doing a lot under the
		hood, including cryptography, book-keeping, and dealing with the UDP layer.
		However, it's very small for what it's doing!

		There is no HTTPS support, so distributing connect tokens is *out of scope*
		of this library. If you want an out-of-the-box solution for making HTTPS
		calls to a game server, you can try out the higher level library Cute
		Framework (https://github.com/RandyGaul/cute_framework).


	SPECIAL THANKS

		Special thanks to Glenn Fiedler for his online resources, especially his
		netcode.io (https://github.com/networkprotocol/netcode).

		Special thanks to Frank Denis for providing the cryptography AEAD primitive
		within his libhydrogen (https://github.com/jedisct1/libhydrogen).

		Special thanks to Mattias Gustavsson for his rnd and hashtable implementations
		(https://github.com/mattiasgustavsson/libs).


	Revision history:
		1.00 (04/22/2022) initial release
		1.01 (07/22/2022) fixed an old find + replace bug that caused packets to fail
		                  decryption across different platforms/compilers
*/

/*
	DOCUMENTATION

		Clients use connect tokens to connect to game servers. This only allows clients
		who authenticate themselves to connect and play on your game servers, granting
		completel control over who can or cannot play. This is important as dedicated
		game servers are typically fairly expensive to run, and usually only players who
		have paid for the game are able to obtain connect tokens.

		You will have to distribute connect tokens to clients. The recommendation is to
		setup a web service to provide a REST API, like a simple HTTP server. The client
		can send an HTTP request, and the server responds with a connect token.

		The client then reads the connect token, which contains a list of game servers
		to try and connect to along with other needed security info. Here's a diagram
		describing the process.

			+-----------+
			|    Web    |
			|  Service  |
			+-----------+
			    ^  |
			    |  |                            +-----------+              +-----------+
			  REST Call                         | Dedicated |              | Dedicated |
			  returns a                         | Server  1 |              | Server  2 |
			Connect Token                       +-----------+              +-----------+
			    |  |                                  ^                          ^
			    |  v                                  |                          |
			 +--------+   *connect token packet* ->   |   if fail, try next ->   |
			 | Client |-------------------------------+--------------------------+----------> ... Token timeout!
			 +--------+

		Once you get a connect token make a call to `cn_client_connect`.

		The function `cn_generate_connect_token` can be used to generate connect tokens for the web service to distribute.

		The web service distributes connect tokens. Cute net does not provide an implementation
		of a web service because there are many different good solutions out there already. The
		goal is to only respond and provide connect tokens to clients who have authenticated
		themselves over a secure connection with the web service, such as through HTTPS. For
		example: this is how cute net can be used to filter out players who have no purchased the
		game or signed up with an account.


	EXAMPLES
	
		Here is a full example of a client and server, where the client sends a string to the
		server to print to the console.

			Client example: https://github.com/RandyGaul/cute_headers/tree/master/examples_cute_net/client
			Server example: https://github.com/RandyGaul/cute_headers/tree/master/examples_cute_net/server


	UNIT TESTS

		You may enable building the unit tests with CUTE_NET_TESTS. Simply call

			cn_run_tests(-1, false);

		to run all of the unit tests. The first parameter is the test number to run,
		while the second paramater is for a "soak" test (run the tests in an in-
		finite loop).

		You will see a bunch of colored text printed to the console about each test
		and whether or not each test passed or failed.


	CUSTOMIZATION

		There are a number of macros to override c-runtime functions. Simply define
		your own version of these macros before including this header when you setup
		the implementation with CUTE_NET_IMPLEMENTATION.

			CN_ALLOC
			CN_FREE
			CN_MEMCPY
			CN_MEMSET
			CN_ASSERT
			CN_STRNCPY
			CN_STRLEN
			CN_STRNCMP
			CN_MEMCMP
			CN_SNPRINTF
			CN_FPRINTF

		You can disable IPv6 support by defining CUTE_NET_NO_IPV6 like so.

			#define CUTE_NET_NO_IPV6
			#define CUTE_NET_IMPLEMENTATION
			#include <cute_net.h>

		You may override the number of max clients on a server via CN_SERVER_MAX_CLIENTS.
		The memory per client is a constant factor, and not too much. From rough back of the
		envelope math it was estimated to easily support 2k+ players on a single machine.


	BUGS AND CRASHES

		This header is quite new and it takes time to test all code; There may be bugs
		here and there, so please open up an issue on github if you have any questions
		or need help!

		https://github.com/RandyGaul/cute_headers/issues
*/

#ifndef CN_NET_H
#define CN_NET_H

#ifndef _CRT_SECURE_NO_WARNINGS
	#define _CRT_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_NONSTDC_NO_DEPRECATE
	#define _CRT_NONSTDC_NO_DEPRECATE
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

typedef struct cn_error_t cn_error_t;
typedef struct cn_client_t cn_client_t;
typedef struct cn_server_t cn_server_t;
typedef struct cn_endpoint_t cn_endpoint_t;
typedef struct cn_crypto_key_t { uint8_t key[32]; } cn_crypto_key_t;
typedef struct cn_crypto_sign_public_t { uint8_t key[32]; } cn_crypto_sign_public_t;
typedef struct cn_crypto_sign_secret_t { uint8_t key[64]; } cn_crypto_sign_secret_t;
typedef struct cn_crypto_signature_t { uint8_t bytes[64]; } cn_crypto_signature_t;

#ifndef CN_INLINE 
#	define CN_INLINE inline
#endif

//--------------------------------------------------------------------------------------------------
// ENDPOINT

typedef enum cn_address_type_t
{
	CN_ADDRESS_TYPE_NONE,
	CN_ADDRESS_TYPE_IPV4,
#ifndef CUTE_NET_NO_IPV6
	CN_ADDRESS_TYPE_IPV6
#endif
} cn_address_type_t;

struct cn_endpoint_t
{
	cn_address_type_t type;
	uint16_t port;

	union
	{
		uint8_t ipv4[4];
#ifndef CUTE_NET_NO_IPV6
		uint16_t ipv6[8];
#endif
	} u;
};

int cn_endpoint_init(cn_endpoint_t* endpoint, const char* address_and_port_string);
void cn_endpoint_to_string(cn_endpoint_t endpoint, char* buffer, int buffer_size);
int cn_endpoint_equals(cn_endpoint_t a, cn_endpoint_t b);

//--------------------------------------------------------------------------------------------------
// CONNECT TOKEN

#define CN_CONNECT_TOKEN_SIZE 1114
#define CN_CONNECT_TOKEN_USER_DATA_SIZE 256

/**
 * Generates a cryptography key in a cryptographically secure way.
 */
cn_crypto_key_t cn_crypto_generate_key();

/**
 * Fills a buffer in a cryptographically secure way (i.e. a slow way).
 */
void cn_crypto_random_bytes(void* data, int byte_count);

/**
 * Generates a cryptographically secure keypair, used for facilitating connect tokens.
 */
void cn_crypto_sign_keygen(cn_crypto_sign_public_t* public_key, cn_crypto_sign_secret_t* secret_key);

/**
 * Generates a connect token, useable by clients to authenticate and securely connect to
 * a server. You can use this function whenever a validated client wants to join your game servers.
 * 
 * It's recommended to setup a web service specifically for allowing players to authenticate
 * themselves (login). Once authenticated, the webservice can call this function and hand
 * the connect token to the client. The client can then read the public section of the
 * connect token and see the `address_list` of servers to try and connect to. The client then
 * sends the connect token to one of these servers to start the connection handshake. If the
 * handshake completes successfully, the client will connect to the server.
 *
 * The connect token is protected by an AEAD primitive (https://en.wikipedia.org/wiki/Authenticated_encryption),
 * which means the token cannot be modified or forged as long as the `shared_secret_key` is
 * not leaked. In the event your secret key is accidentally leaked, you can always roll a
 * new one and distribute it to your webservice and game servers.
 */
cn_error_t cn_generate_connect_token(
	uint64_t application_id,                          // A unique number to identify your game, can be whatever value you like.
	                                                  // This must be the same number as in `cn_client_create` and `cn_server_create`.
	uint64_t creation_timestamp,                      // A unix timestamp of the current time.
	const cn_crypto_key_t* client_to_server_key,      // A unique key for this connect token for the client to encrypt packets, and server to
	                                                  // decrypt packets. This can be generated with `cn_crypto_generate_key` on your web service.
	const cn_crypto_key_t* server_to_client_key,      // A unique key for this connect token for the server to encrypt packets, and the client to
	                                                  // decrypt packets. This can be generated with `cn_crypto_generate_key` on your web service.
	uint64_t expiration_timestamp,                    // A unix timestamp for when this connect token expires and becomes invalid.
	uint32_t handshake_timeout,                       // The number of seconds the connection will stay alive during the handshake process before
	                                                  // the client and server reject the handshake process as failed.
	int address_count,                                // Must be from 1 to 32 (inclusive). The number of addresses in `address_list`.
	const char** address_list,                        // A list of game servers the client can try connecting to, of length `address_count`.
	uint64_t client_id,                               // The unique client identifier.
	const uint8_t* user_data,                         // Optional buffer of data of `CN_CONNECT_TOKEN_USER_DATA_SIZE` (256) bytes. Can be NULL.
	const cn_crypto_sign_secret_t* shared_secret_key, // Only your webservice and game servers know this key.
	uint8_t* token_ptr_out                            // Pointer to your buffer, should be `CN_CONNECT_TOKEN_SIZE` bytes large.
);

//--------------------------------------------------------------------------------------------------
// CLIENT

cn_client_t* cn_client_create(
	uint16_t port,                            // Port for opening a UDP socket.
	uint64_t application_id,                  // A unique number to identify your game, can be whatever value you like.
	                                          // This must be the same number as in `cn_server_create`.
	bool use_ipv6 /* = false */,              // Whether or not the socket should turn on ipv6. Some users will not have
	                                          // ipv6 enabled, so this defaults to false.
	void* user_allocator_context /* = NULL */ // Used for custom allocators, this can be set to NULL.
);
void cn_client_destroy(cn_client_t* client);

/**
 * The client will make an attempt to connect to all servers listed in the connect token, one after
 * another. If no server can be connected to the client's state will be set to an error state. Call
 * `cn_client_state_get` to get the client's state. Once `cn_client_connect` is called then successive calls to
 * `cn_client_update` is expected, where `cn_client_update` will perform the connection handshake and make
 * connection attempts to your servers.
 */
cn_error_t cn_client_connect(cn_client_t* client, const uint8_t* connect_token);
void cn_client_disconnect(cn_client_t* client);

/**
 * You should call this one per game loop after calling `cn_client_connect`.
 */
void cn_client_update(cn_client_t* client, double dt, uint64_t current_time);

/**
 * Returns a packet from the server if one is available. You must free this packet when you're done by
 * calling `cn_client_free_packet`.
 */
bool cn_client_pop_packet(cn_client_t* client, void** packet, int* size, bool* was_sent_reliably /* = NULL */);
void cn_client_free_packet(cn_client_t* client, void* packet);

/**
 * Sends a packet to the server. If the packet size is too large (over 1k bytes) it will be split up
 * and sent in smaller chunks.
 * 
 * `send_reliably` as true means the packet will be sent reliably an in-order relative to other
 * reliable packets. Under packet loss the packet will continually be sent until an acknowledgement
 * from the server is received. False means to send a typical UDP packet, with no special mechanisms
 * regarding packet loss.
 * 
 * Reliable packets are significantly more expensive than unreliable packets, so try to send any data
 * that can be lost due to packet loss as an unreliable packet. Of course, some packets are required
 * to be sent, and so reliable is appropriate. As an optimization some kinds of data, such as frequent
 * transform updates, can be sent unreliably.
 */
cn_error_t cn_client_send(cn_client_t* client, const void* packet, int size, bool send_reliably);

typedef enum cn_client_state_t
{
	CN_CLIENT_STATE_CONNECT_TOKEN_EXPIRED         = -6,
	CN_CLIENT_STATE_INVALID_CONNECT_TOKEN         = -5,
	CN_CLIENT_STATE_CONNECTION_TIMED_OUT          = -4,
	CN_CLIENT_STATE_CHALLENGE_RESPONSE_TIMED_OUT  = -3,
	CN_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT  = -2,
	CN_CLIENT_STATE_CONNECTION_DENIED             = -1,
	CN_CLIENT_STATE_DISCONNECTED                  = 0,
	CN_CLIENT_STATE_SENDING_CONNECTION_REQUEST    = 1,
	CN_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE    = 2,
	CN_CLIENT_STATE_CONNECTED                     = 3,
} cn_client_state_t;

cn_client_state_t cn_client_state_get(const cn_client_t* client);
const char* cn_client_state_string(cn_client_state_t state); 
float cn_client_time_of_last_packet_recieved(const cn_client_t* client);
void cn_client_enable_network_simulator(cn_client_t* client, double latency, double jitter, double drop_chance, double duplicate_chance);

//--------------------------------------------------------------------------------------------------
// SERVER

// Override this macro as seen fit.
#ifndef CN_SERVER_MAX_CLIENTS
#	define CN_SERVER_MAX_CLIENTS 32
#endif

typedef struct cn_server_config_t
{
	uint64_t application_id;            // A unique number to identify your game, can be whatever value you like.
	                                    // This must be the same number as in `cn_client_make`.
	int max_incoming_bytes_per_second;
	int max_outgoing_bytes_per_second;
	int connection_timeout;             // The number of seconds before consider a connection as timed out when not
	                                    // receiving any packets on the connection.
	double resend_rate;                 // The number of seconds to wait before resending a packet that has not been
	                                    // acknowledge as received by a client.
	cn_crypto_sign_public_t public_key; // The public part of your public key cryptography used for connect tokens.
	                                    // This can be safely shared with your players publicly.
	cn_crypto_sign_secret_t secret_key; // The secret part of your public key cryptography used for connect tokens.
	                                    // This must never be shared publicly and remain a complete secret only know to your servers.
	void* user_allocator_context;
} cn_server_config_t;

CN_INLINE cn_server_config_t cn_server_config_defaults(void)
{
	cn_server_config_t config;
	config.application_id = 0;
	config.max_incoming_bytes_per_second = 0;
	config.max_outgoing_bytes_per_second = 0;
	config.connection_timeout = 10;
	config.resend_rate = 0.1f;
	return config;
}

cn_server_t* cn_server_create(cn_server_config_t config);
void cn_server_destroy(cn_server_t* server);

/**
 * Starts up the server, ready to receive new client connections.
 * 
 * Please note that not all users will be able to access an ipv6 server address, so it might
 * be good to also provide a way to connect through ipv4.
 */
cn_error_t cn_server_start(cn_server_t* server, const char* address_and_port);
void cn_server_stop(cn_server_t* server);

typedef enum cn_server_event_type_t
{
	CN_SERVER_EVENT_TYPE_NEW_CONNECTION, // A new incoming connection.
	CN_SERVER_EVENT_TYPE_DISCONNECTED,   // A disconnecting client.
	CN_SERVER_EVENT_TYPE_PAYLOAD_PACKET, // An incoming packet from a client.
} cn_server_event_type_t;

typedef struct cn_server_event_t
{
	cn_server_event_type_t type;
	union
	{
		struct
		{
			int client_index;       // An index representing this particular client.
			uint64_t client_id;     // A unique identifier for this particular client, as read from the connect token.
			cn_endpoint_t endpoint; // The address and port of the incoming connection.
		} new_connection;

		struct
		{
			int client_index;       // An index representing this particular client.
		} disconnected;

		struct
		{
			int client_index;       // An index representing this particular client.
			void* data;             // Pointer to the packet's payload data. Send this back to `cn_server_free_packet` when done.
			int size;               // Size of the packet at the data pointer.
		} payload_packet;
	} u;
} cn_server_event_t;

/**
 * Server events notify of when a client connects/disconnects, or has sent a payload packet.
 * You must free the payload packets with `cn_server_free_packet` when done.
 */
bool cn_server_pop_event(cn_server_t* server, cn_server_event_t* event);
void cn_server_free_packet(cn_server_t* server, int client_index, void* data);

void cn_server_update(cn_server_t* server, double dt, uint64_t current_time);
void cn_server_disconnect_client(cn_server_t* server, int client_index, bool notify_client /* = true */);
void cn_server_send(cn_server_t* server, const void* packet, int size, int client_index, bool send_reliably);
void cn_server_send_to_all_clients(cn_server_t* server, const void* packet, int size, bool send_reliably);
void cn_server_send_to_all_but_one_client(cn_server_t* server, const void* packet, int size, int client_index, bool send_reliably);

bool cn_server_is_client_connected(cn_server_t* server, int client_index);
void cn_server_enable_network_simulator(cn_server_t* server, double latency, double jitter, double drop_chance, double duplicate_chance);

//--------------------------------------------------------------------------------------------------
// ERROR

#define CN_ERROR_SUCCESS (0)
#define CN_ERROR_FAILURE (-1)

struct cn_error_t
{
	int code;
	const char* details;
};

CN_INLINE bool cn_is_error(cn_error_t err) { return err.code == CN_ERROR_FAILURE; }
CN_INLINE cn_error_t cn_error_failure(const char* details) { cn_error_t error; error.code = CN_ERROR_FAILURE; error.details = details; return error; }
CN_INLINE cn_error_t cn_error_success(void) { cn_error_t error; error.code = CN_ERROR_SUCCESS; error.details = NULL; return error; }
#define CN_RETURN_IF_ERROR(x) do { cn_error_t err = (x); if (cn_is_error(err)) return err; } while (0)

#endif // CN_NET_H

#ifndef CN_NET_INTERNAL_H
#define CN_NET_INTERNAL_H

#define CN_CRYPTO_HEADER_BYTES ((int)(20 + 16))
#define CN_KB 1024
#define CN_MB (CN_KB * CN_KB)

#endif // CN_NET_INTERNAL_H

#ifdef CUTE_NET_IMPLEMENTATION
#ifndef CUTE_NET_IMPLEMENTATION_ONCE
#define CUTE_NET_IMPLEMENTATION_ONCE

#if defined(_WIN32)
#	define CN_WINDOWS 1
#elif defined(__linux__) || defined(__unix__) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
#	define CN_LINUX 1
#elif defined(__APPLE__)
#	include <TargetConditionals.h>
#	if TARGET_IPHONE_SIMULATOR || TARGET_OS_IPHONE
#		define CN_IOS 1
#	elif TARGET_OS_MAC
#		define CN_MACOSX 1
#	else
#		error "Unknown Apple platform"
#	endif
#elif defined(__ANDROID__)
#	define CN_ANDROID 1
#elif defined(__EMSCRIPTEN__)
#	error Emscripten is not supported, as Cute Net uses the UDP transport layer.
#endif

#if !defined(CN_ALLOC)
	#include <stdlib.h>
	#define CN_ALLOC(size, ctx) malloc(size)
	#define CN_FREE(mem, ctx) free(mem)
#endif

#if !defined(CN_MEMCPY)
	#include <string.h>
	#define CN_MEMCPY memcpy
#endif

#if !defined(CN_MEMSET)
	#include <string.h>
	#define CN_MEMSET memset
#endif

#if !defined(CN_ASSERT)
	#include <assert.h>
	#define CN_ASSERT assert
#endif

#ifndef CN_STRNCPY
#	include <string.h>
#	define CN_STRNCPY strncpy
#endif

#ifndef CN_STRLEN
#	include <string.h>
#	define CN_STRLEN strlen
#endif

#ifndef CN_STRNCMP
#	include <string.h>
#	define CN_STRNCMP strncmp
#endif

#ifndef CN_MEMCMP
#	include <string.h>
#	define CN_MEMCMP memcmp
#endif

#ifndef CN_SNPRINTF
#	include <stdio.h>
#	define CN_SNPRINTF snprintf
#endif

#ifndef CN_FPRINTF
#	include <stdio.h>
#	define CN_FPRINTF fprintf
#endif

//--------------------------------------------------------------------------------------------------
// Embedded libhydrogen packed up from a python script.
// See https://github.com/jedisct1/libhydrogen for the original source.

#ifndef hydrogen_H
#define hydrogen_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef __cplusplus
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wlong-long"
#endif
extern "C" {
#endif

#if defined(__clang__) || defined(__GNUC__)
#define _hydro_attr_(X) __attribute__(X)
#else
#define _hydro_attr_(X)
#endif
#define _hydro_attr_deprecated_ _hydro_attr_((deprecated))
#define _hydro_attr_malloc_ _hydro_attr_((malloc))
#define _hydro_attr_noinline_ _hydro_attr_((noinline))
#define _hydro_attr_noreturn_ _hydro_attr_((noreturn))
#define _hydro_attr_warn_unused_result_ _hydro_attr_((warn_unused_result))
#define _hydro_attr_weak_ _hydro_attr_((weak))

#if defined(__INTEL_COMPILER) || defined(_MSC_VER)
#define _hydro_attr_aligned_(X) __declspec(align(X))
#elif defined(__clang__) || defined(__GNUC__)
#define _hydro_attr_aligned_(X) _hydro_attr_((aligned(X)))
#else
#define _hydro_attr_aligned_(X)
#endif

#define HYDRO_VERSION_MAJOR 1
#define HYDRO_VERSION_MINOR 0

int hydro_init(void);

/* ---------------- */

#define hydro_random_SEEDBYTES 32

uint32_t hydro_random_u32(void);

uint32_t hydro_random_uniform(const uint32_t upper_bound);

void hydro_random_buf(void *out, size_t out_len);

void hydro_random_buf_deterministic(void *out, size_t out_len,
									const uint8_t seed[hydro_random_SEEDBYTES]);

void hydro_random_ratchet(void);

void hydro_random_reseed(void);

/* ---------------- */

#define hydro_hash_BYTES 32
#define hydro_hash_BYTES_MAX 65535
#define hydro_hash_BYTES_MIN 16
#define hydro_hash_CONTEXTBYTES 8
#define hydro_hash_KEYBYTES 32

typedef struct hydro_hash_state {
	uint32_t state[12];
	uint8_t  buf_off;
	uint8_t  align[3];
} hydro_hash_state;

void hydro_hash_keygen(uint8_t key[hydro_hash_KEYBYTES]);

int hydro_hash_init(hydro_hash_state *state, const char ctx[hydro_hash_CONTEXTBYTES],
					const uint8_t key[hydro_hash_KEYBYTES]);

int hydro_hash_update(hydro_hash_state *state, const void *in_, size_t in_len);

int hydro_hash_final(hydro_hash_state *state, uint8_t *out, size_t out_len);

int hydro_hash_hash(uint8_t *out, size_t out_len, const void *in_, size_t in_len,
					const char    ctx[hydro_hash_CONTEXTBYTES],
					const uint8_t key[hydro_hash_KEYBYTES]);

/* ---------------- */

#define hydro_secretbox_CONTEXTBYTES 8
#define hydro_secretbox_HEADERBYTES (20 + 16)
#define hydro_secretbox_KEYBYTES 32
#define hydro_secretbox_PROBEBYTES 16

void hydro_secretbox_keygen(uint8_t key[hydro_secretbox_KEYBYTES]);

int hydro_secretbox_encrypt(uint8_t *c, const void *m_, size_t mlen, uint64_t msg_id,
							const char    ctx[hydro_secretbox_CONTEXTBYTES],
							const uint8_t key[hydro_secretbox_KEYBYTES]);

int hydro_secretbox_decrypt(void *m_, const uint8_t *c, size_t clen, uint64_t msg_id,
							const char    ctx[hydro_secretbox_CONTEXTBYTES],
							const uint8_t key[hydro_secretbox_KEYBYTES])
	_hydro_attr_warn_unused_result_;

void hydro_secretbox_probe_create(uint8_t probe[hydro_secretbox_PROBEBYTES], const uint8_t *c,
								  size_t c_len, const char ctx[hydro_secretbox_CONTEXTBYTES],
								  const uint8_t key[hydro_secretbox_KEYBYTES]);

int hydro_secretbox_probe_verify(const uint8_t probe[hydro_secretbox_PROBEBYTES], const uint8_t *c,
								 size_t c_len, const char ctx[hydro_secretbox_CONTEXTBYTES],
								 const uint8_t key[hydro_secretbox_KEYBYTES])
	_hydro_attr_warn_unused_result_;

/* ---------------- */

#define hydro_kdf_CONTEXTBYTES 8
#define hydro_kdf_KEYBYTES 32
#define hydro_kdf_BYTES_MAX 65535
#define hydro_kdf_BYTES_MIN 16

void hydro_kdf_keygen(uint8_t key[hydro_kdf_KEYBYTES]);

int hydro_kdf_derive_from_key(uint8_t *subkey, size_t subkey_len, uint64_t subkey_id,
							  const char    ctx[hydro_kdf_CONTEXTBYTES],
							  const uint8_t key[hydro_kdf_KEYBYTES]);

/* ---------------- */

#define hydro_sign_BYTES 64
#define hydro_sign_CONTEXTBYTES 8
#define hydro_sign_PUBLICKEYBYTES 32
#define hydro_sign_SECRETKEYBYTES 64
#define hydro_sign_SEEDBYTES 32

typedef struct hydro_sign_state {
	hydro_hash_state hash_st;
} hydro_sign_state;

typedef struct hydro_sign_keypair {
	uint8_t pk[hydro_sign_PUBLICKEYBYTES];
	uint8_t sk[hydro_sign_SECRETKEYBYTES];
} hydro_sign_keypair;

void hydro_sign_keygen(hydro_sign_keypair *kp);

void hydro_sign_keygen_deterministic(hydro_sign_keypair *kp,
									 const uint8_t       seed[hydro_sign_SEEDBYTES]);

int hydro_sign_init(hydro_sign_state *state, const char ctx[hydro_sign_CONTEXTBYTES]);

int hydro_sign_update(hydro_sign_state *state, const void *m_, size_t mlen);

int hydro_sign_final_create(hydro_sign_state *state, uint8_t csig[hydro_sign_BYTES],
							const uint8_t sk[hydro_sign_SECRETKEYBYTES]);

int hydro_sign_final_verify(hydro_sign_state *state, const uint8_t csig[hydro_sign_BYTES],
							const uint8_t pk[hydro_sign_PUBLICKEYBYTES])
	_hydro_attr_warn_unused_result_;

int hydro_sign_create(uint8_t csig[hydro_sign_BYTES], const void *m_, size_t mlen,
					  const char    ctx[hydro_sign_CONTEXTBYTES],
					  const uint8_t sk[hydro_sign_SECRETKEYBYTES]);

int hydro_sign_verify(const uint8_t csig[hydro_sign_BYTES], const void *m_, size_t mlen,
					  const char    ctx[hydro_sign_CONTEXTBYTES],
					  const uint8_t pk[hydro_sign_PUBLICKEYBYTES]) _hydro_attr_warn_unused_result_;

/* ---------------- */

#define hydro_kx_SESSIONKEYBYTES 32
#define hydro_kx_PUBLICKEYBYTES 32
#define hydro_kx_SECRETKEYBYTES 32
#define hydro_kx_PSKBYTES 32
#define hydro_kx_SEEDBYTES 32

typedef struct hydro_kx_keypair {
	uint8_t pk[hydro_kx_PUBLICKEYBYTES];
	uint8_t sk[hydro_kx_SECRETKEYBYTES];
} hydro_kx_keypair;

typedef struct hydro_kx_session_keypair {
	uint8_t rx[hydro_kx_SESSIONKEYBYTES];
	uint8_t tx[hydro_kx_SESSIONKEYBYTES];
} hydro_kx_session_keypair;

typedef struct hydro_kx_state {
	hydro_kx_keypair eph_kp;
	hydro_hash_state h_st;
} hydro_kx_state;

void hydro_kx_keygen(hydro_kx_keypair *static_kp);

void hydro_kx_keygen_deterministic(hydro_kx_keypair *static_kp,
								   const uint8_t     seed[hydro_kx_SEEDBYTES]);

/* NOISE_N */

#define hydro_kx_N_PACKET1BYTES (32 + 16)

int hydro_kx_n_1(hydro_kx_session_keypair *kp, uint8_t packet1[hydro_kx_N_PACKET1BYTES],
				 const uint8_t psk[hydro_kx_PSKBYTES],
				 const uint8_t peer_static_pk[hydro_kx_PUBLICKEYBYTES]);

int hydro_kx_n_2(hydro_kx_session_keypair *kp, const uint8_t packet1[hydro_kx_N_PACKET1BYTES],
				 const uint8_t psk[hydro_kx_PSKBYTES], const hydro_kx_keypair *static_kp);

/* NOISE_KK */

#define hydro_kx_KK_PACKET1BYTES (32 + 16)
#define hydro_kx_KK_PACKET2BYTES (32 + 16)

int hydro_kx_kk_1(hydro_kx_state *state, uint8_t packet1[hydro_kx_KK_PACKET1BYTES],
				  const uint8_t           peer_static_pk[hydro_kx_PUBLICKEYBYTES],
				  const hydro_kx_keypair *static_kp);

int hydro_kx_kk_2(hydro_kx_session_keypair *kp, uint8_t packet2[hydro_kx_KK_PACKET2BYTES],
				  const uint8_t           packet1[hydro_kx_KK_PACKET1BYTES],
				  const uint8_t           peer_static_pk[hydro_kx_PUBLICKEYBYTES],
				  const hydro_kx_keypair *static_kp);

int hydro_kx_kk_3(hydro_kx_state *state, hydro_kx_session_keypair *kp,
				  const uint8_t           packet2[hydro_kx_KK_PACKET2BYTES],
				  const hydro_kx_keypair *static_kp);

/* NOISE_XX */

#define hydro_kx_XX_PACKET1BYTES (32 + 16)
#define hydro_kx_XX_PACKET2BYTES (32 + 32 + 16 + 16)
#define hydro_kx_XX_PACKET3BYTES (32 + 16 + 16)

int hydro_kx_xx_1(hydro_kx_state *state, uint8_t packet1[hydro_kx_XX_PACKET1BYTES],
				  const uint8_t psk[hydro_kx_PSKBYTES]);

int hydro_kx_xx_2(hydro_kx_state *state, uint8_t packet2[hydro_kx_XX_PACKET2BYTES],
				  const uint8_t packet1[hydro_kx_XX_PACKET1BYTES],
				  const uint8_t psk[hydro_kx_PSKBYTES], const hydro_kx_keypair *static_kp);

int hydro_kx_xx_3(hydro_kx_state *state, hydro_kx_session_keypair *kp,
				  uint8_t       packet3[hydro_kx_XX_PACKET3BYTES],
				  uint8_t       peer_static_pk[hydro_kx_PUBLICKEYBYTES],
				  const uint8_t packet2[hydro_kx_XX_PACKET2BYTES],
				  const uint8_t psk[hydro_kx_PSKBYTES], const hydro_kx_keypair *static_kp);

int hydro_kx_xx_4(hydro_kx_state *state, hydro_kx_session_keypair *kp,
				  uint8_t       peer_static_pk[hydro_kx_PUBLICKEYBYTES],
				  const uint8_t packet3[hydro_kx_XX_PACKET3BYTES],
				  const uint8_t psk[hydro_kx_PSKBYTES]);

/* NOISE_NK */

#define hydro_kx_NK_PACKET1BYTES (32 + 16)
#define hydro_kx_NK_PACKET2BYTES (32 + 16)

int hydro_kx_nk_1(hydro_kx_state *state, uint8_t packet1[hydro_kx_NK_PACKET1BYTES],
				  const uint8_t psk[hydro_kx_PSKBYTES],
				  const uint8_t peer_static_pk[hydro_kx_PUBLICKEYBYTES]);

int hydro_kx_nk_2(hydro_kx_session_keypair *kp, uint8_t packet2[hydro_kx_NK_PACKET2BYTES],
				  const uint8_t packet1[hydro_kx_NK_PACKET1BYTES],
				  const uint8_t psk[hydro_kx_PSKBYTES], const hydro_kx_keypair *static_kp);

int hydro_kx_nk_3(hydro_kx_state *state, hydro_kx_session_keypair *kp,
				  const uint8_t packet2[hydro_kx_NK_PACKET2BYTES]);

/* ---------------- */

#define hydro_pwhash_CONTEXTBYTES 8
#define hydro_pwhash_MASTERKEYBYTES 32
#define hydro_pwhash_STOREDBYTES 128

void hydro_pwhash_keygen(uint8_t master_key[hydro_pwhash_MASTERKEYBYTES]);

int hydro_pwhash_deterministic(uint8_t *h, size_t h_len, const char *passwd, size_t passwd_len,
							   const char    ctx[hydro_pwhash_CONTEXTBYTES],
							   const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES],
							   uint64_t opslimit, size_t memlimit, uint8_t threads);

int hydro_pwhash_create(uint8_t stored[hydro_pwhash_STOREDBYTES], const char *passwd,
						size_t passwd_len, const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES],
						uint64_t opslimit, size_t memlimit, uint8_t threads);

int hydro_pwhash_verify(const uint8_t stored[hydro_pwhash_STOREDBYTES], const char *passwd,
						size_t passwd_len, const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES],
						uint64_t opslimit_max, size_t memlimit_max, uint8_t threads_max);

int hydro_pwhash_derive_static_key(uint8_t *static_key, size_t static_key_len,
								   const uint8_t stored[hydro_pwhash_STOREDBYTES],
								   const char *passwd, size_t passwd_len,
								   const char    ctx[hydro_pwhash_CONTEXTBYTES],
								   const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES],
								   uint64_t opslimit_max, size_t memlimit_max, uint8_t threads_max);

int hydro_pwhash_reencrypt(uint8_t       stored[hydro_pwhash_STOREDBYTES],
						   const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES],
						   const uint8_t new_master_key[hydro_pwhash_MASTERKEYBYTES]);

int hydro_pwhash_upgrade(uint8_t       stored[hydro_pwhash_STOREDBYTES],
						 const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES], uint64_t opslimit,
						 size_t memlimit, uint8_t threads);

/* ---------------- */

void hydro_memzero(void *pnt, size_t len);

void hydro_increment(uint8_t *n, size_t len);

bool hydro_equal(const void *b1_, const void *b2_, size_t len);

int hydro_compare(const uint8_t *b1_, const uint8_t *b2_, size_t len);

char *hydro_bin2hex(char *hex, size_t hex_maxlen, const uint8_t *bin, size_t bin_len);

int hydro_hex2bin(uint8_t *bin, size_t bin_maxlen, const char *hex, size_t hex_len,
				  const char *ignore, const char **hex_end_p);

int hydro_pad(unsigned char *buf, size_t unpadded_buflen, size_t blocksize, size_t max_buflen);

int hydro_unpad(const unsigned char *buf, size_t padded_buflen, size_t blocksize);

/* ---------------- */

#define HYDRO_HWTYPE_ATMEGA328 1

#ifndef HYDRO_HWTYPE
#ifdef __AVR__
#define HYDRO_HWTYPE HYDRO_HWTYPE_ATMEGA328
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(__unix__) && (defined(__APPLE__) || defined(__linux__))
#define __unix__ 1
#endif
#ifndef __GNUC__
#define __restrict__
#endif

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
	__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define NATIVE_BIG_ENDIAN
#endif
#ifndef NATIVE_BIG_ENDIAN
#ifndef NATIVE_LITTLE_ENDIAN
#define NATIVE_LITTLE_ENDIAN
#endif
#endif

#ifndef TLS
#if defined(_WIN32) && !defined(__GNUC__)
#define TLS __declspec(thread)
#elif (defined(__clang__) || defined(__GNUC__)) && defined(__unix__)
#define TLS __thread
#else
#define TLS
#endif
#endif

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t) -1)
#endif

#ifdef __OpenBSD__
#define HAVE_EXPLICIT_BZERO 1
#elif defined(__GLIBC__) && defined(__GLIBC_PREREQ) && defined(_GNU_SOURCE)
#if __GLIBC_PREREQ(2, 25)
#define HAVE_EXPLICIT_BZERO 1
#endif
#endif

#define COMPILER_ASSERT(X) (void) sizeof(char[(X) ? 1 : -1])

#define ROTL32(x, b) (uint32_t)(((x) << (b)) | ((x) >> (32 - (b))))
#define ROTL64(x, b) (uint64_t)(((x) << (b)) | ((x) >> (64 - (b))))
#define ROTR32(x, b) (uint32_t)(((x) >> (b)) | ((x) << (32 - (b))))
#define ROTR64(x, b) (uint64_t)(((x) >> (b)) | ((x) << (64 - (b))))

#if defined(_MSC_VER)
#	pragma warning(push)
#	pragma warning(disable : 4197)
#	pragma warning(disable : 4146)
#endif

#define LOAD64_LE(SRC) load64_le(SRC)
static inline uint64_t
load64_le(const uint8_t src[8])
{
#ifdef NATIVE_LITTLE_ENDIAN
	uint64_t w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	uint64_t w = (uint64_t) src[0];
	w |= (uint64_t) src[1] << 8;
	w |= (uint64_t) src[2] << 16;
	w |= (uint64_t) src[3] << 24;
	w |= (uint64_t) src[4] << 32;
	w |= (uint64_t) src[5] << 40;
	w |= (uint64_t) src[6] << 48;
	w |= (uint64_t) src[7] << 56;
	return w;
#endif
}

#define STORE64_LE(DST, W) store64_le((DST), (W))
static inline void
store64_le(uint8_t dst[8], uint64_t w)
{
#ifdef NATIVE_LITTLE_ENDIAN
	memcpy(dst, &w, sizeof w);
#else
	dst[0] = (uint8_t) w;
	w >>= 8;
	dst[1] = (uint8_t) w;
	w >>= 8;
	dst[2] = (uint8_t) w;
	w >>= 8;
	dst[3] = (uint8_t) w;
	w >>= 8;
	dst[4] = (uint8_t) w;
	w >>= 8;
	dst[5] = (uint8_t) w;
	w >>= 8;
	dst[6] = (uint8_t) w;
	w >>= 8;
	dst[7] = (uint8_t) w;
#endif
}

#define LOAD32_LE(SRC) load32_le(SRC)
static inline uint32_t
load32_le(const uint8_t src[4])
{
#ifdef NATIVE_LITTLE_ENDIAN
	uint32_t w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	uint32_t w = (uint32_t) src[0];
	w |= (uint32_t) src[1] << 8;
	w |= (uint32_t) src[2] << 16;
	w |= (uint32_t) src[3] << 24;
	return w;
#endif
}

#define STORE32_LE(DST, W) store32_le((DST), (W))
static inline void
store32_le(uint8_t dst[4], uint32_t w)
{
#ifdef NATIVE_LITTLE_ENDIAN
	memcpy(dst, &w, sizeof w);
#else
	dst[0] = (uint8_t) w;
	w >>= 8;
	dst[1] = (uint8_t) w;
	w >>= 8;
	dst[2] = (uint8_t) w;
	w >>= 8;
	dst[3] = (uint8_t) w;
#endif
}

#define LOAD16_LE(SRC) load16_le(SRC)
static inline uint16_t
load16_le(const uint8_t src[2])
{
#ifdef NATIVE_LITTLE_ENDIAN
	uint16_t w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	uint16_t w = (uint16_t) src[0];
	w |= (uint16_t) src[1] << 8;
	return w;
#endif
}

#define STORE16_LE(DST, W) store16_le((DST), (W))
static inline void
store16_le(uint8_t dst[2], uint16_t w)
{
#ifdef NATIVE_LITTLE_ENDIAN
	memcpy(dst, &w, sizeof w);
#else
	dst[0] = (uint8_t) w;
	w >>= 8;
	dst[1] = (uint8_t) w;
#endif
}

/* ----- */

#define LOAD64_BE(SRC) load64_be(SRC)
static inline uint64_t
load64_be(const uint8_t src[8])
{
#ifdef NATIVE_BIG_ENDIAN
	uint64_t w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	uint64_t w = (uint64_t) src[7];
	w |= (uint64_t) src[6] << 8;
	w |= (uint64_t) src[5] << 16;
	w |= (uint64_t) src[4] << 24;
	w |= (uint64_t) src[3] << 32;
	w |= (uint64_t) src[2] << 40;
	w |= (uint64_t) src[1] << 48;
	w |= (uint64_t) src[0] << 56;
	return w;
#endif
}

#define STORE64_BE(DST, W) store64_be((DST), (W))
static inline void
store64_be(uint8_t dst[8], uint64_t w)
{
#ifdef NATIVE_BIG_ENDIAN
	memcpy(dst, &w, sizeof w);
#else
	dst[7] = (uint8_t) w;
	w >>= 8;
	dst[6] = (uint8_t) w;
	w >>= 8;
	dst[5] = (uint8_t) w;
	w >>= 8;
	dst[4] = (uint8_t) w;
	w >>= 8;
	dst[3] = (uint8_t) w;
	w >>= 8;
	dst[2] = (uint8_t) w;
	w >>= 8;
	dst[1] = (uint8_t) w;
	w >>= 8;
	dst[0] = (uint8_t) w;
#endif
}

#define LOAD32_BE(SRC) load32_be(SRC)
static inline uint32_t
load32_be(const uint8_t src[4])
{
#ifdef NATIVE_BIG_ENDIAN
	uint32_t w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	uint32_t w = (uint32_t) src[3];
	w |= (uint32_t) src[2] << 8;
	w |= (uint32_t) src[1] << 16;
	w |= (uint32_t) src[0] << 24;
	return w;
#endif
}

#define STORE32_BE(DST, W) store32_be((DST), (W))
static inline void
store32_be(uint8_t dst[4], uint32_t w)
{
#ifdef NATIVE_BIG_ENDIAN
	memcpy(dst, &w, sizeof w);
#else
	dst[3] = (uint8_t) w;
	w >>= 8;
	dst[2] = (uint8_t) w;
	w >>= 8;
	dst[1] = (uint8_t) w;
	w >>= 8;
	dst[0] = (uint8_t) w;
#endif
}

#define LOAD16_BE(SRC) load16_be(SRC)
static inline uint16_t
load16_be(const uint8_t src[2])
{
#ifdef NATIVE_BIG_ENDIAN
	uint16_t w;
	memcpy(&w, src, sizeof w);
	return w;
#else
	uint16_t w = (uint16_t) src[1];
	w |= (uint16_t) src[0] << 8;
	return w;
#endif
}

#define STORE16_BE(DST, W) store16_be((DST), (W))
static inline void
store16_be(uint8_t dst[2], uint16_t w)
{
#ifdef NATIVE_BIG_ENDIAN
	memcpy(dst, &w, sizeof w);
#else
	dst[1] = (uint8_t) w;
	w >>= 8;
	dst[0] = (uint8_t) w;
#endif
}

static inline void
mem_cpy(void *__restrict__ dst_, const void *__restrict__ src_, size_t n)
{
	unsigned char *      dst = (unsigned char *) dst_;
	const unsigned char *src = (const unsigned char *) src_;
	size_t               i;

	for (i = 0; i < n; i++) {
		dst[i] = src[i];
	}
}

static inline void
mem_zero(void *dst_, size_t n)
{
	unsigned char *dst = (unsigned char *) dst_;
	size_t         i;

	for (i = 0; i < n; i++) {
		dst[i] = 0;
	}
}

static inline void
mem_xor(void *__restrict__ dst_, const void *__restrict__ src_, size_t n)
{
	unsigned char *      dst = (unsigned char *) dst_;
	const unsigned char *src = (const unsigned char *) src_;
	size_t               i;

	for (i = 0; i < n; i++) {
		dst[i] ^= src[i];
	}
}

static inline void
mem_xor2(void *__restrict__ dst_, const void *__restrict__ src1_, const void *__restrict__ src2_,
		 size_t n)
{
	unsigned char *      dst  = (unsigned char *) dst_;
	const unsigned char *src1 = (const unsigned char *) src1_;
	const unsigned char *src2 = (const unsigned char *) src2_;
	size_t               i;

	for (i = 0; i < n; i++) {
		dst[i] = src1[i] ^ src2[i];
	}
}

static const uint8_t zero[64] = { 0 };


static int hydro_random_init(void);

/* ---------------- */

#define gimli_BLOCKBYTES 48
#define gimli_CAPACITY 32
#define gimli_RATE 16

#define gimli_TAG_HEADER 0x01
#define gimli_TAG_PAYLOAD 0x02
#define gimli_TAG_FINAL 0x08
#define gimli_TAG_FINAL0 0xf8
#define gimli_TAG_KEY0 0xfe
#define gimli_TAG_KEY 0xff

#define gimli_DOMAIN_AEAD 0x0
#define gimli_DOMAIN_XOF 0xf

static void gimli_core_u8(uint8_t state_u8[gimli_BLOCKBYTES], uint8_t tag);

static inline void
gimli_pad_u8(uint8_t buf[gimli_BLOCKBYTES], size_t pos, uint8_t domain)
{
	buf[pos] ^= (domain << 1) | 1;
	buf[gimli_RATE - 1] ^= 0x80;
}

static inline void
hydro_mem_ct_zero_u32(uint32_t *dst_, size_t n)
{
	volatile uint32_t *volatile dst = (volatile uint32_t * volatile)(void *) dst_;
	size_t i;

	for (i = 0; i < n; i++) {
		dst[i] = 0;
	}
}

static inline uint32_t hydro_mem_ct_cmp_u32(const uint32_t *b1_, const uint32_t *b2,
											size_t n) _hydro_attr_warn_unused_result_;

static inline uint32_t
hydro_mem_ct_cmp_u32(const uint32_t *b1_, const uint32_t *b2, size_t n)
{
	const volatile uint32_t *volatile b1 = (const volatile uint32_t *volatile)(const void *) b1_;
	size_t   i;
	uint32_t cv = 0;

	for (i = 0; i < n; i++) {
		cv |= b1[i] ^ b2[i];
	}
	return cv;
}

/* ---------------- */

static int hydro_hash_init_with_tweak(hydro_hash_state *state,
									  const char ctx[hydro_hash_CONTEXTBYTES], uint64_t tweak,
									  const uint8_t key[hydro_hash_KEYBYTES]);

/* ---------------- */

#define hydro_secretbox_NONCEBYTES 20
#define hydro_secretbox_MACBYTES 16

/* ---------------- */

#define hydro_x25519_BYTES 32
#define hydro_x25519_PUBLICKEYBYTES 32
#define hydro_x25519_SECRETKEYBYTES 32

static int hydro_x25519_scalarmult(uint8_t       out[hydro_x25519_BYTES],
								   const uint8_t scalar[hydro_x25519_SECRETKEYBYTES],
								   const uint8_t x1[hydro_x25519_PUBLICKEYBYTES],
								   bool          clamp) _hydro_attr_warn_unused_result_;

static inline int hydro_x25519_scalarmult_base(uint8_t       pk[hydro_x25519_PUBLICKEYBYTES],
											   const uint8_t sk[hydro_x25519_SECRETKEYBYTES])
	_hydro_attr_warn_unused_result_;

static inline void
hydro_x25519_scalarmult_base_uniform(uint8_t       pk[hydro_x25519_PUBLICKEYBYTES],
									 const uint8_t sk[hydro_x25519_SECRETKEYBYTES]);


int
hydro_init(void)
{
	if (hydro_random_init() != 0) {
		abort();
	}
	return 0;
}

void
hydro_memzero(void *pnt, size_t len)
{
#ifdef HAVE_EXPLICIT_BZERO
	explicit_bzero(pnt, len);
#else
	volatile unsigned char *volatile pnt_ = (volatile unsigned char *volatile) pnt;
	size_t i                              = (size_t) 0U;

	while (i < len) {
		pnt_[i++] = 0U;
	}
#endif
}

void
hydro_increment(uint8_t *n, size_t len)
{
	size_t        i;
	uint_fast16_t c = 1U;

	for (i = 0; i < len; i++) {
		c += (uint_fast16_t) n[i];
		n[i] = (uint8_t) c;
		c >>= 8;
	}
}

char *
hydro_bin2hex(char *hex, size_t hex_maxlen, const uint8_t *bin, size_t bin_len)
{
	size_t       i = (size_t) 0U;
	unsigned int x;
	int          b;
	int          c;

	if (bin_len >= SIZE_MAX / 2 || hex_maxlen <= bin_len * 2U) {
		abort();
	}
	while (i < bin_len) {
		c = bin[i] & 0xf;
		b = bin[i] >> 4;
		x = (unsigned char) (87U + c + (((c - 10U) >> 8) & ~38U)) << 8 |
			(unsigned char) (87U + b + (((b - 10U) >> 8) & ~38U));
		hex[i * 2U] = (char) x;
		x >>= 8;
		hex[i * 2U + 1U] = (char) x;
		i++;
	}
	hex[i * 2U] = 0U;

	return hex;
}

int
hydro_hex2bin(uint8_t *bin, size_t bin_maxlen, const char *hex, size_t hex_len, const char *ignore,
			  const char **hex_end_p)
{
	size_t        bin_pos = (size_t) 0U;
	size_t        hex_pos = (size_t) 0U;
	int           ret     = 0;
	unsigned char c;
	unsigned char c_alpha0, c_alpha;
	unsigned char c_num0, c_num;
	uint8_t       c_acc = 0U;
	uint8_t       c_val;
	unsigned char state = 0U;

	while (hex_pos < hex_len) {
		c        = (unsigned char) hex[hex_pos];
		c_num    = c ^ 48U;
		c_num0   = (c_num - 10U) >> 8;
		c_alpha  = (c & ~32U) - 55U;
		c_alpha0 = ((c_alpha - 10U) ^ (c_alpha - 16U)) >> 8;
		if ((c_num0 | c_alpha0) == 0U) {
			if (ignore != NULL && state == 0U && strchr(ignore, c) != NULL) {
				hex_pos++;
				continue;
			}
			break;
		}
		c_val = (uint8_t)((c_num0 & c_num) | (c_alpha0 & c_alpha));
		if (bin_pos >= bin_maxlen) {
			ret   = -1;
			errno = ERANGE;
			break;
		}
		if (state == 0U) {
			c_acc = c_val * 16U;
		} else {
			bin[bin_pos++] = c_acc | c_val;
		}
		state = ~state;
		hex_pos++;
	}
	if (state != 0U) {
		hex_pos--;
		errno = EINVAL;
		ret   = -1;
	}
	if (ret != 0) {
		bin_pos = (size_t) 0U;
	}
	if (hex_end_p != NULL) {
		*hex_end_p = &hex[hex_pos];
	} else if (hex_pos != hex_len) {
		errno = EINVAL;
		ret   = -1;
	}
	if (ret != 0) {
		return ret;
	}
	return (int) bin_pos;
}

bool
hydro_equal(const void *b1_, const void *b2_, size_t len)
{
	const volatile uint8_t *volatile b1 = (const volatile uint8_t *volatile) b1_;
	const uint8_t *b2                   = (const uint8_t *) b2_;
	size_t         i;
	uint8_t        d = (uint8_t) 0U;

	if (b1 == b2) {
		d = ~d;
	}
	for (i = 0U; i < len; i++) {
		d |= b1[i] ^ b2[i];
	}
	return (bool) (1 & ((d - 1) >> 8));
}

int
hydro_compare(const uint8_t *b1_, const uint8_t *b2_, size_t len)
{
	const volatile uint8_t *volatile b1 = (const volatile uint8_t *volatile) b1_;
	const uint8_t *b2                   = (const uint8_t *) b2_;
	uint8_t        gt                   = 0U;
	uint8_t        eq                   = 1U;
	size_t         i;

	i = len;
	while (i != 0U) {
		i--;
		gt |= ((b2[i] - b1[i]) >> 8) & eq;
		eq &= ((b2[i] ^ b1[i]) - 1) >> 8;
	}
	return (int) (gt + gt + eq) - 1;
}

int
hydro_pad(unsigned char *buf, size_t unpadded_buflen, size_t blocksize, size_t max_buflen)
{
	unsigned char *        tail;
	size_t                 i;
	size_t                 xpadlen;
	size_t                 xpadded_len;
	volatile unsigned char mask;
	unsigned char          barrier_mask;

	if (blocksize <= 0U || max_buflen > INT_MAX) {
		return -1;
	}
	xpadlen = blocksize - 1U;
	if ((blocksize & (blocksize - 1U)) == 0U) {
		xpadlen -= unpadded_buflen & (blocksize - 1U);
	} else {
		xpadlen -= unpadded_buflen % blocksize;
	}
	if (SIZE_MAX - unpadded_buflen <= xpadlen) {
		return -1;
	}
	xpadded_len = unpadded_buflen + xpadlen;
	if (xpadded_len >= max_buflen) {
		return -1;
	}
	tail = &buf[xpadded_len];
	mask = 0U;
	for (i = 0; i < blocksize; i++) {
		barrier_mask = (unsigned char) (((i ^ xpadlen) - 1U) >> ((sizeof(size_t) - 1U) * CHAR_BIT));
		tail[-i]     = (tail[-i] & mask) | (0x80 & barrier_mask);
		mask |= barrier_mask;
	}
	return (int) (xpadded_len + 1);
}

int
hydro_unpad(const unsigned char *buf, size_t padded_buflen, size_t blocksize)
{
	const unsigned char *tail;
	unsigned char        acc = 0U;
	unsigned char        c;
	unsigned char        valid   = 0U;
	volatile size_t      pad_len = 0U;
	size_t               i;
	size_t               is_barrier;

	if (padded_buflen < blocksize || blocksize <= 0U) {
		return -1;
	}
	tail = &buf[padded_buflen - 1U];

	for (i = 0U; i < blocksize; i++) {
		c          = tail[-i];
		is_barrier = (((acc - 1U) & (pad_len - 1U) & ((c ^ 0x80) - 1U)) >> 8) & 1U;
		acc |= c;
		pad_len |= (i & -is_barrier);
		valid |= (unsigned char) is_barrier;
	}
	if (valid == 0) {
		return -1;
	}
	return (int) (padded_buflen - 1 - pad_len);
}

#ifdef __SSE2__

	#include <emmintrin.h>
	#ifdef __SSSE3__
	# include <tmmintrin.h>
	#endif

	#define S 9

	static inline __m128i
	shift(__m128i x, int bits)
	{
		return _mm_slli_epi32(x, bits);
	}

	static inline __m128i
	rotate(__m128i x, int bits)
	{
		return _mm_slli_epi32(x, bits) | _mm_srli_epi32(x, 32 - bits);
	}

	#ifdef __SSSE3__
	static inline __m128i
	rotate24(__m128i x)
	{
		return _mm_shuffle_epi8(x, _mm_set_epi8(12, 15, 14, 13, 8, 11, 10, 9, 4, 7, 6, 5, 0, 3, 2, 1));
	}
	#else
	static inline __m128i
	rotate24(__m128i x)
	{
		uint8_t _hydro_attr_aligned_(16) x8[16], y8[16];

		_mm_storeu_si128((__m128i *) (void *) x8, x);

		y8[ 0] = x8[ 1]; y8[ 1] = x8[ 2]; y8[ 2] = x8[ 3]; y8[ 3] = x8[ 0];
		y8[ 4] = x8[ 5]; y8[ 5] = x8[ 6]; y8[ 6] = x8[ 7]; y8[ 7] = x8[ 4];
		y8[ 8] = x8[ 9]; y8[ 9] = x8[10]; y8[10] = x8[11]; y8[11] = x8[ 8];
		y8[12] = x8[13]; y8[13] = x8[14]; y8[14] = x8[15]; y8[15] = x8[12];

		return _mm_loadu_si128((const __m128i *) (const void *) y8);
	}
	#endif

	static const uint32_t coeffs[24] _hydro_attr_aligned_(16) = {
		0x9e377904, 0, 0, 0, 0x9e377908, 0, 0, 0, 0x9e37790c, 0, 0, 0,
		0x9e377910, 0, 0, 0, 0x9e377914, 0, 0, 0, 0x9e377918, 0, 0, 0,
	};

	static void
	gimli_core(uint32_t state[gimli_BLOCKBYTES / 4])
	{
		__m128i x = _mm_loadu_si128((const __m128i *) (const void *) &state[0]);
		__m128i y = _mm_loadu_si128((const __m128i *) (const void *) &state[4]);
		__m128i z = _mm_loadu_si128((const __m128i *) (const void *) &state[8]);
		__m128i newy;
		__m128i newz;
		int     round;

		for (round = 5; round >= 0; round--) {
			x    = rotate24(x);
			y    = rotate(y, S);
			newz = x ^ shift(z, 1) ^ shift(y & z, 2);
			newy = y ^ x ^ shift(x | z, 1);
			x    = z ^ y ^ shift(x & y, 3);
			y    = newy;
			z    = newz;

			x = _mm_shuffle_epi32(x, _MM_SHUFFLE(2, 3, 0, 1));
			x ^= ((const __m128i *) (const void *) coeffs)[round];

			x    = rotate24(x);
			y    = rotate(y, S);
			newz = x ^ shift(z, 1) ^ shift(y & z, 2);
			newy = y ^ x ^ shift(x | z, 1);
			x    = z ^ y ^ shift(x & y, 3);
			y    = newy;
			z    = newz;

			x    = rotate24(x);
			y    = rotate(y, S);
			newz = x ^ shift(z, 1) ^ shift(y & z, 2);
			newy = y ^ x ^ shift(x | z, 1);
			x    = z ^ y ^ shift(x & y, 3);
			y    = newy;
			z    = newz;

			x = _mm_shuffle_epi32(x, _MM_SHUFFLE(1, 0, 3, 2));

			x    = rotate24(x);
			y    = rotate(y, S);
			newz = x ^ shift(z, 1) ^ shift(y & z, 2);
			newy = y ^ x ^ shift(x | z, 1);
			x    = z ^ y ^ shift(x & y, 3);
			y    = newy;
			z    = newz;
		}

		_mm_storeu_si128((__m128i *) (void *) &state[0], x);
		_mm_storeu_si128((__m128i *) (void *) &state[4], y);
		_mm_storeu_si128((__m128i *) (void *) &state[8], z);
	}
	#undef S
#else

	static void
	gimli_core(uint32_t state[gimli_BLOCKBYTES / 4])
	{
		unsigned int round;
		unsigned int column;
		uint32_t     x;
		uint32_t     y;
		uint32_t     z;

		for (round = 24; round > 0; round--) {
			for (column = 0; column < 4; column++) {
				x = ROTL32(state[column], 24);
				y = ROTL32(state[4 + column], 9);
				z = state[8 + column];

				state[8 + column] = x ^ (z << 1) ^ ((y & z) << 2);
				state[4 + column] = y ^ x ^ ((x | z) << 1);
				state[column]     = z ^ y ^ ((x & y) << 3);
			}
			switch (round & 3) {
			case 0:
				x        = state[0];
				state[0] = state[1];
				state[1] = x;
				x        = state[2];
				state[2] = state[3];
				state[3] = x;
				state[0] ^= ((uint32_t) 0x9e377900 | round);
				break;
			case 2:
				x        = state[0];
				state[0] = state[2];
				state[2] = x;
				x        = state[1];
				state[1] = state[3];
				state[3] = x;
			}
		}
	}

#endif

static void
gimli_core_u8(uint8_t state_u8[gimli_BLOCKBYTES], uint8_t tag)
{
	state_u8[gimli_BLOCKBYTES - 1] ^= tag;
#ifndef NATIVE_LITTLE_ENDIAN
	uint32_t state_u32[12];
	int      i;

	for (i = 0; i < 12; i++) {
		state_u32[i] = LOAD32_LE(&state_u8[i * 4]);
	}
	gimli_core(state_u32);
	for (i = 0; i < 12; i++) {
		STORE32_LE(&state_u8[i * 4], state_u32[i]);
	}
#else
	gimli_core((uint32_t *) (void *) state_u8); /* state_u8 must be properly aligned */
#endif
}


static TLS struct {
	_hydro_attr_aligned_(16) uint8_t state[gimli_BLOCKBYTES];
	uint64_t counter;
	uint8_t  initialized;
	uint8_t  available;
} hydro_random_context;

#if defined(AVR) && !defined(__unix__)

	#include <Arduino.h>

	static bool
	hydro_random_rbit(uint16_t x)
	{
		uint8_t x8;

		x8 = ((uint8_t)(x >> 8)) ^ (uint8_t) x;
		x8 = (x8 >> 4) ^ (x8 & 0xf);
		x8 = (x8 >> 2) ^ (x8 & 0x3);
		x8 = (x8 >> 1) ^ x8;

		return (bool) (x8 & 1);
	}

	static int
	hydro_random_init(void)
	{
		const char       ctx[hydro_hash_CONTEXTBYTES] = { 'h', 'y', 'd', 'r', 'o', 'P', 'R', 'G' };
		hydro_hash_state st;
		uint16_t         ebits = 0;
		uint16_t         tc;
		bool             a, b;

		cli();
		MCUSR = 0;
		WDTCSR |= _BV(WDCE) | _BV(WDE);
		WDTCSR = _BV(WDIE);
		sei();

		hydro_hash_init(&st, ctx, NULL);

		while (ebits < 256) {
			delay(1);
			tc = TCNT1;
			hydro_hash_update(&st, (const uint8_t *) &tc, sizeof tc);
			a = hydro_random_rbit(tc);
			delay(1);
			tc = TCNT1;
			b  = hydro_random_rbit(tc);
			hydro_hash_update(&st, (const uint8_t *) &tc, sizeof tc);
			if (a == b) {
				continue;
			}
			hydro_hash_update(&st, (const uint8_t *) &b, sizeof b);
			ebits++;
		}

		cli();
		MCUSR = 0;
		WDTCSR |= _BV(WDCE) | _BV(WDE);
		WDTCSR = 0;
		sei();

		hydro_hash_final(&st, hydro_random_context.state, sizeof hydro_random_context.state);
		hydro_random_context.counter = ~LOAD64_LE(hydro_random_context.state);

		return 0;
	}

	ISR(WDT_vect) {}

#elif (defined(ESP32) || defined(ESP8266)) && !defined(__unix__)

	// Important: RF *must* be activated on ESP board
	// https://techtutorialsx.com/2017/12/22/esp32-arduino-random-number-generation/
	#ifdef ESP32
	# include <esp_system.h>
	#endif

	static int
	hydro_random_init(void)
	{
		const char       ctx[hydro_hash_CONTEXTBYTES] = { 'h', 'y', 'd', 'r', 'o', 'P', 'R', 'G' };
		hydro_hash_state st;
		uint16_t         ebits = 0;

		hydro_hash_init(&st, ctx, NULL);

		while (ebits < 256) {
			uint32_t r = esp_random();

			delay(10);
			hydro_hash_update(&st, (const uint32_t *) &r, sizeof r);
			ebits += 32;
		}

		hydro_hash_final(&st, hydro_random_context.state, sizeof hydro_random_context.state);
		hydro_random_context.counter = ~LOAD64_LE(hydro_random_context.state);

		return 0;
	}

#elif defined(PARTICLE) && defined(PLATFORM_ID) && PLATFORM_ID > 2 && !defined(__unix__)

	// Note: All particle platforms except for the Spark Core have a HW RNG.  Only allow building on
	// supported platforms for now. PLATFORM_ID definitions:
	// https://github.com/particle-iot/device-os/blob/mesh-develop/hal/shared/platforms.h

	#include <Particle.h>

	static int
	hydro_random_init(void)
	{
		const char       ctx[hydro_hash_CONTEXTBYTES] = { 'h', 'y', 'd', 'r', 'o', 'P', 'R', 'G' };
		hydro_hash_state st;
		uint16_t         ebits = 0;

		hydro_hash_init(&st, ctx, NULL);

		while (ebits < 256) {
			uint32_t r = HAL_RNG_GetRandomNumber();
			hydro_hash_update(&st, (const uint32_t *) &r, sizeof r);
			ebits += 32;
		}

		hydro_hash_final(&st, hydro_random_context.state, sizeof hydro_random_context.state);
		hydro_random_context.counter = ~LOAD64_LE(hydro_random_context.state);

		return 0;
	}

#elif (defined(NRF52832_XXAA) || defined(NRF52832_XXAB)) && !defined(__unix__)

	// Important: The SoftDevice *must* be activated to enable reading from the RNG
	// http://infocenter.nordicsemi.com/index.jsp?topic=%2Fcom.nordic.infocenter.nrf52832.ps.v1.1%2Frng.html

	#include <nrf_soc.h>

	static int
	hydro_random_init(void)
	{
		const char       ctx[hydro_hash_CONTEXTBYTES] = { 'h', 'y', 'd', 'r', 'o', 'P', 'R', 'G' };
		hydro_hash_state st;
		const uint8_t    total_bytes     = 32;
		uint8_t          remaining_bytes = total_bytes;
		uint8_t          available_bytes;
		uint8_t          rand_buffer[32];

		hydro_hash_init(&st, ctx, NULL);

		for (;;) {
			if (sd_rand_application_bytes_available_get(&available_bytes) != NRF_SUCCESS) {
				return -1;
			}
			if (available_bytes > 0) {
				if (available_bytes > remaining_bytes) {
					available_bytes = remaining_bytes;
				}
				if (sd_rand_application_vector_get(rand_buffer, available_bytes) != NRF_SUCCESS) {
					return -1;
				}
				hydro_hash_update(&st, rand_buffer, total_bytes);
				remaining_bytes -= available_bytes;
			}
			if (remaining_bytes <= 0) {
				break;
			}
			delay(10);
		}
		hydro_hash_final(&st, hydro_random_context.state, sizeof hydro_random_context.state);
		hydro_random_context.counter = ~LOAD64_LE(hydro_random_context.state);

		return 0;
	}

#elif defined(_WIN32)

	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#define RtlGenRandom SystemFunction036
	#if defined(__cplusplus)
	extern "C"
	#endif
		BOOLEAN NTAPI
		RtlGenRandom(PVOID RandomBuffer, ULONG RandomBufferLength);
	#pragma comment(lib, "advapi32.lib")

	static int
	hydro_random_init(void)
	{
		if (!RtlGenRandom((PVOID) hydro_random_context.state,
						  (ULONG) sizeof hydro_random_context.state)) {
			return -1;
		}
		hydro_random_context.counter = ~LOAD64_LE(hydro_random_context.state);

		return 0;
	}

#elif defined(__wasi__)

	#include <unistd.h>

	static int
	hydro_random_init(void)
	{
		if (getentropy(hydro_random_context.state, sizeof hydro_random_context.state) != 0) {
			return -1;
		}
		hydro_random_context.counter = ~LOAD64_LE(hydro_random_context.state);

		return 0;
	}

#elif defined(__unix__)

	#include <errno.h>
	#include <fcntl.h>
	#ifdef __linux__
	#include <poll.h>
	#endif
	#include <sys/types.h>
	#include <unistd.h>

	#ifdef __linux__
	static int
	hydro_random_block_on_dev_random(void)
	{
		struct pollfd pfd;
		int           fd;
		int           pret;

		fd = open("/dev/random", O_RDONLY);
		if (fd == -1) {
			return 0;
		}
		pfd.fd      = fd;
		pfd.events  = POLLIN;
		pfd.revents = 0;
		do {
			pret = poll(&pfd, 1, -1);
		} while (pret < 0 && (errno == EINTR || errno == EAGAIN));
		if (pret != 1) {
			(void) close(fd);
			errno = EIO;
			return -1;
		}
		return close(fd);
	}
	#endif

	static ssize_t
	hydro_random_safe_read(const int fd, void *const buf_, size_t len)
	{
		unsigned char *buf = (unsigned char *) buf_;
		ssize_t        readnb;

		do {
			while ((readnb = read(fd, buf, len)) < (ssize_t) 0 && (errno == EINTR || errno == EAGAIN)) {
			}
			if (readnb < (ssize_t) 0) {
				return readnb;
			}
			if (readnb == (ssize_t) 0) {
				break;
			}
			len -= (size_t) readnb;
			buf += readnb;
		} while (len > (ssize_t) 0);

		return (ssize_t)(buf - (unsigned char *) buf_);
	}

	static int
	hydro_random_init(void)
	{
		uint8_t tmp[gimli_BLOCKBYTES + 8];
		int     fd;
		int     ret = -1;

	#ifdef __linux__
		if (hydro_random_block_on_dev_random() != 0) {
			return -1;
		}
	#endif
		do {
			fd = open("/dev/urandom", O_RDONLY);
			if (fd == -1 && errno != EINTR) {
				return -1;
			}
		} while (fd == -1);
		if (hydro_random_safe_read(fd, tmp, sizeof tmp) == (ssize_t) sizeof tmp) {
			memcpy(hydro_random_context.state, tmp, gimli_BLOCKBYTES);
			memcpy(&hydro_random_context.counter, tmp + gimli_BLOCKBYTES, 8);
			hydro_memzero(tmp, sizeof tmp);
			ret = 0;
		}
		ret |= close(fd);

		return ret;
	}

#elif defined(TARGET_LIKE_MBED)

	#include <mbedtls/ctr_drbg.h>
	#include <mbedtls/entropy.h>

	#if defined(MBEDTLS_ENTROPY_C)

	static int
	hydro_random_init(void)
	{
		mbedtls_entropy_context entropy;
		uint16_t                pos = 0;

		mbedtls_entropy_init(&entropy);

		// Pull data directly out of the entropy pool for the state, as it's small enough.
		if (mbedtls_entropy_func(&entropy, (uint8_t *) &hydro_random_context.counter,
								 sizeof hydro_random_context.counter) != 0) {
			return -1;
		}
		// mbedtls_entropy_func can't provide more than MBEDTLS_ENTROPY_BLOCK_SIZE in one go.
		// This constant depends of mbedTLS configuration (whether the PRNG is backed by SHA256/SHA512
		// at this time) Therefore, if necessary, we get entropy multiple times.

		do {
			const uint8_t dataLeftToConsume = gimli_BLOCKBYTES - pos;
			const uint8_t currentChunkSize  = (dataLeftToConsume > MBEDTLS_ENTROPY_BLOCK_SIZE)
												 ? MBEDTLS_ENTROPY_BLOCK_SIZE
												 : dataLeftToConsume;

			// Forces mbedTLS to fetch fresh entropy, then get some to feed libhydrogen.
			if (mbedtls_entropy_gather(&entropy) != 0 ||
				mbedtls_entropy_func(&entropy, &hydro_random_context.state[pos], currentChunkSize) !=
					0) {
				return -1;
			}
			pos += MBEDTLS_ENTROPY_BLOCK_SIZE;
		} while (pos < gimli_BLOCKBYTES);

		mbedtls_entropy_free(&entropy);

		return 0;
	}
	#else
	# error Need an entropy source
	#endif

#elif defined(RIOT_VERSION)

	#include <random.h>

	static int
	hydro_random_init(void)
	{
		random_bytes(hydro_random_context.state, sizeof(hydro_random_context.state));
		hydro_random_context.counter = ~LOAD64_LE(hydro_random_context.state);

		return 0;
	}

#else

	#error Unsupported platform (libhydrogen).

#endif

static void
hydro_random_check_initialized(void)
{
	if (hydro_random_context.initialized == 0) {
		if (hydro_random_init() != 0) {
			abort();
		}
		gimli_core_u8(hydro_random_context.state, 0);
		hydro_random_ratchet();
		hydro_random_context.initialized = 1;
	}
}

void
hydro_random_ratchet(void)
{
	mem_zero(hydro_random_context.state, gimli_RATE);
	STORE64_LE(hydro_random_context.state, hydro_random_context.counter);
	hydro_random_context.counter++;
	gimli_core_u8(hydro_random_context.state, 0);
	hydro_random_context.available = gimli_RATE;
}

uint32_t
hydro_random_u32(void)
{
	uint32_t v;

	hydro_random_check_initialized();
	if (hydro_random_context.available < 4) {
		hydro_random_ratchet();
	}
	memcpy(&v, &hydro_random_context.state[gimli_RATE - hydro_random_context.available], 4);
	hydro_random_context.available -= 4;

	return v;
}

uint32_t
hydro_random_uniform(const uint32_t upper_bound)
{
	uint32_t min;
	uint32_t r;

	if (upper_bound < 2U) {
		return 0;
	}
	min = (1U + ~upper_bound) % upper_bound; /* = 2**32 mod upper_bound */
	do {
		r = hydro_random_u32();
	} while (r < min);
	/* r is now clamped to a set whose size mod upper_bound == 0
	 * the worst case (2**31+1) requires 2 attempts on average */

	return r % upper_bound;
}

void
hydro_random_buf(void *out, size_t out_len)
{
	uint8_t *p = (uint8_t *) out;
	size_t   i;
	size_t   leftover;

	hydro_random_check_initialized();
	for (i = 0; i < out_len / gimli_RATE; i++) {
		gimli_core_u8(hydro_random_context.state, 0);
		memcpy(p + i * gimli_RATE, hydro_random_context.state, gimli_RATE);
	}
	leftover = out_len % gimli_RATE;
	if (leftover != 0) {
		gimli_core_u8(hydro_random_context.state, 0);
		mem_cpy(p + i * gimli_RATE, hydro_random_context.state, leftover);
	}
	hydro_random_ratchet();
}

void
hydro_random_buf_deterministic(void *out, size_t out_len,
							   const uint8_t seed[hydro_random_SEEDBYTES])
{
	static const uint8_t             prefix[] = { 7, 'd', 'r', 'b', 'g', '2', '5', '6' };
	_hydro_attr_aligned_(16) uint8_t state[gimli_BLOCKBYTES];
	uint8_t *                        p = (uint8_t *) out;
	size_t                           i;
	size_t                           leftover;

	mem_zero(state, gimli_BLOCKBYTES);
	COMPILER_ASSERT(sizeof prefix + 8 <= gimli_RATE);
	memcpy(state, prefix, sizeof prefix);
	STORE64_LE(state + sizeof prefix, (uint64_t) out_len);
	gimli_core_u8(state, 1);
	COMPILER_ASSERT(hydro_random_SEEDBYTES == gimli_RATE * 2);
	mem_xor(state, seed, gimli_RATE);
	gimli_core_u8(state, 2);
	mem_xor(state, seed + gimli_RATE, gimli_RATE);
	gimli_core_u8(state, 2);
	for (i = 0; i < out_len / gimli_RATE; i++) {
		gimli_core_u8(state, 0);
		memcpy(p + i * gimli_RATE, state, gimli_RATE);
	}
	leftover = out_len % gimli_RATE;
	if (leftover != 0) {
		gimli_core_u8(state, 0);
		mem_cpy(p + i * gimli_RATE, state, leftover);
	}
}

void
hydro_random_reseed(void)
{
	hydro_random_context.initialized = 0;
	hydro_random_check_initialized();
}


int
hydro_hash_update(hydro_hash_state *state, const void *in_, size_t in_len)
{
	const uint8_t *in  = (const uint8_t *) in_;
	uint8_t *      buf = (uint8_t *) (void *) state->state;
	size_t         left;
	size_t         ps;
	size_t         i;

	while (in_len > 0) {
		left = gimli_RATE - state->buf_off;
		if ((ps = in_len) > left) {
			ps = left;
		}
		for (i = 0; i < ps; i++) {
			buf[state->buf_off + i] ^= in[i];
		}
		in += ps;
		in_len -= ps;
		state->buf_off += (uint8_t) ps;
		if (state->buf_off == gimli_RATE) {
			gimli_core_u8(buf, 0);
			state->buf_off = 0;
		}
	}
	return 0;
}

/* pad(str_enc("kmac") || str_enc(context)) || pad(str_enc(k)) ||
   msg || right_enc(msg_len) || 0x00 */

int
hydro_hash_init(hydro_hash_state *state, const char ctx[hydro_hash_CONTEXTBYTES],
				const uint8_t key[hydro_hash_KEYBYTES])
{
	uint8_t block[64] = { 4, 'k', 'm', 'a', 'c', 8 };
	size_t  p;

	COMPILER_ASSERT(hydro_hash_KEYBYTES <= sizeof block - gimli_RATE - 1);
	COMPILER_ASSERT(hydro_hash_CONTEXTBYTES == 8);
	mem_zero(block + 14, sizeof block - 14);
	memcpy(block + 6, ctx, 8);
	if (key != NULL) {
		block[gimli_RATE] = (uint8_t) hydro_hash_KEYBYTES;
		memcpy(block + gimli_RATE + 1, key, hydro_hash_KEYBYTES);
		p = (gimli_RATE + 1 + hydro_hash_KEYBYTES + (gimli_RATE - 1)) & ~(size_t)(gimli_RATE - 1);
	} else {
		block[gimli_RATE] = (uint8_t) 0;
		p                 = (gimli_RATE + 1 + 0 + (gimli_RATE - 1)) & ~(size_t)(gimli_RATE - 1);
	}
	mem_zero(state, sizeof *state);
	hydro_hash_update(state, block, p);

	return 0;
}

/* pad(str_enc("tmac") || str_enc(context)) || pad(str_enc(k)) ||
   pad(right_enc(tweak)) || msg || right_enc(msg_len) || 0x00 */

static int
hydro_hash_init_with_tweak(hydro_hash_state *state, const char ctx[hydro_hash_CONTEXTBYTES],
						   uint64_t tweak, const uint8_t key[hydro_hash_KEYBYTES])
{
	uint8_t block[80] = { 4, 't', 'm', 'a', 'c', 8 };
	size_t  p;

	COMPILER_ASSERT(hydro_hash_KEYBYTES <= sizeof block - 2 * gimli_RATE - 1);
	COMPILER_ASSERT(hydro_hash_CONTEXTBYTES == 8);
	mem_zero(block + 14, sizeof block - 14);
	memcpy(block + 6, ctx, 8);
	if (key != NULL) {
		block[gimli_RATE] = (uint8_t) hydro_hash_KEYBYTES;
		memcpy(block + gimli_RATE + 1, key, hydro_hash_KEYBYTES);
		p = (gimli_RATE + 1 + hydro_hash_KEYBYTES + (gimli_RATE - 1)) & ~(size_t)(gimli_RATE - 1);
	} else {
		block[gimli_RATE] = (uint8_t) 0;
		p                 = (gimli_RATE + 1 + 0 + (gimli_RATE - 1)) & ~(size_t)(gimli_RATE - 1);
	}
	block[p] = (uint8_t) sizeof tweak;
	STORE64_LE(&block[p + 1], tweak);
	p += gimli_RATE;
	mem_zero(state, sizeof *state);
	hydro_hash_update(state, block, p);

	return 0;
}

int
hydro_hash_final(hydro_hash_state *state, uint8_t *out, size_t out_len)
{
	uint8_t  lc[4];
	uint8_t *buf = (uint8_t *) (void *) state->state;
	size_t   i;
	size_t   lc_len;
	size_t   leftover;

	if (out_len < hydro_hash_BYTES_MIN || out_len > hydro_hash_BYTES_MAX) {
		return -1;
	}
	COMPILER_ASSERT(hydro_hash_BYTES_MAX <= 0xffff);
	lc[1]  = (uint8_t) out_len;
	lc[2]  = (uint8_t)(out_len >> 8);
	lc[3]  = 0;
	lc_len = (size_t)(1 + (lc[2] != 0));
	lc[0]  = (uint8_t) lc_len;
	hydro_hash_update(state, lc, 1 + lc_len + 1);
	gimli_pad_u8(buf, state->buf_off, gimli_DOMAIN_XOF);
	for (i = 0; i < out_len / gimli_RATE; i++) {
		gimli_core_u8(buf, 0);
		memcpy(out + i * gimli_RATE, buf, gimli_RATE);
	}
	leftover = out_len % gimli_RATE;
	if (leftover != 0) {
		gimli_core_u8(buf, 0);
		mem_cpy(out + i * gimli_RATE, buf, leftover);
	}
	state->buf_off = gimli_RATE;

	return 0;
}

int
hydro_hash_hash(uint8_t *out, size_t out_len, const void *in_, size_t in_len,
				const char ctx[hydro_hash_CONTEXTBYTES], const uint8_t key[hydro_hash_KEYBYTES])
{
	hydro_hash_state st;
	const uint8_t *  in = (const uint8_t *) in_;

	if (hydro_hash_init(&st, ctx, key) != 0 || hydro_hash_update(&st, in, in_len) != 0 ||
		hydro_hash_final(&st, out, out_len) != 0) {
		return -1;
	}
	return 0;
}

void
hydro_hash_keygen(uint8_t key[hydro_hash_KEYBYTES])
{
	hydro_random_buf(key, hydro_hash_KEYBYTES);
}


int
hydro_kdf_derive_from_key(uint8_t *subkey, size_t subkey_len, uint64_t subkey_id,
						  const char    ctx[hydro_kdf_CONTEXTBYTES],
						  const uint8_t key[hydro_kdf_KEYBYTES])
{
	hydro_hash_state st;

	COMPILER_ASSERT(hydro_kdf_CONTEXTBYTES >= hydro_hash_CONTEXTBYTES);
	COMPILER_ASSERT(hydro_kdf_KEYBYTES >= hydro_hash_KEYBYTES);
	if (hydro_hash_init_with_tweak(&st, ctx, subkey_id, key) != 0) {
		return -1;
	}
	return hydro_hash_final(&st, subkey, subkey_len);
}

void
hydro_kdf_keygen(uint8_t key[hydro_kdf_KEYBYTES])
{
	hydro_random_buf(key, hydro_kdf_KEYBYTES);
}


#define hydro_secretbox_IVBYTES 20
#define hydro_secretbox_SIVBYTES 20
#define hydro_secretbox_MACBYTES 16

void
hydro_secretbox_keygen(uint8_t key[hydro_secretbox_KEYBYTES])
{
	hydro_random_buf(key, hydro_secretbox_KEYBYTES);
}

static void
hydro_secretbox_xor_enc(uint8_t buf[gimli_BLOCKBYTES], uint8_t *out, const uint8_t *in,
						size_t inlen)
{
	size_t i;
	size_t leftover;

	for (i = 0; i < inlen / gimli_RATE; i++) {
		mem_xor2(&out[i * gimli_RATE], &in[i * gimli_RATE], buf, gimli_RATE);
		memcpy(buf, &out[i * gimli_RATE], gimli_RATE);
		gimli_core_u8(buf, gimli_TAG_PAYLOAD);
	}
	leftover = inlen % gimli_RATE;
	if (leftover != 0) {
		mem_xor2(&out[i * gimli_RATE], &in[i * gimli_RATE], buf, leftover);
		mem_cpy(buf, &out[i * gimli_RATE], leftover);
	}
	gimli_pad_u8(buf, leftover, gimli_DOMAIN_AEAD);
	gimli_core_u8(buf, gimli_TAG_PAYLOAD);
}

static void
hydro_secretbox_xor_dec(uint8_t buf[gimli_BLOCKBYTES], uint8_t *out, const uint8_t *in,
						size_t inlen)
{
	size_t i;
	size_t leftover;

	for (i = 0; i < inlen / gimli_RATE; i++) {
		mem_xor2(&out[i * gimli_RATE], &in[i * gimli_RATE], buf, gimli_RATE);
		memcpy(buf, &in[i * gimli_RATE], gimli_RATE);
		gimli_core_u8(buf, gimli_TAG_PAYLOAD);
	}
	leftover = inlen % gimli_RATE;
	if (leftover != 0) {
		mem_xor2(&out[i * gimli_RATE], &in[i * gimli_RATE], buf, leftover);
		mem_cpy(buf, &in[i * gimli_RATE], leftover);
	}
	gimli_pad_u8(buf, leftover, gimli_DOMAIN_AEAD);
	gimli_core_u8(buf, gimli_TAG_PAYLOAD);
}

static void
hydro_secretbox_setup(uint8_t buf[gimli_BLOCKBYTES], uint64_t msg_id,
					  const char    ctx[hydro_secretbox_CONTEXTBYTES],
					  const uint8_t key[hydro_secretbox_KEYBYTES],
					  const uint8_t iv[hydro_secretbox_IVBYTES], uint8_t key_tag)
{
	static const uint8_t prefix[] = { 6, 's', 'b', 'x', '2', '5', '6', 8 };
	uint8_t              msg_id_le[8];

	mem_zero(buf, gimli_BLOCKBYTES);
	COMPILER_ASSERT(hydro_secretbox_CONTEXTBYTES == 8);
	COMPILER_ASSERT(sizeof prefix + hydro_secretbox_CONTEXTBYTES <= gimli_RATE);
	memcpy(buf, prefix, sizeof prefix);
	memcpy(buf + sizeof prefix, ctx, hydro_secretbox_CONTEXTBYTES);
	COMPILER_ASSERT(sizeof prefix + hydro_secretbox_CONTEXTBYTES == gimli_RATE);
	gimli_core_u8(buf, gimli_TAG_HEADER);

	COMPILER_ASSERT(hydro_secretbox_KEYBYTES == 2 * gimli_RATE);
	mem_xor(buf, key, gimli_RATE);
	gimli_core_u8(buf, key_tag);
	mem_xor(buf, key + gimli_RATE, gimli_RATE);
	gimli_core_u8(buf, key_tag);

	COMPILER_ASSERT(hydro_secretbox_IVBYTES < gimli_RATE * 2);
	buf[0] ^= hydro_secretbox_IVBYTES;
	mem_xor(&buf[1], iv, gimli_RATE - 1);
	gimli_core_u8(buf, gimli_TAG_HEADER);
	mem_xor(buf, iv + gimli_RATE - 1, hydro_secretbox_IVBYTES - (gimli_RATE - 1));
	STORE64_LE(msg_id_le, msg_id);
	COMPILER_ASSERT(hydro_secretbox_IVBYTES - gimli_RATE + 8 <= gimli_RATE);
	mem_xor(buf + hydro_secretbox_IVBYTES - gimli_RATE, msg_id_le, 8);
	gimli_core_u8(buf, gimli_TAG_HEADER);
}

static void
hydro_secretbox_final(uint8_t *buf, const uint8_t key[hydro_secretbox_KEYBYTES], uint8_t tag)
{
	COMPILER_ASSERT(hydro_secretbox_KEYBYTES == gimli_CAPACITY);
	mem_xor(buf + gimli_RATE, key, hydro_secretbox_KEYBYTES);
	gimli_core_u8(buf, tag);
	mem_xor(buf + gimli_RATE, key, hydro_secretbox_KEYBYTES);
	gimli_core_u8(buf, tag);
}

static int
hydro_secretbox_encrypt_iv(uint8_t *c, const void *m_, size_t mlen, uint64_t msg_id,
						   const char    ctx[hydro_secretbox_CONTEXTBYTES],
						   const uint8_t key[hydro_secretbox_KEYBYTES],
						   const uint8_t iv[hydro_secretbox_IVBYTES])
{
	_hydro_attr_aligned_(16) uint32_t state[gimli_BLOCKBYTES / 4];
	uint8_t *                         buf = (uint8_t *) (void *) state;
	const uint8_t *                   m   = (const uint8_t *) m_;
	uint8_t *                         siv = &c[0];
	uint8_t *                         mac = &c[hydro_secretbox_SIVBYTES];
	uint8_t *                         ct  = &c[hydro_secretbox_SIVBYTES + hydro_secretbox_MACBYTES];
	size_t                            i;
	size_t                            leftover;

	if (c == m) {
		memmove(c + hydro_secretbox_HEADERBYTES, m, mlen);
		m = c + hydro_secretbox_HEADERBYTES;
	}

	/* first pass: compute the SIV */

	hydro_secretbox_setup(buf, msg_id, ctx, key, iv, gimli_TAG_KEY0);
	for (i = 0; i < mlen / gimli_RATE; i++) {
		mem_xor(buf, &m[i * gimli_RATE], gimli_RATE);
		gimli_core_u8(buf, gimli_TAG_PAYLOAD);
	}
	leftover = mlen % gimli_RATE;
	if (leftover != 0) {
		mem_xor(buf, &m[i * gimli_RATE], leftover);
	}
	gimli_pad_u8(buf, leftover, gimli_DOMAIN_XOF);
	gimli_core_u8(buf, gimli_TAG_PAYLOAD);

	hydro_secretbox_final(buf, key, gimli_TAG_FINAL0);
	COMPILER_ASSERT(hydro_secretbox_SIVBYTES <= gimli_CAPACITY);
	memcpy(siv, buf + gimli_RATE, hydro_secretbox_SIVBYTES);

	/* second pass: encrypt the message, mix the key, squeeze an extra block for
	 * the MAC */

	COMPILER_ASSERT(hydro_secretbox_SIVBYTES == hydro_secretbox_IVBYTES);
	hydro_secretbox_setup(buf, msg_id, ctx, key, siv, gimli_TAG_KEY);
	hydro_secretbox_xor_enc(buf, ct, m, mlen);

	hydro_secretbox_final(buf, key, gimli_TAG_FINAL);
	COMPILER_ASSERT(hydro_secretbox_MACBYTES <= gimli_CAPACITY);
	memcpy(mac, buf + gimli_RATE, hydro_secretbox_MACBYTES);

	return 0;
}

void
hydro_secretbox_probe_create(uint8_t probe[hydro_secretbox_PROBEBYTES], const uint8_t *c,
							 size_t c_len, const char ctx[hydro_secretbox_CONTEXTBYTES],
							 const uint8_t key[hydro_secretbox_KEYBYTES])
{
	const uint8_t *mac;

	if (c_len < hydro_secretbox_HEADERBYTES) {
		abort();
	}
	mac = &c[hydro_secretbox_SIVBYTES];
	COMPILER_ASSERT(hydro_secretbox_CONTEXTBYTES >= hydro_hash_CONTEXTBYTES);
	COMPILER_ASSERT(hydro_secretbox_KEYBYTES >= hydro_hash_KEYBYTES);
	hydro_hash_hash(probe, hydro_secretbox_PROBEBYTES, mac, hydro_secretbox_MACBYTES, ctx, key);
}

int
hydro_secretbox_probe_verify(const uint8_t probe[hydro_secretbox_PROBEBYTES], const uint8_t *c,
							 size_t c_len, const char ctx[hydro_secretbox_CONTEXTBYTES],
							 const uint8_t key[hydro_secretbox_KEYBYTES])
{
	uint8_t        computed_probe[hydro_secretbox_PROBEBYTES];
	const uint8_t *mac;

	if (c_len < hydro_secretbox_HEADERBYTES) {
		return -1;
	}
	mac = &c[hydro_secretbox_SIVBYTES];
	hydro_hash_hash(computed_probe, hydro_secretbox_PROBEBYTES, mac, hydro_secretbox_MACBYTES, ctx,
					key);
	if (hydro_equal(computed_probe, probe, hydro_secretbox_PROBEBYTES) == 1) {
		return 0;
	}
	hydro_memzero(computed_probe, hydro_secretbox_PROBEBYTES);
	return -1;
}

int
hydro_secretbox_encrypt(uint8_t *c, const void *m_, size_t mlen, uint64_t msg_id,
						const char    ctx[hydro_secretbox_CONTEXTBYTES],
						const uint8_t key[hydro_secretbox_KEYBYTES])
{
	uint8_t iv[hydro_secretbox_IVBYTES];

	hydro_random_buf(iv, sizeof iv);

	return hydro_secretbox_encrypt_iv(c, m_, mlen, msg_id, ctx, key, iv);
}

int
hydro_secretbox_decrypt(void *m_, const uint8_t *c, size_t clen, uint64_t msg_id,
						const char    ctx[hydro_secretbox_CONTEXTBYTES],
						const uint8_t key[hydro_secretbox_KEYBYTES])
{
	_hydro_attr_aligned_(16) uint32_t state[gimli_BLOCKBYTES / 4];
	uint32_t                          pub_mac[hydro_secretbox_MACBYTES / 4];
	uint8_t *                         buf = (uint8_t *) (void *) state;
	const uint8_t *                   siv;
	const uint8_t *                   mac;
	const uint8_t *                   ct;
	uint8_t *                         m = (uint8_t *) m_;
	size_t                            mlen;
	uint32_t                          cv;

	if (clen < hydro_secretbox_HEADERBYTES) {
		return -1;
	}
	siv = &c[0];
	mac = &c[hydro_secretbox_SIVBYTES];
	ct  = &c[hydro_secretbox_SIVBYTES + hydro_secretbox_MACBYTES];

	mlen = clen - hydro_secretbox_HEADERBYTES;
	memcpy(pub_mac, mac, sizeof pub_mac);
	COMPILER_ASSERT(hydro_secretbox_SIVBYTES == hydro_secretbox_IVBYTES);
	hydro_secretbox_setup(buf, msg_id, ctx, key, siv, gimli_TAG_KEY);
	hydro_secretbox_xor_dec(buf, m, ct, mlen);

	hydro_secretbox_final(buf, key, gimli_TAG_FINAL);
	COMPILER_ASSERT(hydro_secretbox_MACBYTES <= gimli_CAPACITY);
	COMPILER_ASSERT(gimli_RATE % 4 == 0);
	cv = hydro_mem_ct_cmp_u32(state + gimli_RATE / 4, pub_mac, hydro_secretbox_MACBYTES / 4);
	hydro_mem_ct_zero_u32(state, gimli_BLOCKBYTES / 4);
	if (cv != 0) {
		mem_zero(m, mlen);
		return -1;
	}
	return 0;
}


/*
 * Based on Michael Hamburg's STROBE reference implementation.
 * Copyright (c) 2015-2016 Cryptography Research, Inc.
 * MIT License (MIT)
 */

#if defined(__GNUC__) && defined(__SIZEOF_INT128__)
#define hydro_x25519_WBITS 64
#else
#define hydro_x25519_WBITS 32
#endif

#if hydro_x25519_WBITS == 64
typedef uint64_t    hydro_x25519_limb_t;
typedef __uint128_t hydro_x25519_dlimb_t;
typedef __int128_t  hydro_x25519_sdlimb_t;
#define hydro_x25519_eswap_limb(X) LOAD64_LE((const uint8_t *) &(X))
#define hydro_x25519_LIMB(x) x##ull
#elif hydro_x25519_WBITS == 32
typedef uint32_t hydro_x25519_limb_t;
typedef uint64_t hydro_x25519_dlimb_t;
typedef int64_t  hydro_x25519_sdlimb_t;
#define hydro_x25519_eswap_limb(X) LOAD32_LE((const uint8_t *) &(X))
#define hydro_x25519_LIMB(x) (uint32_t)(x##ull), (uint32_t)((x##ull) >> 32)
#else
#error "Need to know hydro_x25519_WBITS"
#endif

#define hydro_x25519_NLIMBS (256 / hydro_x25519_WBITS)
typedef hydro_x25519_limb_t hydro_x25519_fe[hydro_x25519_NLIMBS];

typedef hydro_x25519_limb_t hydro_x25519_scalar_t[hydro_x25519_NLIMBS];

static const hydro_x25519_limb_t hydro_x25519_MONTGOMERY_FACTOR =
	(hydro_x25519_limb_t) 0xd2b51da312547e1bull;

static const hydro_x25519_scalar_t hydro_x25519_sc_p = { hydro_x25519_LIMB(0x5812631a5cf5d3ed),
														 hydro_x25519_LIMB(0x14def9dea2f79cd6),
														 hydro_x25519_LIMB(0x0000000000000000),
														 hydro_x25519_LIMB(0x1000000000000000) };

static const hydro_x25519_scalar_t hydro_x25519_sc_r2 = { hydro_x25519_LIMB(0xa40611e3449c0f01),
														  hydro_x25519_LIMB(0xd00e1ba768859347),
														  hydro_x25519_LIMB(0xceec73d217f5be65),
														  hydro_x25519_LIMB(0x0399411b7c309a3d) };

static const uint8_t hydro_x25519_BASE_POINT[hydro_x25519_BYTES] = { 9 };

static const hydro_x25519_limb_t hydro_x25519_a24[1] = { 121665 };

static inline hydro_x25519_limb_t
hydro_x25519_umaal(hydro_x25519_limb_t *carry, hydro_x25519_limb_t acc, hydro_x25519_limb_t mand,
				   hydro_x25519_limb_t mier)
{
	hydro_x25519_dlimb_t tmp = (hydro_x25519_dlimb_t) mand * mier + acc + *carry;

	*carry = tmp >> hydro_x25519_WBITS;
	return (hydro_x25519_limb_t) tmp;
}

static inline hydro_x25519_limb_t
hydro_x25519_adc(hydro_x25519_limb_t *carry, hydro_x25519_limb_t acc, hydro_x25519_limb_t mand)
{
	hydro_x25519_dlimb_t total = (hydro_x25519_dlimb_t) *carry + acc + mand;

	*carry = total >> hydro_x25519_WBITS;
	return (hydro_x25519_limb_t) total;
}

static inline hydro_x25519_limb_t
hydro_x25519_adc0(hydro_x25519_limb_t *carry, hydro_x25519_limb_t acc)
{
	hydro_x25519_dlimb_t total = (hydro_x25519_dlimb_t) *carry + acc;

	*carry = total >> hydro_x25519_WBITS;
	return (hydro_x25519_limb_t) total;
}

static void
hydro_x25519_propagate(hydro_x25519_fe x, hydro_x25519_limb_t over)
{
	hydro_x25519_limb_t carry;
	int                 i;

	over = x[hydro_x25519_NLIMBS - 1] >> (hydro_x25519_WBITS - 1) | over << 1;
	x[hydro_x25519_NLIMBS - 1] &= ~((hydro_x25519_limb_t) 1 << (hydro_x25519_WBITS - 1));
	carry = over * 19;
	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		x[i] = hydro_x25519_adc0(&carry, x[i]);
	}
}

static void
hydro_x25519_add(hydro_x25519_fe out, const hydro_x25519_fe a, const hydro_x25519_fe b)
{
	hydro_x25519_limb_t carry = 0;
	int                 i;

	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		out[i] = hydro_x25519_adc(&carry, a[i], b[i]);
	}
	hydro_x25519_propagate(out, carry);
}

static void
hydro_x25519_sub(hydro_x25519_fe out, const hydro_x25519_fe a, const hydro_x25519_fe b)
{
	hydro_x25519_sdlimb_t carry = -38;
	int                   i;

	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		out[i] = (hydro_x25519_limb_t)(carry = carry + a[i] - b[i]);
		carry >>= hydro_x25519_WBITS;
	}
	hydro_x25519_propagate(out, (hydro_x25519_limb_t)(1 + carry));
}

static void
hydro_x25519_swapin(hydro_x25519_limb_t *x, const uint8_t *in)
{
	int i;

	memcpy(x, in, sizeof(hydro_x25519_fe));
	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		x[i] = hydro_x25519_eswap_limb(x[i]);
	}
}

static void
hydro_x25519_swapout(uint8_t *out, hydro_x25519_limb_t *x)
{
	int i;

	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		x[i] = hydro_x25519_eswap_limb(x[i]);
	}
	memcpy(out, x, sizeof(hydro_x25519_fe));
}

static void
hydro_x25519_mul(hydro_x25519_fe out, const hydro_x25519_fe a, const hydro_x25519_fe b, int nb)
{
	hydro_x25519_limb_t accum[2 * hydro_x25519_NLIMBS] = { 0 };
	hydro_x25519_limb_t carry2;
	int                 i, j;

	for (i = 0; i < nb; i++) {
		carry2                   = 0;
		hydro_x25519_limb_t mand = b[i];
		for (j = 0; j < hydro_x25519_NLIMBS; j++) {
			accum[i + j] = hydro_x25519_umaal(&carry2, accum[i + j], mand, a[j]);
		}
		accum[i + j] = carry2;
	}
	carry2 = 0;
	for (j = 0; j < hydro_x25519_NLIMBS; j++) {
		const hydro_x25519_limb_t mand = 38;

		out[j] = hydro_x25519_umaal(&carry2, accum[j], mand, accum[j + hydro_x25519_NLIMBS]);
	}
	hydro_x25519_propagate(out, carry2);
}

static void
hydro_x25519_sqr(hydro_x25519_fe out, const hydro_x25519_fe a)
{
	hydro_x25519_mul(out, a, a, hydro_x25519_NLIMBS);
}

static void
hydro_x25519_mul1(hydro_x25519_fe out, const hydro_x25519_fe a)
{
	hydro_x25519_mul(out, a, out, hydro_x25519_NLIMBS);
}

static void
hydro_x25519_sqr1(hydro_x25519_fe a)
{
	hydro_x25519_mul1(a, a);
}

static void
hydro_x25519_condswap(hydro_x25519_limb_t a[2 * hydro_x25519_NLIMBS],
					  hydro_x25519_limb_t b[2 * hydro_x25519_NLIMBS], hydro_x25519_limb_t doswap)
{
	int i;

	for (i = 0; i < 2 * hydro_x25519_NLIMBS; i++) {
		hydro_x25519_limb_t xorv = (a[i] ^ b[i]) & doswap;
		a[i] ^= xorv;
		b[i] ^= xorv;
	}
}

static int
hydro_x25519_canon(hydro_x25519_fe x)
{
	hydro_x25519_sdlimb_t carry;
	hydro_x25519_limb_t   carry0 = 19;
	hydro_x25519_limb_t   res;
	int                   i;

	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		x[i] = hydro_x25519_adc0(&carry0, x[i]);
	}
	hydro_x25519_propagate(x, carry0);
	carry = -19;
	res   = 0;
	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		res |= x[i] = (hydro_x25519_limb_t)(carry += x[i]);
		carry >>= hydro_x25519_WBITS;
	}
	return ((hydro_x25519_dlimb_t) res - 1) >> hydro_x25519_WBITS;
}

static void
hydro_x25519_ladder_part1(hydro_x25519_fe xs[5])
{
	hydro_x25519_limb_t *x2 = xs[0], *z2 = xs[1], *x3 = xs[2], *z3 = xs[3], *t1 = xs[4];

	hydro_x25519_add(t1, x2, z2);              // t1 = A
	hydro_x25519_sub(z2, x2, z2);              // z2 = B
	hydro_x25519_add(x2, x3, z3);              // x2 = C
	hydro_x25519_sub(z3, x3, z3);              // z3 = D
	hydro_x25519_mul1(z3, t1);                 // z3 = DA
	hydro_x25519_mul1(x2, z2);                 // x3 = BC
	hydro_x25519_add(x3, z3, x2);              // x3 = DA+CB
	hydro_x25519_sub(z3, z3, x2);              // z3 = DA-CB
	hydro_x25519_sqr1(t1);                     // t1 = AA
	hydro_x25519_sqr1(z2);                     // z2 = BB
	hydro_x25519_sub(x2, t1, z2);              // x2 = E = AA-BB
	hydro_x25519_mul(z2, x2, hydro_x25519_a24, // z2 = E*a24
					 sizeof(hydro_x25519_a24) / sizeof(hydro_x25519_a24[0]));
	hydro_x25519_add(z2, z2, t1); // z2 = E*a24 + AA
}

static void
hydro_x25519_ladder_part2(hydro_x25519_fe xs[5], const hydro_x25519_fe x1)
{
	hydro_x25519_limb_t *x2 = xs[0], *z2 = xs[1], *x3 = xs[2], *z3 = xs[3], *t1 = xs[4];

	hydro_x25519_sqr1(z3);        // z3 = (DA-CB)^2
	hydro_x25519_mul1(z3, x1);    // z3 = x1 * (DA-CB)^2
	hydro_x25519_sqr1(x3);        // x3 = (DA+CB)^2
	hydro_x25519_mul1(z2, x2);    // z2 = AA*(E*a24+AA)
	hydro_x25519_sub(x2, t1, x2); // x2 = BB again
	hydro_x25519_mul1(x2, t1);    // x2 = AA*BB
}

static void
hydro_x25519_core(hydro_x25519_fe xs[5], const uint8_t scalar[hydro_x25519_BYTES],
				  const uint8_t *x1, bool clamp)
{
	hydro_x25519_limb_t  swap;
	hydro_x25519_limb_t *x2 = xs[0], *x3 = xs[2], *z3 = xs[3];
	hydro_x25519_fe      x1i;
	int                  i;

	hydro_x25519_swapin(x1i, x1);
	x1   = (const uint8_t *) x1i;
	swap = 0;
	mem_zero(xs, 4 * sizeof(hydro_x25519_fe));
	x2[0] = z3[0] = 1;
	memcpy(x3, x1, sizeof(hydro_x25519_fe));
	for (i = 255; i >= 0; i--) {
		uint8_t             bytei = scalar[i / 8];
		hydro_x25519_limb_t doswap;
		hydro_x25519_fe     x1_dup;

		if (clamp) {
			if (i / 8 == 0) {
				bytei &= ~7;
			} else if (i / 8 == hydro_x25519_BYTES - 1) {
				bytei &= 0x7F;
				bytei |= 0x40;
			}
		}
		doswap = 1U + ~(hydro_x25519_limb_t)((bytei >> (i % 8)) & 1);
		hydro_x25519_condswap(x2, x3, swap ^ doswap);
		swap = doswap;
		hydro_x25519_ladder_part1(xs);
		memcpy(x1_dup, x1, sizeof x1_dup);
		hydro_x25519_ladder_part2(xs, x1_dup);
	}
	hydro_x25519_condswap(x2, x3, swap);
}

static int
hydro_x25519_scalarmult(uint8_t       out[hydro_x25519_BYTES],
						const uint8_t scalar[hydro_x25519_SECRETKEYBYTES],
						const uint8_t x1[hydro_x25519_PUBLICKEYBYTES], bool clamp)
{
	hydro_x25519_fe      xs[5];
	hydro_x25519_limb_t *x2, *z2, *z3;
	hydro_x25519_limb_t *prev;
	int                  i;
	int                  ret;

	hydro_x25519_core(xs, scalar, x1, clamp);

	/* Precomputed inversion chain */
	x2   = xs[0];
	z2   = xs[1];
	z3   = xs[3];
	prev = z2;

	/* Raise to the p-2 = 0x7f..ffeb */
	for (i = 253; i >= 0; i--) {
		hydro_x25519_sqr(z3, prev);
		prev = z3;
		if (i >= 8 || (0xeb >> i & 1)) {
			hydro_x25519_mul1(z3, z2);
		}
	}

	/* Here prev = z3 */
	/* x2 /= z2 */
	hydro_x25519_mul1(x2, z3);
	ret = hydro_x25519_canon(x2);
	hydro_x25519_swapout(out, x2);

	if (clamp == 0) {
		return 0;
	}
	return ret;
}

static inline int
hydro_x25519_scalarmult_base(uint8_t       pk[hydro_x25519_PUBLICKEYBYTES],
							 const uint8_t sk[hydro_x25519_SECRETKEYBYTES])
{
	return hydro_x25519_scalarmult(pk, sk, hydro_x25519_BASE_POINT, 1);
}

static inline void
hydro_x25519_scalarmult_base_uniform(uint8_t       pk[hydro_x25519_PUBLICKEYBYTES],
									 const uint8_t sk[hydro_x25519_SECRETKEYBYTES])
{
	if (hydro_x25519_scalarmult(pk, sk, hydro_x25519_BASE_POINT, 0) != 0) {
		abort();
	}
}

static void
hydro_x25519_sc_montmul(hydro_x25519_scalar_t out, const hydro_x25519_scalar_t a,
						const hydro_x25519_scalar_t b)
{
	hydro_x25519_limb_t hic = 0;
	int                 i, j;

	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		hydro_x25519_limb_t carry = 0, carry2 = 0, mand = a[i],
							mand2 = hydro_x25519_MONTGOMERY_FACTOR;

		for (j = 0; j < hydro_x25519_NLIMBS; j++) {
			hydro_x25519_limb_t acc = out[j];

			acc = hydro_x25519_umaal(&carry, acc, mand, b[j]);
			if (j == 0) {
				mand2 *= acc;
			}
			acc = hydro_x25519_umaal(&carry2, acc, mand2, hydro_x25519_sc_p[j]);
			if (j > 0) {
				out[j - 1] = acc;
			}
		}

		/* Add two carry registers and high carry */
		out[hydro_x25519_NLIMBS - 1] = hydro_x25519_adc(&hic, carry, carry2);
	}

	/* Reduce */
	hydro_x25519_sdlimb_t scarry = 0;
	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		out[i] = (hydro_x25519_limb_t)(scarry = scarry + out[i] - hydro_x25519_sc_p[i]);
		scarry >>= hydro_x25519_WBITS;
	}
	hydro_x25519_limb_t need_add = (hydro_x25519_limb_t) - (scarry + hic);

	hydro_x25519_limb_t carry = 0;
	for (i = 0; i < hydro_x25519_NLIMBS; i++) {
		out[i] = hydro_x25519_umaal(&carry, out[i], need_add, hydro_x25519_sc_p[i]);
	}
}


#define hydro_kx_AEAD_KEYBYTES hydro_hash_KEYBYTES
#define hydro_kx_AEAD_MACBYTES 16

#define hydro_kx_CONTEXT "hydro_kx"

static void
hydro_kx_aead_init(uint8_t aead_state[gimli_BLOCKBYTES], uint8_t k[hydro_kx_AEAD_KEYBYTES],
				   hydro_kx_state *state)
{
	static const uint8_t prefix[] = { 6, 'k', 'x', 'x', '2', '5', '6', 0 };

	hydro_hash_final(&state->h_st, k, hydro_kx_AEAD_KEYBYTES);

	mem_zero(aead_state + sizeof prefix, gimli_BLOCKBYTES - sizeof prefix);
	memcpy(aead_state, prefix, sizeof prefix);
	gimli_core_u8(aead_state, gimli_TAG_HEADER);

	COMPILER_ASSERT(hydro_kx_AEAD_KEYBYTES == 2 * gimli_RATE);
	mem_xor(aead_state, k, gimli_RATE);
	gimli_core_u8(aead_state, gimli_TAG_KEY);
	mem_xor(aead_state, k + gimli_RATE, gimli_RATE);
	gimli_core_u8(aead_state, gimli_TAG_KEY);
}

static void
hydro_kx_aead_final(uint8_t *aead_state, const uint8_t key[hydro_kx_AEAD_KEYBYTES])
{
	COMPILER_ASSERT(hydro_kx_AEAD_KEYBYTES == gimli_CAPACITY);
	mem_xor(aead_state + gimli_RATE, key, hydro_kx_AEAD_KEYBYTES);
	gimli_core_u8(aead_state, gimli_TAG_FINAL);
	mem_xor(aead_state + gimli_RATE, key, hydro_kx_AEAD_KEYBYTES);
	gimli_core_u8(aead_state, gimli_TAG_FINAL);
}

static void
hydro_kx_aead_xor_enc(uint8_t aead_state[gimli_BLOCKBYTES], uint8_t *out, const uint8_t *in,
					  size_t inlen)
{
	size_t i;
	size_t leftover;

	for (i = 0; i < inlen / gimli_RATE; i++) {
		mem_xor2(&out[i * gimli_RATE], &in[i * gimli_RATE], aead_state, gimli_RATE);
		memcpy(aead_state, &out[i * gimli_RATE], gimli_RATE);
		gimli_core_u8(aead_state, gimli_TAG_PAYLOAD);
	}
	leftover = inlen % gimli_RATE;
	if (leftover != 0) {
		mem_xor2(&out[i * gimli_RATE], &in[i * gimli_RATE], aead_state, leftover);
		mem_cpy(aead_state, &out[i * gimli_RATE], leftover);
	}
	gimli_pad_u8(aead_state, leftover, gimli_DOMAIN_AEAD);
	gimli_core_u8(aead_state, gimli_TAG_PAYLOAD);
}

static void
hydro_kx_aead_xor_dec(uint8_t aead_state[gimli_BLOCKBYTES], uint8_t *out, const uint8_t *in,
					  size_t inlen)
{
	size_t i;
	size_t leftover;

	for (i = 0; i < inlen / gimli_RATE; i++) {
		mem_xor2(&out[i * gimli_RATE], &in[i * gimli_RATE], aead_state, gimli_RATE);
		memcpy(aead_state, &in[i * gimli_RATE], gimli_RATE);
		gimli_core_u8(aead_state, gimli_TAG_PAYLOAD);
	}
	leftover = inlen % gimli_RATE;
	if (leftover != 0) {
		mem_xor2(&out[i * gimli_RATE], &in[i * gimli_RATE], aead_state, leftover);
		mem_cpy(aead_state, &in[i * gimli_RATE], leftover);
	}
	gimli_pad_u8(aead_state, leftover, gimli_DOMAIN_AEAD);
	gimli_core_u8(aead_state, gimli_TAG_PAYLOAD);
}

static void
hydro_kx_aead_encrypt(hydro_kx_state *state, uint8_t *c, const uint8_t *m, size_t mlen)
{
	_hydro_attr_aligned_(16) uint8_t aead_state[gimli_BLOCKBYTES];
	uint8_t                          k[hydro_kx_AEAD_KEYBYTES];
	uint8_t *                        mac = &c[0];
	uint8_t *                        ct  = &c[hydro_kx_AEAD_MACBYTES];

	hydro_kx_aead_init(aead_state, k, state);
	hydro_kx_aead_xor_enc(aead_state, ct, m, mlen);
	hydro_kx_aead_final(aead_state, k);
	COMPILER_ASSERT(hydro_kx_AEAD_MACBYTES <= gimli_CAPACITY);
	memcpy(mac, aead_state + gimli_RATE, hydro_kx_AEAD_MACBYTES);
	hydro_hash_update(&state->h_st, c, mlen + hydro_kx_AEAD_MACBYTES);
}

static int hydro_kx_aead_decrypt(hydro_kx_state *state, uint8_t *m, const uint8_t *c,
								 size_t clen) _hydro_attr_warn_unused_result_;

static int
hydro_kx_aead_decrypt(hydro_kx_state *state, uint8_t *m, const uint8_t *c, size_t clen)
{
	_hydro_attr_aligned_(16) uint32_t int_state[gimli_BLOCKBYTES / 4];
	uint32_t                          pub_mac[hydro_kx_AEAD_MACBYTES / 4];
	uint8_t                           k[hydro_kx_AEAD_KEYBYTES];
	uint8_t *                         aead_state = (uint8_t *) (void *) int_state;
	const uint8_t *                   mac;
	const uint8_t *                   ct;
	size_t                            mlen;
	uint32_t                          cv;

	if (clen < hydro_kx_AEAD_MACBYTES) {
		return -1;
	}
	mac  = &c[0];
	ct   = &c[hydro_kx_AEAD_MACBYTES];
	mlen = clen - hydro_kx_AEAD_MACBYTES;
	memcpy(pub_mac, mac, sizeof pub_mac);
	hydro_kx_aead_init(aead_state, k, state);
	hydro_hash_update(&state->h_st, c, clen);
	hydro_kx_aead_xor_dec(aead_state, m, ct, mlen);
	hydro_kx_aead_final(aead_state, k);
	COMPILER_ASSERT(hydro_kx_AEAD_MACBYTES <= gimli_CAPACITY);
	COMPILER_ASSERT(gimli_RATE % 4 == 0);
	cv = hydro_mem_ct_cmp_u32(int_state + gimli_RATE / 4, pub_mac, hydro_kx_AEAD_MACBYTES / 4);
	hydro_mem_ct_zero_u32(int_state, gimli_BLOCKBYTES / 4);
	if (cv != 0) {
		mem_zero(m, mlen);
		return -1;
	}
	return 0;
}

/* -- */

void
hydro_kx_keygen(hydro_kx_keypair *static_kp)
{
	hydro_random_buf(static_kp->sk, hydro_kx_SECRETKEYBYTES);
	if (hydro_x25519_scalarmult_base(static_kp->pk, static_kp->sk) != 0) {
		abort();
	}
}

void
hydro_kx_keygen_deterministic(hydro_kx_keypair *static_kp, const uint8_t seed[hydro_kx_SEEDBYTES])
{
	COMPILER_ASSERT(hydro_kx_SEEDBYTES >= hydro_random_SEEDBYTES);
	hydro_random_buf_deterministic(static_kp->sk, hydro_kx_SECRETKEYBYTES, seed);
	if (hydro_x25519_scalarmult_base(static_kp->pk, static_kp->sk) != 0) {
		abort();
	}
}

static void
hydro_kx_init_state(hydro_kx_state *state, const char *name)
{
	mem_zero(state, sizeof *state);
	hydro_hash_init(&state->h_st, hydro_kx_CONTEXT, NULL);
	hydro_hash_update(&state->h_st, name, strlen(name));
	hydro_hash_final(&state->h_st, NULL, 0);
}

static void
hydro_kx_final(hydro_kx_state *state, uint8_t session_k1[hydro_kx_SESSIONKEYBYTES],
			   uint8_t session_k2[hydro_kx_SESSIONKEYBYTES])
{
	uint8_t kdf_key[hydro_kdf_KEYBYTES];

	hydro_hash_final(&state->h_st, kdf_key, sizeof kdf_key);
	hydro_kdf_derive_from_key(session_k1, hydro_kx_SESSIONKEYBYTES, 0, hydro_kx_CONTEXT, kdf_key);
	hydro_kdf_derive_from_key(session_k2, hydro_kx_SESSIONKEYBYTES, 1, hydro_kx_CONTEXT, kdf_key);
}

static int
hydro_kx_dh(hydro_kx_state *state, const uint8_t sk[hydro_x25519_SECRETKEYBYTES],
			const uint8_t pk[hydro_x25519_PUBLICKEYBYTES])
{
	uint8_t dh_result[hydro_x25519_BYTES];

	if (hydro_x25519_scalarmult(dh_result, sk, pk, 1) != 0) {
		return -1;
	}
	hydro_hash_update(&state->h_st, dh_result, hydro_x25519_BYTES);

	return 0;
}

static void
hydro_kx_eph_keygen(hydro_kx_state *state, hydro_kx_keypair *kp)
{
	hydro_kx_keygen(kp);
	hydro_hash_update(&state->h_st, kp->pk, sizeof kp->pk);
}

/* NOISE_N */

int
hydro_kx_n_1(hydro_kx_session_keypair *kp, uint8_t packet1[hydro_kx_N_PACKET1BYTES],
			 const uint8_t psk[hydro_kx_PSKBYTES],
			 const uint8_t peer_static_pk[hydro_kx_PUBLICKEYBYTES])
{
	hydro_kx_state state;
	uint8_t *      packet1_eph_pk = &packet1[0];
	uint8_t *      packet1_mac    = &packet1[hydro_kx_PUBLICKEYBYTES];

	if (psk == NULL) {
		psk = zero;
	}
	hydro_kx_init_state(&state, "Noise_Npsk0_hydro1");
	hydro_hash_update(&state.h_st, peer_static_pk, hydro_x25519_PUBLICKEYBYTES);

	hydro_hash_update(&state.h_st, psk, hydro_kx_PSKBYTES);
	hydro_kx_eph_keygen(&state, &state.eph_kp);
	if (hydro_kx_dh(&state, state.eph_kp.sk, peer_static_pk) != 0) {
		return -1;
	}
	hydro_kx_aead_encrypt(&state, packet1_mac, NULL, 0);
	memcpy(packet1_eph_pk, state.eph_kp.pk, sizeof state.eph_kp.pk);

	hydro_kx_final(&state, kp->rx, kp->tx);

	return 0;
}

int
hydro_kx_n_2(hydro_kx_session_keypair *kp, const uint8_t packet1[hydro_kx_N_PACKET1BYTES],
			 const uint8_t psk[hydro_kx_PSKBYTES], const hydro_kx_keypair *static_kp)
{
	hydro_kx_state state;
	const uint8_t *peer_eph_pk = &packet1[0];
	const uint8_t *packet1_mac = &packet1[hydro_kx_PUBLICKEYBYTES];

	if (psk == NULL) {
		psk = zero;
	}
	hydro_kx_init_state(&state, "Noise_Npsk0_hydro1");
	hydro_hash_update(&state.h_st, static_kp->pk, hydro_kx_PUBLICKEYBYTES);

	hydro_hash_update(&state.h_st, psk, hydro_kx_PSKBYTES);
	hydro_hash_update(&state.h_st, peer_eph_pk, hydro_x25519_PUBLICKEYBYTES);
	if (hydro_kx_dh(&state, static_kp->sk, peer_eph_pk) != 0 ||
		hydro_kx_aead_decrypt(&state, NULL, packet1_mac, hydro_kx_AEAD_MACBYTES) != 0) {
		return -1;
	}
	hydro_kx_final(&state, kp->tx, kp->rx);

	return 0;
}

/* NOISE_KK */

int
hydro_kx_kk_1(hydro_kx_state *state, uint8_t packet1[hydro_kx_KK_PACKET1BYTES],
			  const uint8_t           peer_static_pk[hydro_kx_PUBLICKEYBYTES],
			  const hydro_kx_keypair *static_kp)
{
	uint8_t *packet1_eph_pk = &packet1[0];
	uint8_t *packet1_mac    = &packet1[hydro_kx_PUBLICKEYBYTES];

	hydro_kx_init_state(state, "Noise_KK_hydro1");
	hydro_hash_update(&state->h_st, static_kp->pk, hydro_kx_PUBLICKEYBYTES);
	hydro_hash_update(&state->h_st, peer_static_pk, hydro_kx_PUBLICKEYBYTES);

	hydro_kx_eph_keygen(state, &state->eph_kp);
	if (hydro_kx_dh(state, state->eph_kp.sk, peer_static_pk) != 0 ||
		hydro_kx_dh(state, static_kp->sk, peer_static_pk) != 0) {
		return -1;
	}
	hydro_kx_aead_encrypt(state, packet1_mac, NULL, 0);
	memcpy(packet1_eph_pk, state->eph_kp.pk, sizeof state->eph_kp.pk);

	return 0;
}

int
hydro_kx_kk_2(hydro_kx_session_keypair *kp, uint8_t packet2[hydro_kx_KK_PACKET2BYTES],
			  const uint8_t           packet1[hydro_kx_KK_PACKET1BYTES],
			  const uint8_t           peer_static_pk[hydro_kx_PUBLICKEYBYTES],
			  const hydro_kx_keypair *static_kp)
{
	hydro_kx_state state;
	const uint8_t *peer_eph_pk    = &packet1[0];
	const uint8_t *packet1_mac    = &packet1[hydro_kx_PUBLICKEYBYTES];
	uint8_t *      packet2_eph_pk = &packet2[0];
	uint8_t *      packet2_mac    = &packet2[hydro_kx_PUBLICKEYBYTES];

	hydro_kx_init_state(&state, "Noise_KK_hydro1");
	hydro_hash_update(&state.h_st, peer_static_pk, hydro_kx_PUBLICKEYBYTES);
	hydro_hash_update(&state.h_st, static_kp->pk, hydro_kx_PUBLICKEYBYTES);

	hydro_hash_update(&state.h_st, peer_eph_pk, hydro_kx_PUBLICKEYBYTES);
	if (hydro_kx_dh(&state, static_kp->sk, peer_eph_pk) != 0 ||
		hydro_kx_dh(&state, static_kp->sk, peer_static_pk) != 0 ||
		hydro_kx_aead_decrypt(&state, NULL, packet1_mac, hydro_kx_AEAD_MACBYTES) != 0) {
		return -1;
	}

	hydro_kx_eph_keygen(&state, &state.eph_kp);
	if (hydro_kx_dh(&state, state.eph_kp.sk, peer_eph_pk) != 0 ||
		hydro_kx_dh(&state, state.eph_kp.sk, peer_static_pk) != 0) {
		return -1;
	}
	hydro_kx_aead_encrypt(&state, packet2_mac, NULL, 0);
	hydro_kx_final(&state, kp->tx, kp->rx);
	memcpy(packet2_eph_pk, state.eph_kp.pk, sizeof state.eph_kp.pk);

	return 0;
}

int
hydro_kx_kk_3(hydro_kx_state *state, hydro_kx_session_keypair *kp,
			  const uint8_t packet2[hydro_kx_KK_PACKET2BYTES], const hydro_kx_keypair *static_kp)
{
	const uint8_t *peer_eph_pk = packet2;
	const uint8_t *packet2_mac = &packet2[hydro_kx_PUBLICKEYBYTES];

	hydro_hash_update(&state->h_st, peer_eph_pk, hydro_kx_PUBLICKEYBYTES);
	if (hydro_kx_dh(state, state->eph_kp.sk, peer_eph_pk) != 0 ||
		hydro_kx_dh(state, static_kp->sk, peer_eph_pk) != 0) {
		return -1;
	}

	if (hydro_kx_aead_decrypt(state, NULL, packet2_mac, hydro_kx_AEAD_MACBYTES) != 0) {
		return -1;
	}
	hydro_kx_final(state, kp->rx, kp->tx);

	return 0;
}

/* NOISE_XX */

int
hydro_kx_xx_1(hydro_kx_state *state, uint8_t packet1[hydro_kx_XX_PACKET1BYTES],
			  const uint8_t psk[hydro_kx_PSKBYTES])
{
	uint8_t *packet1_eph_pk = &packet1[0];
	uint8_t *packet1_mac    = &packet1[hydro_kx_PUBLICKEYBYTES];

	if (psk == NULL) {
		psk = zero;
	}
	hydro_kx_init_state(state, "Noise_XXpsk0+psk3_hydro1");

	hydro_kx_eph_keygen(state, &state->eph_kp);
	hydro_hash_update(&state->h_st, psk, hydro_kx_PSKBYTES);
	memcpy(packet1_eph_pk, state->eph_kp.pk, sizeof state->eph_kp.pk);
	hydro_kx_aead_encrypt(state, packet1_mac, NULL, 0);

	return 0;
}

int
hydro_kx_xx_2(hydro_kx_state *state, uint8_t packet2[hydro_kx_XX_PACKET2BYTES],
			  const uint8_t packet1[hydro_kx_XX_PACKET1BYTES], const uint8_t psk[hydro_kx_PSKBYTES],
			  const hydro_kx_keypair *static_kp)
{
	const uint8_t *peer_eph_pk           = &packet1[0];
	const uint8_t *packet1_mac           = &packet1[hydro_kx_PUBLICKEYBYTES];
	uint8_t *      packet2_eph_pk        = &packet2[0];
	uint8_t *      packet2_enc_static_pk = &packet2[hydro_kx_PUBLICKEYBYTES];
	uint8_t *      packet2_mac =
		&packet2[hydro_kx_PUBLICKEYBYTES + hydro_kx_PUBLICKEYBYTES + hydro_kx_AEAD_MACBYTES];

	if (psk == NULL) {
		psk = zero;
	}
	hydro_kx_init_state(state, "Noise_XXpsk0+psk3_hydro1");

	hydro_hash_update(&state->h_st, peer_eph_pk, hydro_kx_PUBLICKEYBYTES);
	hydro_hash_update(&state->h_st, psk, hydro_kx_PSKBYTES);
	if (hydro_kx_aead_decrypt(state, NULL, packet1_mac, hydro_kx_AEAD_MACBYTES) != 0) {
		return -1;
	}

	hydro_kx_eph_keygen(state, &state->eph_kp);
	if (hydro_kx_dh(state, state->eph_kp.sk, peer_eph_pk) != 0) {
		return -1;
	}
	hydro_kx_aead_encrypt(state, packet2_enc_static_pk, static_kp->pk, sizeof static_kp->pk);
	if (hydro_kx_dh(state, static_kp->sk, peer_eph_pk) != 0) {
		return -1;
	}
	hydro_kx_aead_encrypt(state, packet2_mac, NULL, 0);

	memcpy(packet2_eph_pk, state->eph_kp.pk, sizeof state->eph_kp.pk);

	return 0;
}

int
hydro_kx_xx_3(hydro_kx_state *state, hydro_kx_session_keypair *kp,
			  uint8_t       packet3[hydro_kx_XX_PACKET3BYTES],
			  uint8_t       peer_static_pk[hydro_kx_PUBLICKEYBYTES],
			  const uint8_t packet2[hydro_kx_XX_PACKET2BYTES], const uint8_t psk[hydro_kx_PSKBYTES],
			  const hydro_kx_keypair *static_kp)
{
	uint8_t        peer_static_pk_[hydro_kx_PUBLICKEYBYTES];
	const uint8_t *peer_eph_pk        = &packet2[0];
	const uint8_t *peer_enc_static_pk = &packet2[hydro_kx_PUBLICKEYBYTES];
	const uint8_t *packet2_mac =
		&packet2[hydro_kx_PUBLICKEYBYTES + hydro_kx_PUBLICKEYBYTES + hydro_kx_AEAD_MACBYTES];
	uint8_t *packet3_enc_static_pk = &packet3[0];
	uint8_t *packet3_mac           = &packet3[hydro_kx_PUBLICKEYBYTES + hydro_kx_AEAD_MACBYTES];

	if (psk == NULL) {
		psk = zero;
	}
	if (peer_static_pk == NULL) {
		peer_static_pk = peer_static_pk_;
	}
	hydro_hash_update(&state->h_st, peer_eph_pk, hydro_kx_PUBLICKEYBYTES);
	if (hydro_kx_dh(state, state->eph_kp.sk, peer_eph_pk) != 0 ||
		hydro_kx_aead_decrypt(state, peer_static_pk, peer_enc_static_pk,
							  hydro_kx_PUBLICKEYBYTES + hydro_kx_AEAD_MACBYTES) != 0 ||
		hydro_kx_dh(state, state->eph_kp.sk, peer_static_pk) != 0 ||
		hydro_kx_aead_decrypt(state, NULL, packet2_mac, hydro_kx_AEAD_MACBYTES) != 0) {
		return -1;
	}

	hydro_kx_aead_encrypt(state, packet3_enc_static_pk, static_kp->pk, sizeof static_kp->pk);
	if (hydro_kx_dh(state, static_kp->sk, peer_eph_pk) != 0) {
		return -1;
	}
	hydro_hash_update(&state->h_st, psk, hydro_kx_PSKBYTES);
	hydro_kx_aead_encrypt(state, packet3_mac, NULL, 0);
	hydro_kx_final(state, kp->rx, kp->tx);

	return 0;
}

int
hydro_kx_xx_4(hydro_kx_state *state, hydro_kx_session_keypair *kp,
			  uint8_t       peer_static_pk[hydro_kx_PUBLICKEYBYTES],
			  const uint8_t packet3[hydro_kx_XX_PACKET3BYTES], const uint8_t psk[hydro_kx_PSKBYTES])
{
	uint8_t        peer_static_pk_[hydro_kx_PUBLICKEYBYTES];
	const uint8_t *peer_enc_static_pk = &packet3[0];
	const uint8_t *packet3_mac        = &packet3[hydro_kx_PUBLICKEYBYTES + hydro_kx_AEAD_MACBYTES];

	if (psk == NULL) {
		psk = zero;
	}
	if (peer_static_pk == NULL) {
		peer_static_pk = peer_static_pk_;
	}
	if (hydro_kx_aead_decrypt(state, peer_static_pk, peer_enc_static_pk,
							  hydro_kx_PUBLICKEYBYTES + hydro_kx_AEAD_MACBYTES) != 0 ||
		hydro_kx_dh(state, state->eph_kp.sk, peer_static_pk) != 0) {
		return -1;
	}
	hydro_hash_update(&state->h_st, psk, hydro_kx_PSKBYTES);
	if (hydro_kx_aead_decrypt(state, NULL, packet3_mac, hydro_kx_AEAD_MACBYTES) != 0) {
		return -1;
	}
	hydro_kx_final(state, kp->tx, kp->rx);

	return 0;
}

/* NOISE_NK */

int
hydro_kx_nk_1(hydro_kx_state *state, uint8_t packet1[hydro_kx_NK_PACKET1BYTES],
			  const uint8_t psk[hydro_kx_PSKBYTES],
			  const uint8_t peer_static_pk[hydro_kx_PUBLICKEYBYTES])
{
	uint8_t *packet1_eph_pk = &packet1[0];
	uint8_t *packet1_mac    = &packet1[hydro_kx_PUBLICKEYBYTES];

	if (psk == NULL) {
		psk = zero;
	}
	hydro_kx_init_state(state, "Noise_NKpsk0_hydro1");
	hydro_hash_update(&state->h_st, peer_static_pk, hydro_x25519_PUBLICKEYBYTES);

	hydro_hash_update(&state->h_st, psk, hydro_kx_PSKBYTES);
	hydro_kx_eph_keygen(state, &state->eph_kp);
	if (hydro_kx_dh(state, state->eph_kp.sk, peer_static_pk) != 0) {
		return -1;
	}
	hydro_kx_aead_encrypt(state, packet1_mac, NULL, 0);
	memcpy(packet1_eph_pk, state->eph_kp.pk, sizeof state->eph_kp.pk);

	return 0;
}

int
hydro_kx_nk_2(hydro_kx_session_keypair *kp, uint8_t packet2[hydro_kx_NK_PACKET2BYTES],
			  const uint8_t packet1[hydro_kx_NK_PACKET1BYTES], const uint8_t psk[hydro_kx_PSKBYTES],
			  const hydro_kx_keypair *static_kp)
{
	hydro_kx_state state;
	const uint8_t *peer_eph_pk    = &packet1[0];
	const uint8_t *packet1_mac    = &packet1[hydro_kx_PUBLICKEYBYTES];
	uint8_t *      packet2_eph_pk = &packet2[0];
	uint8_t *      packet2_mac    = &packet2[hydro_kx_PUBLICKEYBYTES];

	if (psk == NULL) {
		psk = zero;
	}
	hydro_kx_init_state(&state, "Noise_NKpsk0_hydro1");
	hydro_hash_update(&state.h_st, static_kp->pk, hydro_kx_PUBLICKEYBYTES);

	hydro_hash_update(&state.h_st, psk, hydro_kx_PSKBYTES);
	hydro_hash_update(&state.h_st, peer_eph_pk, hydro_x25519_PUBLICKEYBYTES);
	if (hydro_kx_dh(&state, static_kp->sk, peer_eph_pk) != 0 ||
		hydro_kx_aead_decrypt(&state, NULL, packet1_mac, hydro_kx_AEAD_MACBYTES) != 0) {
		return -1;
	}

	hydro_kx_eph_keygen(&state, &state.eph_kp);
	if (hydro_kx_dh(&state, state.eph_kp.sk, peer_eph_pk) != 0) {
		return -1;
	}
	hydro_kx_aead_encrypt(&state, packet2_mac, NULL, 0);
	hydro_kx_final(&state, kp->tx, kp->rx);
	memcpy(packet2_eph_pk, state.eph_kp.pk, sizeof state.eph_kp.pk);

	return 0;
}

int
hydro_kx_nk_3(hydro_kx_state *state, hydro_kx_session_keypair *kp,
			  const uint8_t packet2[hydro_kx_NK_PACKET2BYTES])
{
	const uint8_t *peer_eph_pk = &packet2[0];
	const uint8_t *packet2_mac = &packet2[hydro_kx_PUBLICKEYBYTES];

	hydro_hash_update(&state->h_st, peer_eph_pk, hydro_x25519_PUBLICKEYBYTES);
	if (hydro_kx_dh(state, state->eph_kp.sk, peer_eph_pk) != 0 ||
		hydro_kx_aead_decrypt(state, NULL, packet2_mac, hydro_kx_AEAD_MACBYTES) != 0) {
		return -1;
	}
	hydro_kx_final(state, kp->rx, kp->tx);

	return 0;
}


#define hydro_pwhash_ENC_ALGBYTES 1
#define hydro_pwhash_HASH_ALGBYTES 1
#define hydro_pwhash_THREADSBYTES 1
#define hydro_pwhash_OPSLIMITBYTES 8
#define hydro_pwhash_MEMLIMITBYTES 8
#define hydro_pwhash_HASHBYTES 32
#define hydro_pwhash_SALTBYTES 16
#define hydro_pwhash_PARAMSBYTES                                                           \
	(hydro_pwhash_HASH_ALGBYTES + hydro_pwhash_THREADSBYTES + hydro_pwhash_OPSLIMITBYTES + \
	 hydro_pwhash_MEMLIMITBYTES + hydro_pwhash_SALTBYTES + hydro_pwhash_HASHBYTES)
#define hydro_pwhash_ENC_ALG 0x01
#define hydro_pwhash_HASH_ALG 0x01
#define hydro_pwhash_CONTEXT "hydro_pw"

static int
_hydro_pwhash_hash(uint8_t out[hydro_random_SEEDBYTES], size_t h_len,
				   const uint8_t salt[hydro_pwhash_SALTBYTES], const char *passwd,
				   size_t passwd_len, const char ctx[hydro_pwhash_CONTEXTBYTES],
				   const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES], uint64_t opslimit,
				   size_t memlimit, uint8_t threads)
{
	_hydro_attr_aligned_(16) uint8_t state[gimli_BLOCKBYTES];
	hydro_hash_state                 h_st;
	uint8_t                          tmp64_u8[8];
	uint64_t                         i;
	uint8_t                          tmp8;

	COMPILER_ASSERT(hydro_pwhash_MASTERKEYBYTES >= hydro_hash_KEYBYTES);
	hydro_hash_init(&h_st, ctx, master_key);

	STORE64_LE(tmp64_u8, (uint64_t) passwd_len);
	hydro_hash_update(&h_st, tmp64_u8, sizeof tmp64_u8);
	hydro_hash_update(&h_st, passwd, passwd_len);

	hydro_hash_update(&h_st, salt, hydro_pwhash_SALTBYTES);

	tmp8 = hydro_pwhash_HASH_ALG;
	hydro_hash_update(&h_st, &tmp8, 1);

	hydro_hash_update(&h_st, &threads, 1);

	STORE64_LE(tmp64_u8, (uint64_t) memlimit);
	hydro_hash_update(&h_st, tmp64_u8, sizeof tmp64_u8);

	STORE64_LE(tmp64_u8, (uint64_t) h_len);
	hydro_hash_update(&h_st, tmp64_u8, sizeof tmp64_u8);

	hydro_hash_final(&h_st, (uint8_t *) (void *) &state, sizeof state);

	gimli_core_u8(state, 1);
	COMPILER_ASSERT(gimli_RATE >= 8);
	for (i = 0; i < opslimit; i++) {
		mem_zero(state, gimli_RATE);
		STORE64_LE(state, i);
		gimli_core_u8(state, 0);
	}
	mem_zero(state, gimli_RATE);

	COMPILER_ASSERT(hydro_random_SEEDBYTES == gimli_CAPACITY);
	memcpy(out, state + gimli_RATE, hydro_random_SEEDBYTES);
	hydro_memzero(state, sizeof state);

	return 0;
}

void
hydro_pwhash_keygen(uint8_t master_key[hydro_pwhash_MASTERKEYBYTES])
{
	hydro_random_buf(master_key, hydro_pwhash_MASTERKEYBYTES);
}

int
hydro_pwhash_deterministic(uint8_t *h, size_t h_len, const char *passwd, size_t passwd_len,
						   const char    ctx[hydro_pwhash_CONTEXTBYTES],
						   const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES], uint64_t opslimit,
						   size_t memlimit, uint8_t threads)
{
	uint8_t seed[hydro_random_SEEDBYTES];

	COMPILER_ASSERT(sizeof zero >= hydro_pwhash_SALTBYTES);
	COMPILER_ASSERT(sizeof zero >= hydro_pwhash_MASTERKEYBYTES);

	(void) memlimit;
	if (_hydro_pwhash_hash(seed, h_len, zero, passwd, passwd_len, ctx, master_key, opslimit,
						   memlimit, threads) != 0) {
		return -1;
	}
	hydro_random_buf_deterministic(h, h_len, seed);
	hydro_memzero(seed, sizeof seed);

	return 0;
}

int
hydro_pwhash_create(uint8_t stored[hydro_pwhash_STOREDBYTES], const char *passwd, size_t passwd_len,
					const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES], uint64_t opslimit,
					size_t memlimit, uint8_t threads)
{
	uint8_t *const enc_alg     = &stored[0];
	uint8_t *const secretbox   = &enc_alg[hydro_pwhash_ENC_ALGBYTES];
	uint8_t *const hash_alg    = &secretbox[hydro_secretbox_HEADERBYTES];
	uint8_t *const threads_u8  = &hash_alg[hydro_pwhash_HASH_ALGBYTES];
	uint8_t *const opslimit_u8 = &threads_u8[hydro_pwhash_THREADSBYTES];
	uint8_t *const memlimit_u8 = &opslimit_u8[hydro_pwhash_OPSLIMITBYTES];
	uint8_t *const salt        = &memlimit_u8[hydro_pwhash_MEMLIMITBYTES];
	uint8_t *const h           = &salt[hydro_pwhash_SALTBYTES];

	COMPILER_ASSERT(hydro_pwhash_STOREDBYTES >= hydro_pwhash_ENC_ALGBYTES +
													hydro_secretbox_HEADERBYTES +
													hydro_pwhash_PARAMSBYTES);
	(void) memlimit;
	mem_zero(stored, hydro_pwhash_STOREDBYTES);
	*enc_alg    = hydro_pwhash_ENC_ALG;
	*hash_alg   = hydro_pwhash_HASH_ALG;
	*threads_u8 = threads;
	STORE64_LE(opslimit_u8, opslimit);
	STORE64_LE(memlimit_u8, (uint64_t) memlimit);
	hydro_random_buf(salt, hydro_pwhash_SALTBYTES);

	COMPILER_ASSERT(sizeof zero >= hydro_pwhash_MASTERKEYBYTES);
	if (_hydro_pwhash_hash(h, hydro_pwhash_HASHBYTES, salt, passwd, passwd_len,
						   hydro_pwhash_CONTEXT, zero, opslimit, memlimit, threads) != 0) {
		return -1;
	}
	COMPILER_ASSERT(hydro_pwhash_MASTERKEYBYTES == hydro_secretbox_KEYBYTES);

	return hydro_secretbox_encrypt(secretbox, hash_alg, hydro_pwhash_PARAMSBYTES,
								   (uint64_t) *enc_alg, hydro_pwhash_CONTEXT, master_key);
}

static int
_hydro_pwhash_verify(uint8_t       computed_h[hydro_pwhash_HASHBYTES],
					 const uint8_t stored[hydro_pwhash_STOREDBYTES], const char *passwd,
					 size_t passwd_len, const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES],
					 uint64_t opslimit_max, size_t memlimit_max, uint8_t threads_max)
{
	const uint8_t *const enc_alg   = &stored[0];
	const uint8_t *const secretbox = &enc_alg[hydro_pwhash_ENC_ALGBYTES];

	uint8_t        params[hydro_pwhash_PARAMSBYTES];
	uint8_t *const hash_alg    = &params[0];
	uint8_t *const threads_u8  = &hash_alg[hydro_pwhash_HASH_ALGBYTES];
	uint8_t *const opslimit_u8 = &threads_u8[hydro_pwhash_THREADSBYTES];
	uint8_t *const memlimit_u8 = &opslimit_u8[hydro_pwhash_OPSLIMITBYTES];
	uint8_t *const salt        = &memlimit_u8[hydro_pwhash_MEMLIMITBYTES];
	uint8_t *const h           = &salt[hydro_pwhash_SALTBYTES];

	uint64_t opslimit;
	size_t   memlimit;
	uint8_t  threads;

	(void) memlimit;
	if (*enc_alg != hydro_pwhash_ENC_ALG) {
		return -1;
	}
	if (hydro_secretbox_decrypt(params, secretbox,
								hydro_secretbox_HEADERBYTES + hydro_pwhash_PARAMSBYTES,
								(uint64_t) *enc_alg, hydro_pwhash_CONTEXT, master_key) != 0) {
		return -1;
	}
	if (*hash_alg != hydro_pwhash_HASH_ALG || (opslimit = LOAD64_LE(opslimit_u8)) > opslimit_max ||
		(memlimit = (size_t) LOAD64_LE(memlimit_u8)) > memlimit_max ||
		(threads = *threads_u8) > threads_max) {
		return -1;
	}
	if (_hydro_pwhash_hash(computed_h, hydro_pwhash_HASHBYTES, salt, passwd, passwd_len,
						   hydro_pwhash_CONTEXT, zero, opslimit, memlimit, threads) == 0 &&
		hydro_equal(computed_h, h, hydro_pwhash_HASHBYTES) == 1) {
		return 0;
	}
	return -1;
}

int
hydro_pwhash_verify(const uint8_t stored[hydro_pwhash_STOREDBYTES], const char *passwd,
					size_t passwd_len, const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES],
					uint64_t opslimit_max, size_t memlimit_max, uint8_t threads_max)
{
	uint8_t computed_h[hydro_pwhash_HASHBYTES];
	int     ret;

	ret = _hydro_pwhash_verify(computed_h, stored, passwd, passwd_len, master_key, opslimit_max,
							   memlimit_max, threads_max);
	hydro_memzero(computed_h, sizeof computed_h);

	return ret;
}

int
hydro_pwhash_derive_static_key(uint8_t *static_key, size_t static_key_len,
							   const uint8_t stored[hydro_pwhash_STOREDBYTES], const char *passwd,
							   size_t passwd_len, const char ctx[hydro_pwhash_CONTEXTBYTES],
							   const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES],
							   uint64_t opslimit_max, size_t memlimit_max, uint8_t threads_max)
{
	uint8_t computed_h[hydro_pwhash_HASHBYTES];

	if (_hydro_pwhash_verify(computed_h, stored, passwd, passwd_len, master_key, opslimit_max,
							 memlimit_max, threads_max) != 0) {
		hydro_memzero(computed_h, sizeof computed_h);
		return -1;
	}
	COMPILER_ASSERT(hydro_kdf_CONTEXTBYTES <= hydro_pwhash_CONTEXTBYTES);
	COMPILER_ASSERT(hydro_kdf_KEYBYTES <= hydro_pwhash_HASHBYTES);
	hydro_kdf_derive_from_key(static_key, static_key_len, 0, ctx, computed_h);
	hydro_memzero(computed_h, sizeof computed_h);

	return 0;
}

int
hydro_pwhash_reencrypt(uint8_t       stored[hydro_pwhash_STOREDBYTES],
					   const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES],
					   const uint8_t new_master_key[hydro_pwhash_MASTERKEYBYTES])
{
	uint8_t *const enc_alg   = &stored[0];
	uint8_t *const secretbox = &enc_alg[hydro_pwhash_ENC_ALGBYTES];
	uint8_t *const params    = &secretbox[hydro_secretbox_HEADERBYTES];

	if (*enc_alg != hydro_pwhash_ENC_ALG) {
		return -1;
	}
	if (hydro_secretbox_decrypt(secretbox, secretbox,
								hydro_secretbox_HEADERBYTES + hydro_pwhash_PARAMSBYTES,
								(uint64_t) *enc_alg, hydro_pwhash_CONTEXT, master_key) != 0) {
		return -1;
	}
	memmove(params, secretbox, hydro_pwhash_PARAMSBYTES);
	return hydro_secretbox_encrypt(secretbox, params, hydro_pwhash_PARAMSBYTES, (uint64_t) *enc_alg,
								   hydro_pwhash_CONTEXT, new_master_key);
}

int
hydro_pwhash_upgrade(uint8_t       stored[hydro_pwhash_STOREDBYTES],
					 const uint8_t master_key[hydro_pwhash_MASTERKEYBYTES], uint64_t opslimit,
					 size_t memlimit, uint8_t threads)
{
	uint8_t *const enc_alg     = &stored[0];
	uint8_t *const secretbox   = &enc_alg[hydro_pwhash_ENC_ALGBYTES];
	uint8_t *const params      = &secretbox[hydro_secretbox_HEADERBYTES];
	uint8_t *const hash_alg    = &params[0];
	uint8_t *const threads_u8  = &hash_alg[hydro_pwhash_HASH_ALGBYTES];
	uint8_t *const opslimit_u8 = &threads_u8[hydro_pwhash_THREADSBYTES];
	uint8_t *const memlimit_u8 = &opslimit_u8[hydro_pwhash_OPSLIMITBYTES];
	uint8_t *const salt        = &memlimit_u8[hydro_pwhash_MEMLIMITBYTES];
	uint8_t *const h           = &salt[hydro_pwhash_SALTBYTES];

	_hydro_attr_aligned_(16) uint8_t state[gimli_BLOCKBYTES];
	uint64_t                         i;
	uint64_t                         opslimit_prev;

	if (*enc_alg != hydro_pwhash_ENC_ALG) {
		return -1;
	}
	if (hydro_secretbox_decrypt(secretbox, secretbox,
								hydro_secretbox_HEADERBYTES + hydro_pwhash_PARAMSBYTES,
								(uint64_t) *enc_alg, hydro_pwhash_CONTEXT, master_key) != 0) {
		return -1;
	}
	memmove(params, secretbox, hydro_pwhash_PARAMSBYTES);
	opslimit_prev = LOAD64_LE(opslimit_u8);
	if (*hash_alg != hydro_pwhash_HASH_ALG) {
		mem_zero(stored, hydro_pwhash_STOREDBYTES);
		return -1;
	}
	COMPILER_ASSERT(hydro_random_SEEDBYTES == gimli_CAPACITY);
	memcpy(state + gimli_RATE, h, hydro_random_SEEDBYTES);
	for (i = opslimit_prev; i < opslimit; i++) {
		mem_zero(state, gimli_RATE);
		STORE64_LE(state, i);
		gimli_core_u8(state, 0);
	}
	mem_zero(state, gimli_RATE);
	memcpy(h, state + gimli_RATE, hydro_random_SEEDBYTES);
	*threads_u8 = threads;
	STORE64_LE(opslimit_u8, opslimit);
	STORE64_LE(memlimit_u8, (uint64_t) memlimit);

	return hydro_secretbox_encrypt(secretbox, params, hydro_pwhash_PARAMSBYTES, (uint64_t) *enc_alg,
								   hydro_pwhash_CONTEXT, master_key);
}


#define hydro_sign_CHALLENGEBYTES 32
#define hydro_sign_NONCEBYTES 32
#define hydro_sign_PREHASHBYTES 64

static void
hydro_sign_p2(uint8_t sig[hydro_x25519_BYTES], const uint8_t challenge[hydro_sign_CHALLENGEBYTES],
			  const uint8_t eph_sk[hydro_x25519_BYTES], const uint8_t sk[hydro_x25519_BYTES])
{
	hydro_x25519_scalar_t scalar1, scalar2, scalar3;

	COMPILER_ASSERT(hydro_sign_CHALLENGEBYTES == hydro_x25519_BYTES);
	hydro_x25519_swapin(scalar1, eph_sk);
	hydro_x25519_swapin(scalar2, sk);
	hydro_x25519_swapin(scalar3, challenge);
	hydro_x25519_sc_montmul(scalar1, scalar2, scalar3);
	mem_zero(scalar2, sizeof scalar2);
	hydro_x25519_sc_montmul(scalar2, scalar1, hydro_x25519_sc_r2);
	hydro_x25519_swapout(sig, scalar2);
}

static void
hydro_sign_challenge(uint8_t       challenge[hydro_sign_CHALLENGEBYTES],
					 const uint8_t nonce[hydro_sign_NONCEBYTES],
					 const uint8_t pk[hydro_sign_PUBLICKEYBYTES],
					 const uint8_t prehash[hydro_sign_PREHASHBYTES])
{
	hydro_hash_state st;

	hydro_hash_init(&st, (const char *) zero, NULL);
	hydro_hash_update(&st, nonce, hydro_sign_NONCEBYTES);
	hydro_hash_update(&st, pk, hydro_sign_PUBLICKEYBYTES);
	hydro_hash_update(&st, prehash, hydro_sign_PREHASHBYTES);
	hydro_hash_final(&st, challenge, hydro_sign_CHALLENGEBYTES);
}

static int
hydro_sign_prehash(uint8_t csig[hydro_sign_BYTES], const uint8_t prehash[hydro_sign_PREHASHBYTES],
				   const uint8_t sk[hydro_sign_SECRETKEYBYTES])
{
	hydro_hash_state st;
	uint8_t          challenge[hydro_sign_CHALLENGEBYTES];
	const uint8_t *  pk     = &sk[hydro_x25519_SECRETKEYBYTES];
	uint8_t *        nonce  = &csig[0];
	uint8_t *        sig    = &csig[hydro_sign_NONCEBYTES];
	uint8_t *        eph_sk = sig;

	hydro_random_buf(eph_sk, hydro_x25519_SECRETKEYBYTES);
	COMPILER_ASSERT(hydro_x25519_SECRETKEYBYTES == hydro_hash_KEYBYTES);
	hydro_hash_init(&st, (const char *) zero, sk);
	hydro_hash_update(&st, eph_sk, hydro_x25519_SECRETKEYBYTES);
	hydro_hash_update(&st, prehash, hydro_sign_PREHASHBYTES);
	hydro_hash_final(&st, eph_sk, hydro_x25519_SECRETKEYBYTES);

	hydro_x25519_scalarmult_base_uniform(nonce, eph_sk);
	hydro_sign_challenge(challenge, nonce, pk, prehash);

	COMPILER_ASSERT(hydro_sign_BYTES == hydro_sign_NONCEBYTES + hydro_x25519_SECRETKEYBYTES);
	COMPILER_ASSERT(hydro_x25519_SECRETKEYBYTES <= hydro_sign_CHALLENGEBYTES);
	hydro_sign_p2(sig, challenge, eph_sk, sk);

	return 0;
}

static int
hydro_sign_verify_core(hydro_x25519_fe xs[5], const hydro_x25519_limb_t *other1,
					   const uint8_t other2[hydro_x25519_BYTES])
{
	hydro_x25519_limb_t *     z2 = xs[1], *x3 = xs[2], *z3 = xs[3];
	hydro_x25519_fe           xo2;
	const hydro_x25519_limb_t sixteen = 16;

	hydro_x25519_swapin(xo2, other2);
	memcpy(x3, other1, 2 * sizeof(hydro_x25519_fe));
	hydro_x25519_ladder_part1(xs);

	/* Here z2 = t2^2 */
	hydro_x25519_mul1(z2, other1);
	hydro_x25519_mul1(z2, other1 + hydro_x25519_NLIMBS);
	hydro_x25519_mul1(z2, xo2);

	hydro_x25519_mul(z2, z2, &sixteen, 1);

	hydro_x25519_mul1(z3, xo2);
	hydro_x25519_sub(z3, z3, x3);
	hydro_x25519_sqr1(z3);

	/* check equality */
	hydro_x25519_sub(z3, z3, z2);

	/* canon(z2): both sides are zero. canon(z3): the two sides are equal. */
	/* Reject sigs where both sides are zero. */
	return hydro_x25519_canon(z2) | ~hydro_x25519_canon(z3);
}

static int
hydro_sign_verify_p2(const uint8_t sig[hydro_x25519_BYTES],
					 const uint8_t challenge[hydro_sign_CHALLENGEBYTES],
					 const uint8_t nonce[hydro_sign_NONCEBYTES],
					 const uint8_t pk[hydro_x25519_BYTES])
{
	hydro_x25519_fe xs[7];

	hydro_x25519_core(&xs[0], challenge, pk, 0);
	hydro_x25519_core(&xs[2], sig, hydro_x25519_BASE_POINT, 0);

	return hydro_sign_verify_core(&xs[2], xs[0], nonce);
}

static int
hydro_sign_verify_challenge(const uint8_t csig[hydro_sign_BYTES],
							const uint8_t challenge[hydro_sign_CHALLENGEBYTES],
							const uint8_t pk[hydro_sign_PUBLICKEYBYTES])
{
	const uint8_t *nonce = &csig[0];
	const uint8_t *sig   = &csig[hydro_sign_NONCEBYTES];

	return hydro_sign_verify_p2(sig, challenge, nonce, pk);
}

void
hydro_sign_keygen(hydro_sign_keypair *kp)
{
	uint8_t *pk_copy = &kp->sk[hydro_x25519_SECRETKEYBYTES];

	COMPILER_ASSERT(hydro_sign_SECRETKEYBYTES ==
					hydro_x25519_SECRETKEYBYTES + hydro_x25519_PUBLICKEYBYTES);
	COMPILER_ASSERT(hydro_sign_PUBLICKEYBYTES == hydro_x25519_PUBLICKEYBYTES);
	hydro_random_buf(kp->sk, hydro_x25519_SECRETKEYBYTES);
	hydro_x25519_scalarmult_base_uniform(kp->pk, kp->sk);
	memcpy(pk_copy, kp->pk, hydro_x25519_PUBLICKEYBYTES);
}

void
hydro_sign_keygen_deterministic(hydro_sign_keypair *kp, const uint8_t seed[hydro_sign_SEEDBYTES])
{
	uint8_t *pk_copy = &kp->sk[hydro_x25519_SECRETKEYBYTES];

	COMPILER_ASSERT(hydro_sign_SEEDBYTES >= hydro_random_SEEDBYTES);
	hydro_random_buf_deterministic(kp->sk, hydro_x25519_SECRETKEYBYTES, seed);
	hydro_x25519_scalarmult_base_uniform(kp->pk, kp->sk);
	memcpy(pk_copy, kp->pk, hydro_x25519_PUBLICKEYBYTES);
}

int
hydro_sign_init(hydro_sign_state *state, const char ctx[hydro_sign_CONTEXTBYTES])
{
	return hydro_hash_init(&state->hash_st, ctx, NULL);
}

int
hydro_sign_update(hydro_sign_state *state, const void *m_, size_t mlen)
{
	return hydro_hash_update(&state->hash_st, m_, mlen);
}

int
hydro_sign_final_create(hydro_sign_state *state, uint8_t csig[hydro_sign_BYTES],
						const uint8_t sk[hydro_sign_SECRETKEYBYTES])
{
	uint8_t prehash[hydro_sign_PREHASHBYTES];

	hydro_hash_final(&state->hash_st, prehash, sizeof prehash);

	return hydro_sign_prehash(csig, prehash, sk);
}

int
hydro_sign_final_verify(hydro_sign_state *state, const uint8_t csig[hydro_sign_BYTES],
						const uint8_t pk[hydro_sign_PUBLICKEYBYTES])
{
	uint8_t        challenge[hydro_sign_CHALLENGEBYTES];
	uint8_t        prehash[hydro_sign_PREHASHBYTES];
	const uint8_t *nonce = &csig[0];

	hydro_hash_final(&state->hash_st, prehash, sizeof prehash);
	hydro_sign_challenge(challenge, nonce, pk, prehash);

	return hydro_sign_verify_challenge(csig, challenge, pk);
}

int
hydro_sign_create(uint8_t csig[hydro_sign_BYTES], const void *m_, size_t mlen,
				  const char    ctx[hydro_sign_CONTEXTBYTES],
				  const uint8_t sk[hydro_sign_SECRETKEYBYTES])
{
	hydro_sign_state st;

	if (hydro_sign_init(&st, ctx) != 0 || hydro_sign_update(&st, m_, mlen) != 0 ||
		hydro_sign_final_create(&st, csig, sk) != 0) {
		return -1;
	}
	return 0;
}

int
hydro_sign_verify(const uint8_t csig[hydro_sign_BYTES], const void *m_, size_t mlen,
				  const char    ctx[hydro_sign_CONTEXTBYTES],
				  const uint8_t pk[hydro_sign_PUBLICKEYBYTES])
{
	hydro_sign_state st;

	if (hydro_sign_init(&st, ctx) != 0 || hydro_sign_update(&st, m_, mlen) != 0 ||
		hydro_sign_final_verify(&st, csig, pk) != 0) {
		return -1;
	}
	return 0;
}

#if defined(_MSC_VER)
#	pragma warning(pop)
#endif

// End of embedded libhydrogen source.
// See https://github.com/jedisct1/libhydrogen for the original source.
//--------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------
// PROTOCOL

#define CN_PROTOCOL_VERSION_STRING ((const uint8_t*)"CUTE 1.00")
#define CN_PROTOCOL_VERSION_STRING_LEN (9 + 1)
#define CN_PROTOCOL_SERVER_MAX_CLIENTS 32
#define CN_PROTOCOL_PACKET_SIZE_MAX (CN_KB + 256)
#define CN_PROTOCOL_PACKET_PAYLOAD_MAX (1207 - 2)
#define CN_PROTOCOL_CLIENT_SEND_BUFFER_SIZE (256 * CN_KB)
#define CN_PROTOCOL_CLIENT_RECEIVE_BUFFER_SIZE (256 * CN_KB)
#define CN_PROTOCOL_SERVER_SEND_BUFFER_SIZE (CN_MB * 2)
#define CN_PROTOCOL_SERVER_RECEIVE_BUFFER_SIZE (CN_MB * 2)
#define CN_PROTOCOL_EVENT_QUEUE_SIZE (CN_MB * 4)
#define CN_PROTOCOL_SIGNATURE_SIZE 64

#define CN_PROTOCOL_CONNECT_TOKEN_PACKET_SIZE 1024
#define CN_PROTOCOL_CONNECT_TOKEN_SIZE 1114
#define CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE 256
#define CN_PROTOCOL_CONNECT_TOKEN_SECRET_SECTION_SIZE (64 + 8 + 32 + 32 + 256)
#define CN_PROTOCOL_CONNECT_TOKEN_ENDPOINT_MAX 32

#define CN_PROTOCOL_REPLAY_BUFFER_SIZE 256
#define CN_PROTOCOL_SEND_RATE (1.0f / 10.0f)
#define CN_PROTOCOL_DISCONNECT_REDUNDANT_PACKET_COUNT 10
#define CN_PROTOCOL_CHALLENGE_DATA_SIZE 256
#define CN_PROTOCOL_REDUNDANT_DISCONNECT_PACKET_COUNT 10

typedef enum cn_protocol_packet_type_t
{
	CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN,
	CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED,
	CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED,
	CN_PROTOCOL_PACKET_TYPE_KEEPALIVE,
	CN_PROTOCOL_PACKET_TYPE_DISCONNECT,
	CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST,
	CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE,
	CN_PROTOCOL_PACKET_TYPE_PAYLOAD,

	CN_PROTOCOL_PACKET_TYPE_COUNT,
} cn_protocol_packet_type_t;

typedef struct cn_protocol_packet_allocator_t cn_protocol_packet_allocator_t;
typedef struct cn_protocol_client_t cn_protocol_client_t;

typedef enum cn_protocol_client_state_t
{
	CN_PROTOCOL_CLIENT_STATE_CONNECT_TOKEN_EXPIRED         = -6,
	CN_PROTOCOL_CLIENT_STATE_INVALID_CONNECT_TOKEN         = -5,
	CN_PROTOCOL_CLIENT_STATE_CONNECTION_TIMED_OUT          = -4,
	CN_PROTOCOL_CLIENT_STATE_CHALLENGED_RESPONSE_TIMED_OUT = -3,
	CN_PROTOCOL_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT  = -2,
	CN_PROTOCOL_CLIENT_STATE_CONNECTION_DENIED             = -1,
	CN_PROTOCOL_CLIENT_STATE_DISCONNECTED                  =  0,
	CN_PROTOCOL_CLIENT_STATE_SENDING_CONNECTION_REQUEST    =  1,
	CN_PROTOCOL_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE    =  2,
	CN_PROTOCOL_CLIENT_STATE_CONNECTED                     =  3,
} cn_protocol_client_state_t;

typedef struct cn_protocol_server_t cn_protocol_server_t;

typedef enum cn_protocol_server_event_type_t
{
	CN_PROTOCOL_SERVER_EVENT_NEW_CONNECTION,
	CN_PROTOCOL_SERVER_EVENT_DISCONNECTED,
	CN_PROTOCOL_SERVER_EVENT_PAYLOAD_PACKET,
} cn_protocol_server_event_type_t;

typedef struct cn_protocol_server_event_t
{
	cn_protocol_server_event_type_t type;
	union
	{
		struct
		{
			int client_index;
			uint64_t client_id;
			cn_endpoint_t endpoint;
		} new_connection;

		struct
		{
			int client_index;
		} disconnected;

		struct
		{
			int client_index;
			void* data;
			int size;
		} payload_packet;
	} u;
} cn_protocol_server_event_t;

void cn_write_uint8(uint8_t** p, uint8_t value)
{
	**p = value;
	++(*p);
}

void cn_write_uint16(uint8_t** p, uint16_t value)
{
	(*p)[0] = value & 0xFF;
	(*p)[1] = value >> 8;
	*p += 2;
}

void cn_write_uint32(uint8_t** p, uint32_t value)
{
	(*p)[0] = value & 0xFF;
	(*p)[1] = (value >> 8 ) & 0xFF;
	(*p)[2] = (value >> 16) & 0xFF;
	(*p)[3] = value >> 24;
	*p += 4;
}

void cn_write_float(uint8_t** p, float value)
{
	union
	{
		uint32_t as_uint32;
		float as_float;
	} val;
	val.as_float = value;
	cn_write_uint32(p, val.as_uint32);
}

void cn_write_uint64(uint8_t** p, uint64_t value)
{
	(*p)[0] = value & 0xFF;
	(*p)[1] = (value >> 8 ) & 0xFF;
	(*p)[2] = (value >> 16) & 0xFF;
	(*p)[3] = (value >> 24) & 0xFF;
	(*p)[4] = (value >> 32) & 0xFF;
	(*p)[5] = (value >> 40) & 0xFF;
	(*p)[6] = (value >> 48) & 0xFF;
	(*p)[7] = value >> 56;
	*p += 8;
}

void cn_write_bytes(uint8_t** p, const uint8_t* byte_array, int num_bytes)
{
	for (int i = 0; i < num_bytes; ++i)
	{
		cn_write_uint8(p, byte_array[i]);
	}
}

void cn_write_endpoint(uint8_t** p, cn_endpoint_t endpoint)
{
	cn_write_uint8(p, (uint8_t)endpoint.type);
	if (endpoint.type == CN_ADDRESS_TYPE_IPV4) {
		cn_write_uint8(p, endpoint.u.ipv4[0]);
		cn_write_uint8(p, endpoint.u.ipv4[1]);
		cn_write_uint8(p, endpoint.u.ipv4[2]);
		cn_write_uint8(p, endpoint.u.ipv4[3]);
	}
#ifndef CUTE_NET_NO_IPV6
	else if (endpoint.type == CN_ADDRESS_TYPE_IPV6) {
		cn_write_uint16(p, endpoint.u.ipv6[0]);
		cn_write_uint16(p, endpoint.u.ipv6[1]);
		cn_write_uint16(p, endpoint.u.ipv6[2]);
		cn_write_uint16(p, endpoint.u.ipv6[3]);
		cn_write_uint16(p, endpoint.u.ipv6[4]);
		cn_write_uint16(p, endpoint.u.ipv6[5]);
		cn_write_uint16(p, endpoint.u.ipv6[6]);
		cn_write_uint16(p, endpoint.u.ipv6[7]);
	} 
#endif
	else {
		CN_ASSERT(0);
	}
	cn_write_uint16(p, endpoint.port);
}

void cn_write_key(uint8_t** p, const cn_crypto_key_t* key)
{
	cn_write_bytes(p, (const uint8_t*)key, sizeof(*key));
}

void cn_write_fourcc(uint8_t** p, const char* fourcc)
{
	cn_write_uint8(p, fourcc[0]);
	cn_write_uint8(p, fourcc[1]);
	cn_write_uint8(p, fourcc[2]);
	cn_write_uint8(p, fourcc[3]);
}

CN_INLINE uint8_t cn_read_uint8(uint8_t** p)
{
	uint8_t value = **p;
	++(*p);
	return value;
}

CN_INLINE uint16_t cn_read_uint16(uint8_t** p)
{
	uint16_t value;
	value = (*p)[0];
	value |= (((uint16_t)((*p)[1])) << 8);
	*p += 2;
	return value;
}

CN_INLINE uint32_t cn_read_uint32(uint8_t** p)
{
	uint32_t value;
	value  = (*p)[0];
	value |= (((uint32_t)((*p)[1])) << 8);
	value |= (((uint32_t)((*p)[2])) << 16);
	value |= (((uint32_t)((*p)[3])) << 24);
	*p += 4;
	return value;
}

CN_INLINE float cn_read_float(uint8_t** p)
{
	union
	{
		uint32_t as_uint32;
		float as_float;
	} val;
	val.as_uint32 = cn_read_uint32(p);
	return val.as_float;
}

CN_INLINE uint64_t cn_read_uint64(uint8_t** p)
{
	uint64_t value;
	value  = (*p)[0];
	value |= (((uint64_t)((*p)[1])) << 8 );
	value |= (((uint64_t)((*p)[2])) << 16);
	value |= (((uint64_t)((*p)[3])) << 24);
	value |= (((uint64_t)((*p)[4])) << 32);
	value |= (((uint64_t)((*p)[5])) << 40);
	value |= (((uint64_t)((*p)[6])) << 48);
	value |= (((uint64_t)((*p)[7])) << 56);
	*p += 8;
	return value;
}

CN_INLINE void cn_read_bytes(uint8_t** p, uint8_t* byte_array, int num_bytes)
{
	for (int i = 0; i < num_bytes; ++i)
	{
		byte_array[i] = cn_read_uint8(p);
	}
}

cn_endpoint_t cn_read_endpoint(uint8_t** p)
{
	cn_endpoint_t endpoint;
	endpoint.type = (cn_address_type_t)cn_read_uint8(p);
	if (endpoint.type == CN_ADDRESS_TYPE_IPV4) {
		endpoint.u.ipv4[0] = cn_read_uint8(p);
		endpoint.u.ipv4[1] = cn_read_uint8(p);
		endpoint.u.ipv4[2] = cn_read_uint8(p);
		endpoint.u.ipv4[3] = cn_read_uint8(p);
	}
#ifndef CUTE_NET_NO_IPV6
	else if (endpoint.type == CN_ADDRESS_TYPE_IPV6) {
		endpoint.u.ipv6[0] = cn_read_uint16(p);
		endpoint.u.ipv6[1] = cn_read_uint16(p);
		endpoint.u.ipv6[2] = cn_read_uint16(p);
		endpoint.u.ipv6[3] = cn_read_uint16(p);
		endpoint.u.ipv6[4] = cn_read_uint16(p);
		endpoint.u.ipv6[5] = cn_read_uint16(p);
		endpoint.u.ipv6[6] = cn_read_uint16(p);
		endpoint.u.ipv6[7] = cn_read_uint16(p);
	}
#endif
	else {
		CN_ASSERT(0);
	}
	endpoint.port = cn_read_uint16(p);
	return endpoint;
}

CN_INLINE cn_crypto_key_t cn_read_key(uint8_t** p)
{
	cn_crypto_key_t key;
	cn_read_bytes(p, (uint8_t*)&key, sizeof(key));
	return key;
}

CN_INLINE void cn_read_fourcc(uint8_t** p, uint8_t* fourcc)
{
	fourcc[0] = cn_read_uint8(p);
	fourcc[1] = cn_read_uint8(p);
	fourcc[2] = cn_read_uint8(p);
	fourcc[3] = cn_read_uint8(p);
}

typedef struct cn_protocol_replay_buffer_t
{
	uint64_t max;
	uint64_t entries[CN_PROTOCOL_REPLAY_BUFFER_SIZE];
} cn_protocol_replay_buffer_t;

// -------------------------------------------------------------------------------------------------

typedef struct cn_protocol_packet_connect_token_t
{
	uint8_t packet_type;
	uint64_t expiration_timestamp;
	uint32_t handshake_timeout;
	uint16_t endpoint_count;
	cn_endpoint_t endpoints[CN_PROTOCOL_CONNECT_TOKEN_ENDPOINT_MAX];
} cn_protocol_packet_connect_token_t;

typedef struct cn_protocol_packet_connection_accepted_t
{
	uint8_t packet_type;
	uint64_t client_id;
	uint32_t max_clients;
	uint32_t connection_timeout;
} cn_protocol_packet_connection_accepted_t;

typedef struct cn_protocol_packet_connection_denied_t
{
	uint8_t packet_type;
} cn_protocol_packet_connection_denied_t;

typedef struct cn_protocol_packet_keepalive_t
{
	uint8_t packet_type;
} cn_protocol_packet_keepalive_t;

typedef struct cn_protocol_packet_disconnect_t
{
	uint8_t packet_type;
} cn_protocol_packet_disconnect_t;

typedef struct cn_protocol_packet_challenge_t
{
	uint8_t packet_type;
	uint64_t challenge_nonce;
	uint8_t challenge_data[CN_PROTOCOL_CHALLENGE_DATA_SIZE];
} cn_protocol_packet_challenge_t;

typedef struct cn_protocol_packet_payload_t
{
	uint8_t packet_type;
	uint16_t payload_size;
	uint8_t payload[CN_PROTOCOL_PACKET_PAYLOAD_MAX];
} cn_protocol_packet_payload_t;

// -------------------------------------------------------------------------------------------------

typedef struct cn_protocol_connect_token_t
{
	uint64_t creation_timestamp;
	cn_crypto_key_t client_to_server_key;
	cn_crypto_key_t server_to_client_key;

	uint64_t expiration_timestamp;
	uint32_t handshake_timeout;
	uint16_t endpoint_count;
	cn_endpoint_t endpoints[CN_PROTOCOL_CONNECT_TOKEN_ENDPOINT_MAX];
} cn_protocol_connect_token_t;

typedef struct cn_protocol_connect_token_decrypted_t
{
	uint64_t expiration_timestamp;
	uint32_t handshake_timeout;
	uint16_t endpoint_count;
	cn_endpoint_t endpoints[CN_PROTOCOL_CONNECT_TOKEN_ENDPOINT_MAX];

	uint64_t client_id;
	cn_crypto_key_t client_to_server_key;
	cn_crypto_key_t server_to_client_key;
	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_signature_t signature;
} cn_protocol_connect_token_decrypted_t;

// -------------------------------------------------------------------------------------------------

#define CN_HASHTABLE_KEY_BYTES (hydro_hash_KEYBYTES)
#define CN_HASHTABLE_HASH_BYTES (hydro_hash_BYTES)

typedef struct cn_hashtable_slot_t
{
	uint64_t key_hash;
	int item_index;
	int base_count;
} cn_hashtable_slot_t;

typedef struct cn_hashtable_t
{
	int count;
	int slot_capacity;
	cn_hashtable_slot_t* slots;

	uint8_t secret_key[CN_HASHTABLE_KEY_BYTES];

	int key_size;
	int item_size;
	int item_capacity;
	void* items_key;
	int* items_slot_index;
	void* items_data;

	void* temp_key;
	void* temp_item;
	void* mem_ctx;
} cn_hashtable_t;

// -------------------------------------------------------------------------------------------------

typedef struct cn_list_node_t cn_list_node_t;

struct cn_list_node_t
{
	cn_list_node_t* next;
	cn_list_node_t* prev;
};

typedef struct cn_list_t
{
	cn_list_node_t nodes;
} cn_list_t;

#define CN_OFFSET_OF(T, member) ((size_t)((uintptr_t)(&(((T*)0)->member))))
#define CN_LIST_NODE(T, member, ptr) ((cn_list_node_t*)((uintptr_t)ptr + CN_OFFSET_OF(T, member)))
#define CN_LIST_HOST(T, member, ptr) ((T*)((uintptr_t)ptr - CN_OFFSET_OF(T, member)))

CN_INLINE void cn_list_init_node(cn_list_node_t* node)
{
	node->next = node;
	node->prev = node;
}

CN_INLINE void cn_list_init(cn_list_t* list)
{
	cn_list_init_node(&list->nodes);
}

CN_INLINE void cn_list_push_front(cn_list_t* list, cn_list_node_t* node)
{
	node->next = list->nodes.next;
	node->prev = &list->nodes;
	list->nodes.next->prev = node;
	list->nodes.next = node;
}

CN_INLINE void cn_list_push_back(cn_list_t* list, cn_list_node_t* node)
{
	node->prev = list->nodes.prev;
	node->next = &list->nodes;
	list->nodes.prev->next = node;
	list->nodes.prev = node;
}

CN_INLINE void cn_list_remove(cn_list_node_t* node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
	cn_list_init_node(node);
}

CN_INLINE cn_list_node_t* cn_list_pop_front(cn_list_t* list)
{
	cn_list_node_t* node = list->nodes.next;
	cn_list_remove(node);
	return node;
}

CN_INLINE cn_list_node_t* cn_list_pop_back(cn_list_t* list)
{
	cn_list_node_t* node = list->nodes.prev;
	cn_list_remove(node);
	return node;
}

CN_INLINE int cn_list_empty(cn_list_t* list)
{
	return list->nodes.next == list->nodes.prev && list->nodes.next == &list->nodes;
}

CN_INLINE cn_list_node_t* cn_list_begin(cn_list_t* list)
{
	return list->nodes.next;
}

CN_INLINE cn_list_node_t* cn_list_end(cn_list_t* list)
{
	return &list->nodes;
}

CN_INLINE cn_list_node_t* cn_list_front(cn_list_t* list)
{
	return list->nodes.next;
}

CN_INLINE cn_list_node_t* cn_list_back(cn_list_t* list)
{
	return list->nodes.prev;
}

// -------------------------------------------------------------------------------------------------

#define CN_PROTOCOL_CONNECT_TOKEN_ENTRIES_MAX (CN_PROTOCOL_SERVER_MAX_CLIENTS * 8)

typedef struct cn_protocol_connect_token_cache_entry_t
{
	cn_list_node_t* node;
} cn_protocol_connect_token_cache_entry_t;

typedef struct cn_protocol_connect_token_cache_node_t
{
	cn_crypto_signature_t signature;
	cn_list_node_t node;
} cn_protocol_connect_token_cache_node_t;

typedef struct cn_protocol_connect_token_cache_t
{
	int capacity;
	cn_hashtable_t table;
	cn_list_t list;
	cn_list_t free_list;
	cn_protocol_connect_token_cache_node_t* node_memory;
	void* mem_ctx;
} cn_protocol_connect_token_cache_t;

// -------------------------------------------------------------------------------------------------

#define CN_PROTOCOL_ENCRYPTION_STATES_MAX (CN_PROTOCOL_SERVER_MAX_CLIENTS * 2)

typedef struct cn_protocol_encryption_state_t
{
	uint64_t sequence;
	uint64_t expiration_timestamp;
	uint32_t handshake_timeout;
	double last_packet_recieved_time;
	double last_packet_sent_time;
	cn_crypto_key_t client_to_server_key;
	cn_crypto_key_t server_to_client_key;
	uint64_t client_id;
	cn_crypto_signature_t signature;
} cn_protocol_encryption_state_t;

typedef struct cn_protocol_encryption_map_t
{
	cn_hashtable_t table;
} cn_protocol_encryption_map_t;

// -------------------------------------------------------------------------------------------------

typedef struct cn_circular_buffer_t
{
	int index0;
	int index1;
	int size_left;
	int capacity;
	uint8_t* data;
	void* user_allocator_context;
} cn_circular_buffer_t;

cn_circular_buffer_t cn_circular_buffer_create(int initial_size_in_bytes, void* user_allocator_context)
{
	cn_circular_buffer_t buffer;
	buffer.index0 = 0;
	buffer.index1 = 0;
	buffer.size_left = initial_size_in_bytes;
	buffer.capacity = initial_size_in_bytes;
	buffer.data = (uint8_t*)CN_ALLOC(initial_size_in_bytes, user_allocator_context);
	buffer.user_allocator_context = user_allocator_context;
	return buffer;
}

void cn_circular_buffer_free(cn_circular_buffer_t* buffer)
{
	CN_FREE(buffer->data, buffer->user_allocator_context);
	CN_MEMSET(buffer, 0, sizeof(*buffer));
}

void cn_circular_buffer_reset(cn_circular_buffer_t* buffer)
{
	buffer->index0 = 0;
	buffer->index1 = 0;
	buffer->size_left = buffer->capacity;
}

int cn_circular_buffer_push(cn_circular_buffer_t* buffer, const void* data, int size)
{
	if (buffer->size_left < size) {
		return -1;
	}

	int bytes_to_end = buffer->capacity - buffer->index1;
	if (size > bytes_to_end) {
		CN_MEMCPY(buffer->data + buffer->index1, data, bytes_to_end);
		CN_MEMCPY(buffer->data, (uint8_t*)data + bytes_to_end, size - bytes_to_end);
		buffer->index1 = (size - bytes_to_end) % buffer->capacity;
	} else {
		CN_MEMCPY(buffer->data + buffer->index1, data, size);
		buffer->index1 = (buffer->index1 + size) % buffer->capacity;
	}

	buffer->size_left -= size;

	return 0;
}

int cn_circular_buffer_pull(cn_circular_buffer_t* buffer, void* data, int size)
{
	if (buffer->capacity - buffer->size_left < size) {
		return -1;
	}

	int bytes_to_end = buffer->capacity - buffer->index0;
	if (size > bytes_to_end) {
		CN_MEMCPY(data, buffer->data + buffer->index0, bytes_to_end);
		CN_MEMCPY((uint8_t*)data + bytes_to_end, buffer->data, size - bytes_to_end);
		buffer->index0 = (size - bytes_to_end) % buffer->capacity;
	} else {
		CN_MEMCPY(data, buffer->data + buffer->index0, size);
		buffer->index0 = (buffer->index0 + size) % buffer->capacity;
	}

	buffer->size_left += size;

	return 0;
}

int cn_circular_buffer_grow(cn_circular_buffer_t* buffer, int new_size_in_bytes)
{
	uint8_t* old_data = buffer->data;
	uint8_t* new_data = (uint8_t*)CN_ALLOC(new_size_in_bytes, buffer->user_allocator_context);
	if (!new_data) return -1;

	int index0 = buffer->index0;
	int index1 = buffer->index1;

	if (index0 < index1) {
		CN_MEMCPY(new_data + index0, old_data + index0, index1 - index0);
	} else {
		CN_MEMCPY(new_data, old_data, index1);
		int offset_from_end = buffer->capacity - index0;
		CN_MEMCPY(new_data + new_size_in_bytes - offset_from_end, old_data + index0, offset_from_end);
	}

	CN_FREE(old_data, buffer->user_allocator_context);
	buffer->data = new_data;
	buffer->size_left += new_size_in_bytes - buffer->capacity;
	buffer->capacity = new_size_in_bytes;

	return 0;
}

// -------------------------------------------------------------------------------------------------

#ifdef CN_WINDOWS
#	include <ws2tcpip.h>   // WSA stuff
#	include <winsock2.h>   // socket
#	pragma comment(lib, "ws2_32.lib")
#else
#	include <sys/socket.h> // socket
#	include <fcntl.h>      // fcntl
#	include <arpa/inet.h>  // inet_pton
#	include <unistd.h>     // close
#	include <errno.h>
#endif

static char* s_parse_ipv6_for_port(cn_endpoint_t* endpoint, char* str, int len)
{
	if (*str == '[') {
		int base_index = len - 1;
		for (int i = 0; i < 6; ++i)
		{
			int index = base_index - i;
			if (index < 3) return NULL;
			if (str[index] == ':') {
				endpoint->port = (uint16_t)atoi(str + index + 1);
				str[index - 1] = '\0';
				break;
			}
		}
		CN_ASSERT(*str == '[');
		++str;
	}
	return str;
}

static int s_parse_ipv4_for_port(cn_endpoint_t* endpoint, char* str)
{
	int len = (int)CN_STRLEN(str);
	int base_index = len - 1;
	for (int i = 0; i < 6; ++i)
	{
		int index = base_index - i;
		if (index < 0) break;
		if (str[index] == ':') {
			endpoint->port = (uint16_t)atoi(str + index + 1);
			str[index] = '\0';
		}
	}
	return len;
}

#define CN_ENDPOINT_STRING_MAX_LENGTH INET6_ADDRSTRLEN

int cn_endpoint_init(cn_endpoint_t* endpoint, const char* address_and_port_string)
{
	CN_ASSERT(address_and_port_string);
	CN_MEMSET(endpoint, 0, sizeof(*endpoint));

	char buffer[CN_ENDPOINT_STRING_MAX_LENGTH];
	CN_STRNCPY(buffer, address_and_port_string, CN_ENDPOINT_STRING_MAX_LENGTH - 1);
	buffer[CN_ENDPOINT_STRING_MAX_LENGTH - 1] = '\0';

	char* str = buffer;
	int len = (int)CN_STRLEN(str);

#ifndef CUTE_NET_NO_IPV6
	str = s_parse_ipv6_for_port(endpoint, str, len);
	struct in6_addr sockaddr6;
	if (inet_pton(AF_INET6, str, &sockaddr6) == 1) {
		endpoint->type = CN_ADDRESS_TYPE_IPV6;
		int i;
		for (i = 0; i < 8; ++i) {
			endpoint->u.ipv6[i] = ntohs(((uint16_t*)&sockaddr6)[i]);
		}
		return 0;
	}
#endif


	len = s_parse_ipv4_for_port(endpoint, str);

	struct sockaddr_in sockaddr4;
	if (inet_pton(AF_INET, str, &sockaddr4.sin_addr) == 1)
	{
		endpoint->type = CN_ADDRESS_TYPE_IPV4;
		endpoint->u.ipv4[3] = (uint8_t)((sockaddr4.sin_addr.s_addr & 0xFF000000) >> 24);
		endpoint->u.ipv4[2] = (uint8_t)((sockaddr4.sin_addr.s_addr & 0x00FF0000) >> 16);
		endpoint->u.ipv4[1] = (uint8_t)((sockaddr4.sin_addr.s_addr & 0x0000FF00) >> 8 );
		endpoint->u.ipv4[0] = (uint8_t)((sockaddr4.sin_addr.s_addr & 0x000000FF)      );
		return 0;
	}

	return -1;
}

void cn_endpoint_to_string(cn_endpoint_t endpoint, char* buffer, int buffer_size)
{
	CN_ASSERT(buffer);
	CN_ASSERT(buffer_size >= 0);

#ifndef CUTE_NET_NO_IPV6
	if (endpoint.type == CN_ADDRESS_TYPE_IPV6) {
		if (endpoint.port == 0) {
			uint16_t ipv6_network_order[8];
			for (int i = 0; i < 8; ++i) ipv6_network_order[i] = htons(endpoint.u.ipv6[i]);
			inet_ntop(AF_INET6, (void*)ipv6_network_order, buffer, CN_ENDPOINT_STRING_MAX_LENGTH);
		} else {
			uint16_t ipv6_network_order[8];
			for (int i = 0; i < 8; ++i) ipv6_network_order[i] = htons(endpoint.u.ipv6[i]);
			inet_ntop(AF_INET6, (void*)ipv6_network_order, buffer, INET6_ADDRSTRLEN);
			CN_SNPRINTF(buffer, CN_ENDPOINT_STRING_MAX_LENGTH, "[%s]:%d", buffer, endpoint.port);
		}
	} 
	else 
#endif
	if (endpoint.type == CN_ADDRESS_TYPE_IPV4) {
		if (endpoint.port != 0) {
			CN_SNPRINTF(buffer, CN_ENDPOINT_STRING_MAX_LENGTH, "%d.%d.%d.%d:%d",
				endpoint.u.ipv4[0],
				endpoint.u.ipv4[1],
				endpoint.u.ipv4[2],
				endpoint.u.ipv4[3],
				endpoint.port);
		} else {
			CN_SNPRINTF(buffer, CN_ENDPOINT_STRING_MAX_LENGTH, "%d.%d.%d.%d",
				endpoint.u.ipv4[0],
				endpoint.u.ipv4[1],
				endpoint.u.ipv4[2],
				endpoint.u.ipv4[3]);
		}
	} else {
		CN_SNPRINTF(buffer, CN_ENDPOINT_STRING_MAX_LENGTH, "%s", "INVALID ADDRESS");
	}
}

int cn_endpoint_equals(cn_endpoint_t a, cn_endpoint_t b)
{
	if (a.type != b.type) return 0;
	if (a.port != b.port) return 0;

	if (a.type == CN_ADDRESS_TYPE_IPV4) {
		for (int i = 0; i < 4; ++i)
			if (a.u.ipv4[i] != b.u.ipv4[i])
				return 0;
	} 
#ifndef CUTE_NET_NO_IPV6
	else if (a.type == CN_ADDRESS_TYPE_IPV6) {
		for (int i = 0; i < 8; ++i)
			if (a.u.ipv6[i] != b.u.ipv6[i])
				return 0;
	} 
#endif
	else {
		return 0;
	}

	return 1;
}

#ifdef CN_WINDOWS
	typedef SOCKET cn_socket_cn_handle_t;
#else
	typedef int cn_socket_cn_handle_t;
#endif

typedef struct cn_socket_t
{
	cn_socket_cn_handle_t handle;
	cn_endpoint_t endpoint;
} cn_socket_t;

void cn_socket_cleanup(cn_socket_t* socket)
{
	CN_ASSERT(socket);

	if (socket->handle != 0)
	{
#if CN_WINDOWS
		closesocket(socket->handle);
#else
		close(socket->handle);
#endif
		socket->handle = 0;
	}
}

static int s_socket_init(cn_socket_t* the_socket, cn_address_type_t address_type, int send_buffer_size, int receive_buffer_size)
{
#ifndef CUTE_NET_NO_IPV6
	int af = address_type == CN_ADDRESS_TYPE_IPV6 ? AF_INET6 : AF_INET;
#else
	int af = AF_INET;
#endif
	the_socket->handle = socket(af, SOCK_DGRAM, IPPROTO_UDP);

#ifdef CN_WINDOWS
	if (the_socket->handle == INVALID_SOCKET)
#else
	if (the_socket->handle <= 0)
#endif
	{
		//error_set("Failed to create socket.");
		return -1;
	}

#ifndef CUTE_NET_NO_IPV6
	// Allow users to enforce ipv6 only.
	// See: https://msdn.microsoft.com/en-us/library/windows/desktop/ms738574(v=vs.85).aspx
	if (address_type == CN_ADDRESS_TYPE_IPV6)
	{
		int enable = 1;
		if (setsockopt(the_socket->handle, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&enable, sizeof(enable)) != 0)
		{
			//error_set("Failed to strictly set socket only ipv6.");
			cn_socket_cleanup(the_socket);
			return -1;
		}
	}
#endif

	// Increase socket send buffer size.
	if (setsockopt(the_socket->handle, SOL_SOCKET, SO_SNDBUF, (char*)&send_buffer_size, sizeof(int)) != 0)
	{
		//error_set("Failed to set socket send buffer size.");
		cn_socket_cleanup(the_socket);
		return -1;
	}

	// Increase socket receive buffer size.
	if (setsockopt(the_socket->handle, SOL_SOCKET, SO_RCVBUF, (char*)&receive_buffer_size, sizeof(int)) != 0)
	{
		//error_set("Failed to set socket receive buffer size.");
		cn_socket_cleanup(the_socket);
		return -1;
	}

	return 0;
}

static int s_socket_bind_port_and_set_non_blocking(cn_socket_t* the_socket, cn_address_type_t address_type, uint16_t port)
{
	// Binding to port zero means "any port", so record which one was bound.
	if (port == 0)
	{
#ifndef CUTE_NET_NO_IPV6
		if (address_type == CN_ADDRESS_TYPE_IPV6)
		{
			struct sockaddr_in6 sin;
			socklen_t len = sizeof(sin);
			if (getsockname(the_socket->handle, (struct sockaddr*)&sin, &len) == -1)
			{
				//error_set("Failed to get ipv6 socket's assigned port number when binding to port 0.");
				cn_socket_cleanup(the_socket);
				return -1;
			}
			the_socket->endpoint.port = ntohs(sin.sin6_port);
		}
		else
#endif
		{
			struct sockaddr_in sin;
			socklen_t len = sizeof(sin);
			if (getsockname(the_socket->handle, (struct sockaddr*)&sin, &len) == -1)
			{
				//error_set("Failed to get ipv4 socket's assigned port number when binding to port 0.");
				cn_socket_cleanup(the_socket);
				return -1;
			}
			the_socket->endpoint.port = ntohs(sin.sin_port);
		}
	}

	// Set non-blocking io.
#ifdef CN_WINDOWS

	DWORD non_blocking = 1;
	if (ioctlsocket(the_socket->handle, FIONBIO, &non_blocking) != 0)
	{
		//error_set("Failed to set socket to non blocking io.");
		cn_socket_cleanup(the_socket);
		return -1;
	}

#else

	int non_blocking = 1;
	if (fcntl(the_socket->handle, F_SETFL, O_NONBLOCK, non_blocking) == -1)
	{
		//error_set("Failed to set socket to non blocking io.");
		cn_socket_cleanup(the_socket);
		return -1;
	}

#endif

	return 0;
}

int cn_socket_init1(cn_socket_t* the_socket, cn_address_type_t address_type, uint16_t port, int send_buffer_size, int receive_buffer_size)
{
	CN_MEMSET(&the_socket->endpoint, 0, sizeof(cn_endpoint_t));
	the_socket->endpoint.type = address_type;
	the_socket->endpoint.port = port;

	if (s_socket_init(the_socket, address_type, send_buffer_size, receive_buffer_size)) {
		return -1;
	}

	// Bind port.
#ifndef CUTE_NET_NO_IPV6
	if (address_type == CN_ADDRESS_TYPE_IPV6)
	{
		struct sockaddr_in6 socket_endpoint;
		CN_MEMSET(&socket_endpoint, 0, sizeof(struct sockaddr_in6));
		socket_endpoint.sin6_family = AF_INET6;
		socket_endpoint.sin6_addr = in6addr_any;
		socket_endpoint.sin6_port = htons(port);

		if (bind(the_socket->handle, (struct sockaddr*)&socket_endpoint, sizeof(socket_endpoint)) < 0)
		{
			//error_set("Failed to bind ipv6 socket.");
			cn_socket_cleanup(the_socket);
			return -1;
		}
	}
	else
#endif
	{
		struct sockaddr_in socket_endpoint;
		CN_MEMSET(&socket_endpoint, 0, sizeof(socket_endpoint));
		socket_endpoint.sin_family = AF_INET;
		socket_endpoint.sin_addr.s_addr = INADDR_ANY;
		socket_endpoint.sin_port = htons(port);

		if (bind(the_socket->handle, (struct sockaddr*)&socket_endpoint, sizeof(socket_endpoint)) < 0)
		{
			//error_set("Failed to bind ipv4 socket.");
			cn_socket_cleanup(the_socket);
			return -1;
		}
	}

	if (s_socket_bind_port_and_set_non_blocking(the_socket, address_type, port)) {
		return -1;
	}

	return 0;
}

int cn_socket_init2(cn_socket_t* the_socket, const char* address_and_port, int send_buffer_size, int receive_buffer_size)
{
	cn_endpoint_t endpoint;
	if (cn_endpoint_init(&endpoint, address_and_port)) {
		return -1;
	}

	the_socket->endpoint = endpoint;

	if (s_socket_init(the_socket, endpoint.type, send_buffer_size, receive_buffer_size)) {
		return -1;
	}

	// Bind port.
#ifndef CUTE_NET_NO_IPV6
	if (endpoint.type == CN_ADDRESS_TYPE_IPV6)
	{
		struct sockaddr_in6 socket_endpoint;
		CN_MEMSET(&socket_endpoint, 0, sizeof(struct sockaddr_in6));
		socket_endpoint.sin6_family = AF_INET6;
		for (int i = 0; i < 8; ++i) ((uint16_t*)&socket_endpoint.sin6_addr) [i] = htons(endpoint.u.ipv6[i]);
		socket_endpoint.sin6_port = htons(endpoint.port);

		if (bind(the_socket->handle, (struct sockaddr*)&socket_endpoint, sizeof(socket_endpoint)) < 0)
		{
			//error_set("Failed to bind ipv6 socket.");
			cn_socket_cleanup(the_socket);
			return -1;
		}
	}
	else
#endif
	{
		struct sockaddr_in socket_endpoint;
		CN_MEMSET(&socket_endpoint, 0, sizeof(socket_endpoint));
		socket_endpoint.sin_family = AF_INET;
		socket_endpoint.sin_addr.s_addr = (((uint32_t) endpoint.u.ipv4[0]))       |
		                                  (((uint32_t) endpoint.u.ipv4[1]) << 8)  |
		                                  (((uint32_t) endpoint.u.ipv4[2]) << 16) |
		                                  (((uint32_t) endpoint.u.ipv4[3]) << 24);
		socket_endpoint.sin_port = htons(endpoint.port);

		if (bind(the_socket->handle, (struct sockaddr*)&socket_endpoint, sizeof(socket_endpoint)) < 0)
		{
			//error_set("Failed to bind ipv4 socket.");
			cn_socket_cleanup(the_socket);
			return -1;
		}
	}

	if (s_socket_bind_port_and_set_non_blocking(the_socket, endpoint.type, endpoint.port)) {
		return -1;
	}

	return 0;
}

int cn_socket_send_internal(cn_socket_t* socket, cn_endpoint_t send_to, const void* data, int byte_count)
{
	cn_endpoint_t endpoint = send_to;
	CN_ASSERT(data);
	CN_ASSERT(byte_count >= 0);
	CN_ASSERT(socket->handle != 0);
	CN_ASSERT(endpoint.type != CN_ADDRESS_TYPE_NONE);

#ifndef CUTE_NET_NO_IPV6
	if (endpoint.type == CN_ADDRESS_TYPE_IPV6)
	{
		struct sockaddr_in6 socket_address;
		CN_MEMSET(&socket_address, 0, sizeof(socket_address));
		socket_address.sin6_family = AF_INET6;
		int i;
		for (i = 0; i < 8; ++i)
		{
			((uint16_t*) &socket_address.sin6_addr) [i] = htons(endpoint.u.ipv6[i]);
		}
		socket_address.sin6_port = htons(endpoint.port);
		int result = sendto(socket->handle, (const char*)data, byte_count, 0, (struct sockaddr*)&socket_address, sizeof(socket_address));
		return result;
	}
	else 
#endif
	if (endpoint.type == CN_ADDRESS_TYPE_IPV4)
	{
		struct sockaddr_in socket_address;
		CN_MEMSET(&socket_address, 0, sizeof(socket_address));
		socket_address.sin_family = AF_INET;
		socket_address.sin_addr.s_addr = (((uint32_t)endpoint.u.ipv4[0]))        |
		                                 (((uint32_t)endpoint.u.ipv4[1]) << 8)   |
		                                 (((uint32_t)endpoint.u.ipv4[2]) << 16)  |
		                                 (((uint32_t)endpoint.u.ipv4[3]) << 24);
		socket_address.sin_port = htons(endpoint.port);
		int result = sendto(socket->handle, (const char*)data, byte_count, 0, (struct sockaddr*)&socket_address, sizeof(socket_address));
		return result;
	}

	return -1;
}

int cn_socket_receive(cn_socket_t* the_socket, cn_endpoint_t* from, void* data, int byte_count)
{
	CN_ASSERT(the_socket);
	CN_ASSERT(the_socket->handle != 0);
	CN_ASSERT(from);
	CN_ASSERT(data);
	CN_ASSERT(byte_count >= 0);

#ifdef CN_WINDOWS
	typedef int socklen_t;
#endif

	CN_MEMSET(from, 0, sizeof(*from));

	struct sockaddr_storage sockaddr_from;
	socklen_t from_length = sizeof(sockaddr_from);
	int result = recvfrom(the_socket->handle, (char*)data, byte_count, 0, (struct sockaddr*)&sockaddr_from, &from_length);

#ifdef CN_WINDOWS
	if (result == SOCKET_ERROR)
	{
		int error = WSAGetLastError();
		if (error == WSAEWOULDBLOCK || error == WSAECONNRESET) return 0;
		//error_set("The function recvfrom failed.");
		return -1;
	}
#else
	if (result <= 0)
	{
		if (errno == EAGAIN) return 0;
		//error_set("The function recvfrom failed.");
		return -1;
	}
#endif

#ifndef CUTE_NET_NO_IPV6
	if (sockaddr_from.ss_family == AF_INET6)
	{
		struct sockaddr_in6* addr_ipv6 = (struct sockaddr_in6*) &sockaddr_from;
		from->type = CN_ADDRESS_TYPE_IPV6;
		int i;
		for (i = 0; i < 8; ++i)
		{
			from->u.ipv6[i] = ntohs(((uint16_t*) &addr_ipv6->sin6_addr) [i]);
		}
		from->port = ntohs(addr_ipv6->sin6_port);
	}
	else
#endif
	if (sockaddr_from.ss_family == AF_INET)
	{
		struct sockaddr_in* addr_ipv4 = (struct sockaddr_in*) &sockaddr_from;
		from->type = CN_ADDRESS_TYPE_IPV4;
		from->u.ipv4[0] = (uint8_t)((addr_ipv4->sin_addr.s_addr & 0x000000FF));
		from->u.ipv4[1] = (uint8_t)((addr_ipv4->sin_addr.s_addr & 0x0000FF00) >> 8);
		from->u.ipv4[2] = (uint8_t)((addr_ipv4->sin_addr.s_addr & 0x00FF0000) >> 16);
		from->u.ipv4[3] = (uint8_t)((addr_ipv4->sin_addr.s_addr & 0xFF000000) >> 24);
		from->port = ntohs(addr_ipv4->sin_port);
	}
	else
	{
		CN_ASSERT(0);
		//error_set("The function recvfrom returned an invalid ip format.");
		return -1;
	}

	CN_ASSERT(result >= 0);
	int bytes_read = result;
	return bytes_read;
}

// -------------------------------------------------------------------------------------------------

typedef struct cn_simulator_t cn_simulator_t;

struct cn_protocol_client_t
{
	bool use_ipv6;
	uint16_t port;
	cn_protocol_client_state_t state;
	double last_packet_recieved_time;
	double last_packet_sent_time;
	uint64_t application_id;
	uint64_t current_time;
	uint64_t client_id;
	int max_clients;
	double connection_timeout;
	int has_sent_disconnect_packets;
	cn_protocol_connect_token_t connect_token;
	uint64_t challenge_nonce;
	uint8_t challenge_data[CN_PROTOCOL_CHALLENGE_DATA_SIZE];
	int goto_next_server;
	cn_protocol_client_state_t goto_next_server_tentative_state;
	int server_endpoint_index;
	cn_endpoint_t web_service_endpoint;
	cn_socket_t socket;
	uint64_t sequence;
	cn_circular_buffer_t packet_queue;
	cn_protocol_replay_buffer_t replay_buffer;
	cn_simulator_t* sim;
	uint8_t buffer[CN_PROTOCOL_PACKET_SIZE_MAX];
	uint8_t connect_token_packet[CN_PROTOCOL_CONNECT_TOKEN_PACKET_SIZE];
	void* mem_ctx;
};

// -------------------------------------------------------------------------------------------------

struct cn_protocol_server_t
{
	bool running;
	uint64_t application_id;
	uint64_t current_time;
	cn_socket_t socket;
	cn_protocol_packet_allocator_t* packet_allocator;
	cn_crypto_sign_public_t public_key;
	cn_crypto_sign_secret_t secret_key;
	uint32_t connection_timeout;
	cn_circular_buffer_t event_queue;
	cn_simulator_t* sim;

	uint64_t challenge_nonce;
	cn_protocol_encryption_map_t encryption_map;
	cn_protocol_connect_token_cache_t token_cache;

	int client_count;
	cn_hashtable_t client_endpoint_table;
	cn_hashtable_t client_id_table;
	uint64_t client_id[CN_PROTOCOL_SERVER_MAX_CLIENTS];
	bool client_is_connected[CN_PROTOCOL_SERVER_MAX_CLIENTS];
	bool client_is_confirmed[CN_PROTOCOL_SERVER_MAX_CLIENTS];
	double client_last_packet_received_time[CN_PROTOCOL_SERVER_MAX_CLIENTS];
	double client_last_packet_sent_time[CN_PROTOCOL_SERVER_MAX_CLIENTS];
	cn_endpoint_t client_endpoint[CN_PROTOCOL_SERVER_MAX_CLIENTS];
	uint64_t client_sequence[CN_PROTOCOL_SERVER_MAX_CLIENTS];
	cn_crypto_key_t client_client_to_server_key[CN_PROTOCOL_SERVER_MAX_CLIENTS];
	cn_crypto_key_t client_server_to_client_key[CN_PROTOCOL_SERVER_MAX_CLIENTS];
	cn_protocol_replay_buffer_t client_replay_buffer[CN_PROTOCOL_SERVER_MAX_CLIENTS];

	uint8_t buffer[CN_PROTOCOL_PACKET_SIZE_MAX];
	void* mem_ctx;
};

// -------------------------------------------------------------------------------------------------

#include <inttypes.h>

static uint8_t s_crypto_ctx[] = "CUTE_CTX";
#define CN_CHECK(X) if (X) ret = -1;
#define CN_CRYPTO_CONTEXT (const char*)s_crypto_ctx

static bool s_cn_is_init = false;

cn_error_t cn_crypto_init()
{
	if (hydro_init() != 0) {
		return cn_error_failure("Unable to initialize crypto library. It is *not safe* to connect to the net.");
	}
	return cn_error_success();
}

cn_error_t cn_init()
{
#ifdef CN_WINDOWS
	WSADATA wsa_data;
	if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != NO_ERROR) {
		return cn_error_failure("Unable to initialize WSA.");
	}
#else
#endif
	return cn_crypto_init();
}

static CN_INLINE cn_error_t s_cn_init_check()
{
	if (!s_cn_is_init) {
		if (cn_is_error(cn_init())) {
			return cn_error_failure("Unable to initialization Cute Net.");
		} else {
			s_cn_is_init = true;
		}
	}
	return cn_error_success();
}

void cn_crypto_encrypt(const cn_crypto_key_t* key, uint8_t* data, int data_size, uint64_t msg_id)
{
	hydro_secretbox_encrypt(data, data, (uint64_t)data_size, msg_id, CN_CRYPTO_CONTEXT, key->key);
}

cn_error_t cn_crypto_decrypt(const cn_crypto_key_t* key, uint8_t* data, int data_size, uint64_t msg_id)
{
	if (hydro_secretbox_decrypt(data, data, (size_t)data_size, msg_id, CN_CRYPTO_CONTEXT, key->key) != 0) {
		return cn_error_failure("Message forged.");
	} else {
		return cn_error_success();
	}
}

cn_crypto_key_t cn_crypto_generate_key()
{
	s_cn_init_check();
	cn_crypto_key_t key;
	hydro_secretbox_keygen(key.key);
	return key;
}

void cn_crypto_random_bytes(void* data, int byte_count)
{
	s_cn_init_check();
	hydro_random_buf(data, byte_count);
}

void cn_crypto_sign_keygen(cn_crypto_sign_public_t* public_key, cn_crypto_sign_secret_t* secret_key)
{
	s_cn_init_check();
	hydro_sign_keypair key_pair;
	hydro_sign_keygen(&key_pair);
	CN_MEMCPY(public_key->key, key_pair.pk, 32);
	CN_MEMCPY(secret_key->key, key_pair.sk, 64);
}

void cn_crypto_sign_create(const cn_crypto_sign_secret_t* secret_key, cn_crypto_signature_t* signature, const uint8_t* data, int data_size)
{
	hydro_sign_create(signature->bytes, data, (size_t)data_size, CN_CRYPTO_CONTEXT, secret_key->key);
}

cn_error_t cn_crypto_sign_verify(const cn_crypto_sign_public_t* public_key, const cn_crypto_signature_t* signature, const uint8_t* data, int data_size)
{
	if (hydro_sign_verify(signature->bytes, data, (size_t)data_size, CN_CRYPTO_CONTEXT, public_key->key) != 0) {
		return cn_error_failure("Message forged.");
	} else {
		return cn_error_success();
	}
}

void cn_cleanup()
{
#ifdef CN_WINDOWS
	WSACleanup();
#endif
}

// -------------------------------------------------------------------------------------------------

#define CN_PROTOCOL_NET_SIMULATOR_MAX_PACKETS (1024 * 5)

typedef struct cn_rnd_t
{
	uint64_t state[2];
} cn_rnd_t;

static uint64_t cn_rnd_murmur3_avalanche64(uint64_t h)
{
	h ^= h >> 33;
	h *= 0xff51afd7ed558ccd;
	h ^= h >> 33;
	h *= 0xc4ceb9fe1a85ec53;
	h ^= h >> 33;
	return h;
}

static cn_rnd_t cn_rnd_seed(uint64_t seed)
{
	cn_rnd_t rnd;
	uint64_t value = cn_rnd_murmur3_avalanche64((seed << 1ULL) | 1ULL);
	rnd.state[0] = value;
	rnd.state[1] = cn_rnd_murmur3_avalanche64(value);
	return rnd;
}

static uint64_t cn_rnd_next(cn_rnd_t* rnd)
{
	uint64_t x = rnd->state[0];
	uint64_t y = rnd->state[1];
	rnd->state[0] = y;
	x ^= x << 23;
	x ^= x >> 17;
	x ^= y ^ (y >> 26);
	rnd->state[1] = x;
	return x + y;
}

static double cn_rnd_next_double(cn_rnd_t* rnd)
{
	uint64_t value = cn_rnd_next(rnd);
	uint64_t exponent = 1023;
	uint64_t mantissa = value >> 12;
	uint64_t result = (exponent << 52) | mantissa;
	return *(double*)&result - 1.0;
}

typedef struct cn_simulator_packet_t
{
	double delay;
	cn_endpoint_t to;
	void* data;
	int size;
} cn_simulator_packet_t;

struct cn_simulator_t
{
	cn_socket_t* socket;
	double latency;
	double jitter;
	double drop_chance;
	double duplicate_chance;
	cn_rnd_t rnd;
	int index;
	void* mem_ctx;
	cn_simulator_packet_t packets[CN_PROTOCOL_NET_SIMULATOR_MAX_PACKETS];
};

cn_simulator_t* cn_simulator_create(cn_socket_t* socket, void* mem_ctx)
{
	cn_simulator_t* sim = (cn_simulator_t*)CN_ALLOC(sizeof(cn_simulator_t), mem_ctx);
	CN_MEMSET(sim, 0, sizeof(*sim));
	sim->socket = socket;
	sim->rnd = cn_rnd_seed(0);
	sim->mem_ctx = mem_ctx;
	return sim;
}

void cn_simulator_destroy(cn_simulator_t* sim)
{
	if (!sim) return;
	for (int i = 0; i < CN_PROTOCOL_NET_SIMULATOR_MAX_PACKETS; ++i) {
		cn_simulator_packet_t* p = sim->packets + i;
		if (p->data) CN_FREE(p->data, sim->mem_ctx);
	}
	CN_FREE(sim, sim->mem_ctx);
}

void cn_simulator_add(cn_simulator_t* sim, cn_endpoint_t to, const void* packet, int size)
{
	bool drop = cn_rnd_next_double(&sim->rnd) < sim->drop_chance;
	if (drop) return;

	int index = sim->index++ % CN_PROTOCOL_NET_SIMULATOR_MAX_PACKETS;
	cn_simulator_packet_t* p = sim->packets + index;
	if (p->data) CN_FREE(p->data, sim->mem_ctx);
	p->delay = sim->latency + cn_rnd_next_double(&sim->rnd) * sim->jitter;
	p->to = to;
	p->data = CN_ALLOC(size, sim->mem_ctx);
	p->size = size;
	CN_MEMCPY(p->data, packet, size);
}

void cn_simulator_update(cn_simulator_t* sim, double dt)
{
	if (!sim) return;
	for (int i = 0; i < CN_PROTOCOL_NET_SIMULATOR_MAX_PACKETS; ++i) {
		cn_simulator_packet_t* p = sim->packets + i;
		if (p->data) {
			p->delay -= dt;
			if (p->delay < 0) {
				cn_socket_send_internal(sim->socket, p->to, p->data, p->size);
				bool duplicate = cn_rnd_next_double(&sim->rnd) < sim->duplicate_chance;
				if (!duplicate) {
					CN_FREE(p->data, sim->mem_ctx);
					p->data = NULL;
				} else {
					p->delay = cn_rnd_next_double(&sim->rnd) * sim->jitter;
				}
			}
		}
	}
}

int cn_socket_send(cn_socket_t* socket, cn_simulator_t* sim, cn_endpoint_t to, const void* data, int size)
{
	if (sim) {
		cn_simulator_add(sim, to, data, size);
		return size;
	} else {
		return cn_socket_send_internal(socket, to, data, size);
	}
}

// -------------------------------------------------------------------------------------------------

cn_error_t cn_generate_connect_token(
	uint64_t application_id,
	uint64_t creation_timestamp,
	const cn_crypto_key_t* client_to_server_key,
	const cn_crypto_key_t* server_to_client_key,
	uint64_t expiration_timestamp,
	uint32_t handshake_timeout,
	int address_count,
	const char** address_list,
	uint64_t client_id,
	const uint8_t* user_data,
	const cn_crypto_sign_secret_t* shared_secret_key,
	uint8_t* token_ptr_out
)
{
	cn_error_t err = s_cn_init_check();
	if (cn_is_error(err)) return err;

	CN_ASSERT(address_count >= 1 && address_count <= 32);
	CN_ASSERT(creation_timestamp < expiration_timestamp);

	uint8_t** p = &token_ptr_out;

	// Write the REST SECTION.
	cn_write_bytes(p, CN_PROTOCOL_VERSION_STRING, CN_PROTOCOL_VERSION_STRING_LEN);
	cn_write_uint64(p, application_id);
	cn_write_uint64(p, creation_timestamp);
	cn_write_key(p, client_to_server_key);
	cn_write_key(p, server_to_client_key);

	// Write the PUBLIC SECTION.
	uint8_t* public_section = *p;
	cn_write_uint8(p, 0);
	cn_write_bytes(p, CN_PROTOCOL_VERSION_STRING, CN_PROTOCOL_VERSION_STRING_LEN);
	cn_write_uint64(p, application_id);
	cn_write_uint64(p, expiration_timestamp);
	cn_write_uint32(p, handshake_timeout);
	cn_write_uint32(p, (uint32_t)address_count);
	for (int i = 0; i < address_count; ++i)
	{
		cn_endpoint_t endpoint;
		if (cn_endpoint_init(&endpoint, address_list[i])) return cn_error_failure("Unable to initialize endpoint.");
		cn_write_endpoint(p, endpoint);
	}

	int bytes_written = (int)(*p - public_section);
	CN_ASSERT(bytes_written <= 568);
	int zeroes = 568 - bytes_written;
	for (int i = 0; i < zeroes; ++i)
		cn_write_uint8(p, 0);

	bytes_written = (int)(*p - public_section);
	CN_ASSERT(bytes_written == 568);

	// Write the SECRET SECTION.
	uint8_t* secret_section = *p;
	CN_MEMSET(*p, 0, CN_PROTOCOL_SIGNATURE_SIZE - CN_CRYPTO_HEADER_BYTES);
	*p += CN_PROTOCOL_SIGNATURE_SIZE - CN_CRYPTO_HEADER_BYTES;
	cn_write_uint64(p, client_id);
	cn_write_key(p, client_to_server_key);
	cn_write_key(p, server_to_client_key);
	if (user_data) {
		CN_MEMCPY(*p, user_data, CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE);
	} else {
		CN_MEMSET(*p, 0, CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE);
	}
	*p += CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE;

	// Encrypt the SECRET SECTION.
	cn_crypto_encrypt((const cn_crypto_key_t*)shared_secret_key, secret_section, CN_PROTOCOL_CONNECT_TOKEN_SECRET_SECTION_SIZE - CN_CRYPTO_HEADER_BYTES, 0);
	*p += CN_CRYPTO_HEADER_BYTES;

	// Compute the signature.
	cn_crypto_signature_t signature;
	cn_crypto_sign_create(shared_secret_key, &signature, public_section, 1024 - CN_PROTOCOL_SIGNATURE_SIZE);

	// Write the signature.
	CN_MEMCPY(*p, signature.bytes, sizeof(signature));
	*p += sizeof(signature);
	bytes_written = (int)(*p - public_section);
	CN_ASSERT(bytes_written == CN_PROTOCOL_CONNECT_TOKEN_PACKET_SIZE);

	return cn_error_success();
}

// -------------------------------------------------------------------------------------------------

void cn_protocol_replay_buffer_init(cn_protocol_replay_buffer_t* replay_buffer)
{
	replay_buffer->max = 0;
	CN_MEMSET(replay_buffer->entries, ~0, sizeof(uint64_t) * CN_PROTOCOL_REPLAY_BUFFER_SIZE);
}

int cn_protocol_replay_buffer_cull_duplicate(cn_protocol_replay_buffer_t* replay_buffer, uint64_t sequence)
{
	if (sequence + CN_PROTOCOL_REPLAY_BUFFER_SIZE < replay_buffer->max) {
		// This is UDP - just drop old packets.
		return -1;
	}

	int index = (int)(sequence % CN_PROTOCOL_REPLAY_BUFFER_SIZE);
	uint64_t val = replay_buffer->entries[index];
	int empty_slot = val == ~0ULL;
	int outdated = val >= sequence;
	if (empty_slot | !outdated) {
		return 0;
	} else {
		// Duplicate or replayed packet detected.
		return -1;
	}
}

void cn_protocol_replay_buffer_update(cn_protocol_replay_buffer_t* replay_buffer, uint64_t sequence)
{
	if (replay_buffer->max < sequence) {
		replay_buffer->max = sequence;
	}

	int index = (int)(sequence % CN_PROTOCOL_REPLAY_BUFFER_SIZE);
	uint64_t val = replay_buffer->entries[index];
	int empty_slot = val == ~0ULL;
	int outdated = val >= sequence;
	if (empty_slot | !outdated) {
		replay_buffer->entries[index] = sequence;
	}
}

// -------------------------------------------------------------------------------------------------

typedef struct cn_memory_pool_t
{
	int element_size;
	int arena_size;
	uint8_t* arena;
	void* free_list;
	int overflow_count;
	void* mem_ctx;
} cn_memory_pool_t;

cn_memory_pool_t* cn_memory_pool_create(int element_size, int element_count, void* user_allocator_context)
{
	size_t stride = (size_t)element_size > sizeof(void*) ? element_size : sizeof(void*);
	size_t arena_size = sizeof(cn_memory_pool_t) + stride * element_count;
	cn_memory_pool_t* pool = (cn_memory_pool_t*)CN_ALLOC(arena_size, user_allocator_context);

	pool->element_size = element_size;
	pool->arena_size = (int)(arena_size - sizeof(cn_memory_pool_t));
	pool->arena = (uint8_t*)(pool + 1);
	pool->free_list = pool->arena;
	pool->overflow_count = 0;

	for (int i = 0; i < element_count - 1; ++i)
	{
		void** element = (void**)(pool->arena + stride * i);
		void* next = (void*)(pool->arena + stride * (i + 1));
		*element = next;
	};

	void** last_element = (void**)(pool->arena + stride * (element_count - 1));
	*last_element = NULL;

	return pool;
}

void cn_memory_pool_destroy(cn_memory_pool_t* pool)
{
	if (pool->overflow_count) {
		// Attempted to destroy pool without freeing all overflow allocations.
		CN_ASSERT(pool->overflow_count == 0);
	}
	CN_FREE(pool, pool->mem_ctx);
}

void* cn_memory_pool_try_alloc(cn_memory_pool_t* pool)
{
	if (pool->free_list) {
		void *mem = pool->free_list;
		pool->free_list = *((void**)pool->free_list);
		return mem;
	} else {
		return NULL;
	}
}

void* cn_memory_pool_alloc(cn_memory_pool_t* pool)
{
	void *mem = cn_memory_pool_try_alloc(pool);
	if (!mem) {
		mem = CN_ALLOC(pool->element_size, pool->mem_ctx);
		if (mem) {
			pool->overflow_count++;
		}
	}
	return mem;
}

void cn_memory_pool_free(cn_memory_pool_t* pool, void* element)
{
	int difference = (int)((uint8_t*)element - pool->arena);
	int in_bounds = difference < pool->arena_size;
	if (pool->overflow_count && !in_bounds) {
		CN_FREE(element, pool->mem_ctx);
		pool->overflow_count--;
	} else if (in_bounds) {
		*(void**)element = pool->free_list;
		pool->free_list = element;
	} else {
		// Pointer was outside of arena bounds, or a double free was detected.
		CN_ASSERT(0);
	}
}

// -------------------------------------------------------------------------------------------------

static int s_packet_size(cn_protocol_packet_type_t type)
{
	int size = 0;

	switch (type)
	{
	case CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN:
		size = sizeof(cn_protocol_packet_connect_token_t);
		break;

	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED:
		size = sizeof(cn_protocol_packet_connection_accepted_t);
		break;

	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED:
		size = sizeof(cn_protocol_packet_connection_denied_t);
		break;

	case CN_PROTOCOL_PACKET_TYPE_KEEPALIVE:
		size = sizeof(cn_protocol_packet_keepalive_t);
		break;

	case CN_PROTOCOL_PACKET_TYPE_DISCONNECT:
		size = sizeof(cn_protocol_packet_disconnect_t);
		break;

	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST: // fall-thru
	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE:
		size = sizeof(cn_protocol_packet_challenge_t);
		break;

	case CN_PROTOCOL_PACKET_TYPE_PAYLOAD:
		size = sizeof(cn_protocol_packet_payload_t);
		break;

	default:
		size = -1;
		CN_ASSERT(false);
		break;
	}

	return size;
}

struct cn_protocol_packet_allocator_t
{
	cn_memory_pool_t* pools[CN_PROTOCOL_PACKET_TYPE_COUNT];
	void* user_allocator_context;
};

cn_protocol_packet_allocator_t* cn_protocol_packet_allocator_create(void* user_allocator_context)
{
	cn_protocol_packet_allocator_t* packet_allocator = (cn_protocol_packet_allocator_t*)CN_ALLOC(sizeof(cn_protocol_packet_allocator_t), user_allocator_context);
	packet_allocator->pools[CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN] = cn_memory_pool_create(s_packet_size(CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN), 256, user_allocator_context);
	packet_allocator->pools[CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED] = cn_memory_pool_create(s_packet_size(CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED), 256, user_allocator_context);
	packet_allocator->pools[CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED] = cn_memory_pool_create(s_packet_size(CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED), 256, user_allocator_context);
	packet_allocator->pools[CN_PROTOCOL_PACKET_TYPE_KEEPALIVE] = cn_memory_pool_create(s_packet_size(CN_PROTOCOL_PACKET_TYPE_KEEPALIVE), 1024, user_allocator_context);
	packet_allocator->pools[CN_PROTOCOL_PACKET_TYPE_DISCONNECT] = cn_memory_pool_create(s_packet_size(CN_PROTOCOL_PACKET_TYPE_DISCONNECT), 256, user_allocator_context);
	packet_allocator->pools[CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST] = cn_memory_pool_create(s_packet_size(CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST), 256, user_allocator_context);
	packet_allocator->pools[CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE] = cn_memory_pool_create(s_packet_size(CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE), 256, user_allocator_context);
	packet_allocator->pools[CN_PROTOCOL_PACKET_TYPE_PAYLOAD] = cn_memory_pool_create(s_packet_size(CN_PROTOCOL_PACKET_TYPE_PAYLOAD), 1024 * 10, user_allocator_context);
	packet_allocator->user_allocator_context = user_allocator_context;
	return packet_allocator;
}

void cn_protocol_packet_allocator_destroy(cn_protocol_packet_allocator_t* packet_allocator)
{
	for (int i = 0; i < CN_PROTOCOL_PACKET_TYPE_COUNT; ++i) {
		cn_memory_pool_destroy(packet_allocator->pools[i]);
	}
	CN_FREE(packet_allocator, packet_allocator->user_allocator_context);
}

void* cn_protocol_packet_allocator_alloc(cn_protocol_packet_allocator_t* packet_allocator, cn_protocol_packet_type_t type)
{
	if (!packet_allocator) {
		return CN_ALLOC(s_packet_size(type), NULL);
	} else {
		void* packet = cn_memory_pool_alloc(packet_allocator->pools[type]);
		return packet;
	}
}

void cn_protocol_packet_allocator_free(cn_protocol_packet_allocator_t* packet_allocator, cn_protocol_packet_type_t type, void* packet)
{
	if (!packet_allocator) {
		CN_FREE(packet, NULL);
	} else {
		cn_memory_pool_free(packet_allocator->pools[type], packet);
	}
}

// -------------------------------------------------------------------------------------------------

cn_error_t cn_protocol_read_connect_token_packet_public_section(uint8_t* buffer, uint64_t application_id, uint64_t current_time, cn_protocol_packet_connect_token_t* packet)
{
	uint8_t* buffer_start = buffer;

	// Read public section.
	packet->packet_type = (cn_protocol_packet_type_t)cn_read_uint8(&buffer);
	if (packet->packet_type != CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN) return cn_error_failure("Expected packet type to be CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN.");
	if (CN_STRNCMP((const char*)buffer, (const char*)CN_PROTOCOL_VERSION_STRING, CN_PROTOCOL_VERSION_STRING_LEN)) {
		return cn_error_failure("Unable to find `CN_PROTOCOL_VERSION_STRING` string.");
	}
	buffer += CN_PROTOCOL_VERSION_STRING_LEN;
	if (cn_read_uint64(&buffer) != application_id) return cn_error_failure("Found invalid application id.");
	packet->expiration_timestamp = cn_read_uint64(&buffer);
	if (packet->expiration_timestamp < current_time) return cn_error_failure("Packet has expired.");
	packet->handshake_timeout = cn_read_uint32(&buffer);
	packet->endpoint_count = cn_read_uint32(&buffer);
	int count = (int)packet->endpoint_count;
	if (count <= 0 || count > 32) return cn_error_failure("Invalid endpoint count.");
	for (int i = 0; i < count; ++i)
		packet->endpoints[i] = cn_read_endpoint(&buffer);
	int bytes_read = (int)(buffer - buffer_start);
	CN_ASSERT(bytes_read <= 568);
	buffer += 568 - bytes_read;
	bytes_read = (int)(buffer - buffer_start);
	CN_ASSERT(bytes_read == 568);

	return cn_error_success();
}

static uint8_t* s_protocol_header(uint8_t** p, uint8_t type, uint64_t sequence)
{
	cn_write_uint8(p, type);
	cn_write_uint64(p, sequence);
	CN_MEMSET(*p, 0, CN_PROTOCOL_SIGNATURE_SIZE - CN_CRYPTO_HEADER_BYTES);
	*p += CN_PROTOCOL_SIGNATURE_SIZE - CN_CRYPTO_HEADER_BYTES;
	return *p;
}

int cn_protocol_packet_write(void* packet_ptr, uint8_t* buffer, uint64_t sequence, const cn_crypto_key_t* key)
{
	uint8_t type = *(uint8_t*)packet_ptr;

	if (type == CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN) {
		CN_MEMCPY(buffer, packet_ptr, CN_PROTOCOL_CONNECT_TOKEN_PACKET_SIZE);
		return CN_PROTOCOL_CONNECT_TOKEN_PACKET_SIZE;
	}

	uint8_t* buffer_start = buffer;
	uint8_t* payload = s_protocol_header(&buffer, type, sequence);
	int payload_size = 0;

	switch (type)
	{
	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED:
	{
		cn_protocol_packet_connection_accepted_t* packet = (cn_protocol_packet_connection_accepted_t*)packet_ptr;
		cn_write_uint64(&buffer, packet->client_id);
		cn_write_uint32(&buffer, packet->max_clients);
		cn_write_uint32(&buffer, packet->connection_timeout);
		payload_size = (int)(buffer - payload);
		CN_ASSERT(payload_size == 8 + 4 + 4);
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED:
	{
		cn_protocol_packet_connection_denied_t* packet = (cn_protocol_packet_connection_denied_t*)packet_ptr;
		payload_size = (int)(buffer - payload);
		CN_ASSERT(payload_size == 0);
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_KEEPALIVE:
	{
		cn_protocol_packet_keepalive_t* packet = (cn_protocol_packet_keepalive_t*)packet_ptr;
		payload_size = (int)(buffer - payload);
		CN_ASSERT(payload_size == 0);
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_DISCONNECT:
	{
		cn_protocol_packet_disconnect_t* packet = (cn_protocol_packet_disconnect_t*)packet_ptr;
		payload_size = (int)(buffer - payload);
		CN_ASSERT(payload_size == 0);
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST: // fall-thru
	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE:
	{
		cn_protocol_packet_challenge_t* packet = (cn_protocol_packet_challenge_t*)packet_ptr;
		cn_write_uint64(&buffer, packet->challenge_nonce);
		CN_MEMCPY(buffer, packet->challenge_data, CN_PROTOCOL_CHALLENGE_DATA_SIZE);
		buffer += CN_PROTOCOL_CHALLENGE_DATA_SIZE;
		payload_size = (int)(buffer - payload);
		CN_ASSERT(payload_size == 8 + CN_PROTOCOL_CHALLENGE_DATA_SIZE);
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_PAYLOAD:
	{
		cn_protocol_packet_payload_t* packet = (cn_protocol_packet_payload_t*)packet_ptr;
		cn_write_uint16(&buffer, packet->payload_size);
		CN_MEMCPY(buffer, packet->payload, packet->payload_size);
		buffer += packet->payload_size;
		payload_size = (int)(buffer - payload);
		CN_ASSERT(payload_size == 2 + packet->payload_size);
	}	break;
	}

	cn_crypto_encrypt(key, payload, payload_size, sequence);

	size_t written = buffer - buffer_start;
	return (int)(written) + CN_CRYPTO_HEADER_BYTES;
}

void* cn_protocol_packet_open(uint8_t* buffer, int size, const cn_crypto_key_t* key, cn_protocol_packet_allocator_t* pa, cn_protocol_replay_buffer_t* replay_buffer, uint64_t* sequence_ptr)
{
	int ret = 0;
	uint8_t* buffer_start = buffer;
	uint8_t type = cn_read_uint8(&buffer);

	switch (type)
	{
	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED: CN_CHECK(size != 16 + 73); if (ret) return NULL; break;
	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED: CN_CHECK(size != 73); if (ret) return NULL; break;
	case CN_PROTOCOL_PACKET_TYPE_KEEPALIVE: CN_CHECK(size != 73); if (ret) return NULL; break;
	case CN_PROTOCOL_PACKET_TYPE_DISCONNECT: CN_CHECK(size != 73); if (ret) return NULL; break;
	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST: CN_CHECK(size != 264 + 73); if (ret) return NULL; break;
	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE: CN_CHECK(size != 264 + 73); if (ret) return NULL; break;
	case CN_PROTOCOL_PACKET_TYPE_PAYLOAD: CN_CHECK((size - 73 < 1) | (size - 73 > 1255)); if (ret) return NULL; break;
	}

	uint64_t sequence = cn_read_uint64(&buffer);
	int bytes_read = (int)(buffer - buffer_start);
	CN_ASSERT(bytes_read == 1 + 8);

	if (replay_buffer) {
		CN_CHECK(cn_protocol_replay_buffer_cull_duplicate(replay_buffer, sequence));
		if (ret) return NULL;
	}

	buffer += CN_PROTOCOL_SIGNATURE_SIZE - CN_CRYPTO_HEADER_BYTES;
	bytes_read = (int)(buffer - buffer_start);
	CN_ASSERT(bytes_read == 1 + 8 + CN_PROTOCOL_SIGNATURE_SIZE - CN_CRYPTO_HEADER_BYTES);
	if (cn_is_error(cn_crypto_decrypt(key, buffer, size - 37, sequence))) return NULL;

	if (replay_buffer) {
		cn_protocol_replay_buffer_update(replay_buffer, sequence);
	}

	if (sequence_ptr) {
		*sequence_ptr = sequence;
	}

	switch (type)
	{
	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED:
	{
		cn_protocol_packet_connection_accepted_t* packet = (cn_protocol_packet_connection_accepted_t*)cn_protocol_packet_allocator_alloc(pa, (cn_protocol_packet_type_t)type);
		packet->packet_type = type;
		packet->client_id = cn_read_uint64(&buffer);
		packet->max_clients = cn_read_uint32(&buffer);
		packet->connection_timeout = cn_read_uint32(&buffer);
		return packet;
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED:
	{
		cn_protocol_packet_connection_denied_t* packet = (cn_protocol_packet_connection_denied_t*)cn_protocol_packet_allocator_alloc(pa, (cn_protocol_packet_type_t)type);
		packet->packet_type = type;
		return packet;
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_KEEPALIVE:
	{
		cn_protocol_packet_keepalive_t* packet = (cn_protocol_packet_keepalive_t*)cn_protocol_packet_allocator_alloc(pa, (cn_protocol_packet_type_t)type);
		packet->packet_type = type;
		return packet;
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_DISCONNECT:
	{
		cn_protocol_packet_disconnect_t* packet = (cn_protocol_packet_disconnect_t*)cn_protocol_packet_allocator_alloc(pa, (cn_protocol_packet_type_t)type);
		packet->packet_type = type;
		return packet;
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST: // fall-thru
	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE:
	{
		cn_protocol_packet_challenge_t* packet = (cn_protocol_packet_challenge_t*)cn_protocol_packet_allocator_alloc(pa, (cn_protocol_packet_type_t)type);
		packet->packet_type = type;
		packet->challenge_nonce = cn_read_uint64(&buffer);
		CN_MEMCPY(packet->challenge_data, buffer, CN_PROTOCOL_CHALLENGE_DATA_SIZE);
		return packet;
	}	break;

	case CN_PROTOCOL_PACKET_TYPE_PAYLOAD:
	{
		cn_protocol_packet_payload_t* packet = (cn_protocol_packet_payload_t*)cn_protocol_packet_allocator_alloc(pa, (cn_protocol_packet_type_t)type);
		packet->packet_type = type;
		packet->payload_size = cn_read_uint16(&buffer);
		CN_MEMCPY(packet->payload, buffer, packet->payload_size);
		return packet;
	}	break;
	}

	return NULL;
}

// -------------------------------------------------------------------------------------------------

uint8_t* cn_protocol_client_read_connect_token_from_web_service(uint8_t* buffer, uint64_t application_id, uint64_t current_time, cn_protocol_connect_token_t* token)
{
	int ret = 0;

	// Read rest section.
	CN_CHECK(CN_STRNCMP((const char*)buffer, (const char*)CN_PROTOCOL_VERSION_STRING, CN_PROTOCOL_VERSION_STRING_LEN));
	buffer += CN_PROTOCOL_VERSION_STRING_LEN;
	CN_CHECK(cn_read_uint64(&buffer) != application_id);
	token->creation_timestamp = cn_read_uint64(&buffer);
	token->client_to_server_key = cn_read_key(&buffer);
	token->server_to_client_key = cn_read_key(&buffer);

	// Read public section.
	uint8_t* connect_token_packet = buffer;
	cn_protocol_packet_connect_token_t packet;
	if (cn_is_error(cn_protocol_read_connect_token_packet_public_section(buffer, application_id, current_time, &packet))) return NULL;
	token->expiration_timestamp = packet.expiration_timestamp;
	token->handshake_timeout = packet.handshake_timeout;
	token->endpoint_count = packet.endpoint_count;
	CN_MEMCPY(token->endpoints, packet.endpoints, sizeof(cn_endpoint_t) * token->endpoint_count);

	return ret ? NULL : connect_token_packet;
}

cn_error_t cn_protocol_server_decrypt_connect_token_packet(uint8_t* packet_buffer, const cn_crypto_sign_public_t* pk, const cn_crypto_sign_secret_t* sk, uint64_t application_id, uint64_t current_time, cn_protocol_connect_token_decrypted_t* token)
{
	// Read public section.
	cn_protocol_packet_connect_token_t packet;
	cn_error_t err;
	err = cn_protocol_read_connect_token_packet_public_section(packet_buffer, application_id, current_time, &packet);
	if (cn_is_error(err)) return err;
	if (packet.expiration_timestamp <= current_time) return cn_error_failure("Invalid timestamp.");
	token->expiration_timestamp = packet.expiration_timestamp;
	token->handshake_timeout = packet.handshake_timeout;
	token->endpoint_count = packet.endpoint_count;
	CN_MEMCPY(token->endpoints, packet.endpoints, sizeof(cn_endpoint_t) * token->endpoint_count);
	CN_MEMCPY(token->signature.bytes, packet_buffer + 1024 - CN_PROTOCOL_SIGNATURE_SIZE, CN_PROTOCOL_SIGNATURE_SIZE);

	// Verify signature.
	if (cn_is_error(cn_crypto_sign_verify(pk, &token->signature, packet_buffer, 1024 - CN_PROTOCOL_SIGNATURE_SIZE))) return cn_error_failure("Failed authentication.");

	// Decrypt the secret section.
	uint8_t* secret_section = packet_buffer + 568;

	if (cn_is_error(cn_crypto_decrypt((cn_crypto_key_t*)sk, secret_section, CN_PROTOCOL_CONNECT_TOKEN_SECRET_SECTION_SIZE, 0))) {
		return cn_error_failure("Failed decryption.");
	}

	// Read secret section.
	secret_section += CN_PROTOCOL_SIGNATURE_SIZE - CN_CRYPTO_HEADER_BYTES;
	token->client_id = cn_read_uint64(&secret_section);
	token->client_to_server_key = cn_read_key(&secret_section);
	token->server_to_client_key = cn_read_key(&secret_section);
	uint8_t* user_data = secret_section + CN_CRYPTO_HEADER_BYTES;
	CN_MEMCPY(token->user_data, user_data, CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE);

	return cn_error_success();
}

// -------------------------------------------------------------------------------------------------

// TODO - Rename this hash table so it's obviously different than cute_hashtable.h -- the difference
// being one uses an unpredictable hash primitive from libhydrogen while the other uses a simple and
// fast hash function.

static uint32_t s_is_prime(uint32_t x)
{
	if ((x == 2) | (x == 3)) return 1;
	if ((x % 2 == 0) | (x % 3 == 0)) return 0;

	uint32_t divisor = 6;
	while (divisor * divisor - 2 * divisor + 1 <= x)
	{
		if (x % (divisor - 1) == 0) return 0;
		if (x % (divisor + 1) == 0) return 0;
		divisor += 6;
	}

	return 1;
}

static uint32_t s_next_prime(uint32_t a)
{
	while (1)
	{
		if (s_is_prime(a)) return a;
		else ++a;
	}
}

void cn_hashtable_init(cn_hashtable_t* table, int key_size, int item_size, int capacity, void* mem_ctx)
{
	CN_ASSERT(capacity);
	CN_MEMSET(table, 0, sizeof(cn_hashtable_t));

	table->count = 0;
	table->slot_capacity = s_next_prime(capacity);
	table->key_size = key_size;
	table->item_size = item_size;
	int slots_size = (int)(table->slot_capacity * sizeof(*table->slots));
	table->slots = (cn_hashtable_slot_t*)CN_ALLOC((size_t)slots_size, mem_ctx);
	CN_MEMSET(table->slots, 0, (size_t) slots_size);

	hydro_hash_keygen(table->secret_key);

	table->item_capacity = s_next_prime(capacity + capacity / 2);
	table->items_key = CN_ALLOC(table->item_capacity * (table->key_size + sizeof(*table->items_slot_index) + table->item_size) + table->item_size + table->key_size, mem_ctx);
	table->items_slot_index = (int*)((uint8_t*)table->items_key + table->item_capacity * table->key_size);
	table->items_data = (void*)(table->items_slot_index + table->item_capacity);
	table->temp_key = (void*)(((uintptr_t)table->items_data) + table->item_size * table->item_capacity);
	table->temp_item = (void*)(((uintptr_t)table->temp_key) + table->key_size);
	table->mem_ctx = mem_ctx;
}

void cn_hashtable_cleanup(cn_hashtable_t* table)
{
	CN_FREE(table->slots, table->mem_ctx);
	CN_FREE(table->items_key, table->mem_ctx);
	CN_MEMSET(table, 0, sizeof(cn_hashtable_t));
}

static int s_keys_equal(const cn_hashtable_t* table, const void* a, const void* b)
{
	return !CN_MEMCMP(a, b, table->key_size);
}

static void* s_get_key(const cn_hashtable_t* table, int index)
{
	uint8_t* keys = (uint8_t*)table->items_key;
	return keys + index * table->key_size;
}

static void* s_get_item(const cn_hashtable_t* table, int index)
{
	uint8_t* items = (uint8_t*)table->items_data;
	return items + index * table->item_size;
}

static uint64_t s_calc_hash(const cn_hashtable_t* table, const void* key)
{
	uint8_t hash_bytes[CN_HASHTABLE_HASH_BYTES];
	if (hydro_hash_hash(hash_bytes, CN_HASHTABLE_HASH_BYTES, (const uint8_t*)key, table->key_size, CN_CRYPTO_CONTEXT, table->secret_key) != 0) {
		CN_ASSERT(0);
		return -1;
	}

	uint64_t hash = *(uint64_t*)hash_bytes;
	return hash;
}

static int cn_hashtable_internal_find_slot(const cn_hashtable_t* table, const void* key)
{
	uint64_t hash = s_calc_hash(table, key);
	int base_slot = (int)(hash % (uint64_t)table->slot_capacity);
	int base_count = table->slots[base_slot].base_count;
	int slot = base_slot;

	while (base_count > 0)
	{
		uint64_t slot_hash = table->slots[slot].key_hash;

		if (slot_hash) {
			int slot_base = (int)(slot_hash % (uint64_t)table->slot_capacity);
			if (slot_base == base_slot)
			{
				CN_ASSERT(base_count > 0);
				--base_count;
				const void* found_key = s_get_key(table, table->slots[slot].item_index);
				if (slot_hash == hash && s_keys_equal(table, found_key, key))
					return slot;
			}
		}
		slot = (slot + 1) % table->slot_capacity;
	}

	return -1;
}

void* cn_hashtable_insert(cn_hashtable_t* table, const void* key, const void* item)
{
	CN_ASSERT(cn_hashtable_internal_find_slot(table, key) < 0);
	uint64_t hash = s_calc_hash(table, key);

	CN_ASSERT(table->count < table->slot_capacity);

	int base_slot = (int)(hash % (uint64_t)table->slot_capacity);
	int base_count = table->slots[base_slot].base_count;
	int slot = base_slot;
	int first_free = slot;
	while (base_count)
	{
		uint64_t slot_hash = table->slots[slot].key_hash;
		if (slot_hash == 0 && table->slots[first_free].key_hash != 0) first_free = slot;
		int slot_base = (int)(slot_hash % (uint64_t)table->slot_capacity);
		if (slot_base == base_slot)
			--base_count;
		slot = (slot + 1) % table->slot_capacity;
	}

	slot = first_free;
	while (table->slots[slot].key_hash)
		slot = (slot + 1) % table->slot_capacity;

	CN_ASSERT(table->count < table->item_capacity);

	CN_ASSERT(!table->slots[slot].key_hash && (hash % (uint64_t)table->slot_capacity) == (uint64_t)base_slot);
	CN_ASSERT(hash);
	table->slots[slot].key_hash = hash;
	table->slots[slot].item_index = table->count;
	++table->slots[base_slot].base_count;

	void* item_dst = s_get_item(table, table->count);
	void* key_dst = s_get_key(table, table->count);
	CN_MEMCPY(item_dst, item, table->item_size);
	CN_MEMCPY(key_dst, key, table->key_size);
	table->items_slot_index[table->count] = slot;
	++table->count;

	return item_dst;
}

void cn_hashtable_remove(cn_hashtable_t* table, const void* key)
{
	int slot = cn_hashtable_internal_find_slot(table, key);
	CN_ASSERT(slot >= 0);

	uint64_t hash = table->slots[slot].key_hash;
	int base_slot = (int)(hash % (uint64_t)table->slot_capacity);
	CN_ASSERT(hash);
	--table->slots[base_slot].base_count;
	table->slots[slot].key_hash = 0;

	int index = table->slots[slot].item_index;
	int last_index = table->count - 1;
	if (index != last_index)
	{
		void* dst_key = s_get_key(table, index);
		void* src_key = s_get_key(table, last_index);
		CN_MEMCPY(dst_key, src_key, (size_t)table->key_size);
		void* dst_item = s_get_item(table, index);
		void* src_item = s_get_item(table, last_index);
		CN_MEMCPY(dst_item, src_item, (size_t)table->item_size);
		table->items_slot_index[index] = table->items_slot_index[last_index];
		table->slots[table->items_slot_index[last_index]].item_index = index;
	}
	--table->count;
}

void cn_hashtable_clear(cn_hashtable_t* table)
{
	table->count = 0;
	CN_MEMSET(table->slots, 0, sizeof(*table->slots) * table->slot_capacity);
}

void* cn_hashtable_find(const cn_hashtable_t* table, const void* key)
{
	int slot = cn_hashtable_internal_find_slot(table, key);
	if (slot < 0) return 0;

	int index = table->slots[slot].item_index;
	return s_get_item(table, index);
}

int cn_hashtable_count(const cn_hashtable_t* table)
{
	return table->count;
}

void* cn_hashtable_items(const cn_hashtable_t* table)
{
	return table->items_data;
}

void* cn_hashtable_keys(const cn_hashtable_t* table)
{
	return table->items_key;
}

void cn_hashtable_swap(cn_hashtable_t* table, int index_a, int index_b)
{
	if (index_a < 0 || index_a >= table->count || index_b < 0 || index_b >= table->count) return;

	int slot_a = table->items_slot_index[index_a];
	int slot_b = table->items_slot_index[index_b];

	table->items_slot_index[index_a] = slot_b;
	table->items_slot_index[index_b] = slot_a;

	void* key_a = s_get_key(table, index_a);
	void* key_b = s_get_key(table, index_b);
	CN_MEMCPY(table->temp_key, key_a, table->key_size);
	CN_MEMCPY(key_a, key_b, table->key_size);
	CN_MEMCPY(key_b, table->temp_key, table->key_size);

	void* item_a = s_get_item(table, index_a);
	void* item_b = s_get_item(table, index_b);
	CN_MEMCPY(table->temp_item, item_a, table->item_size);
	CN_MEMCPY(item_a, item_b, table->item_size);
	CN_MEMCPY(item_b, table->temp_item, table->item_size);

	table->slots[slot_a].item_index = index_b;
	table->slots[slot_b].item_index = index_a;
}

// -------------------------------------------------------------------------------------------------

void cn_protocol_connect_token_cache_init(cn_protocol_connect_token_cache_t* cache, int capacity, void* mem_ctx)
{
	cache->capacity = capacity;
	cn_hashtable_init(&cache->table, CN_PROTOCOL_SIGNATURE_SIZE, sizeof(cn_protocol_connect_token_cache_entry_t), capacity, mem_ctx);
	cn_list_init(&cache->list);
	cn_list_init(&cache->free_list);
	cache->node_memory = (cn_protocol_connect_token_cache_node_t*)CN_ALLOC(sizeof(cn_protocol_connect_token_cache_node_t) * capacity, mem_ctx);

	for (int i = 0; i < capacity; ++i)
	{
		cn_list_node_t* node = &cache->node_memory[i].node;
		cn_list_init_node(node);
		cn_list_push_front(&cache->free_list, node);
	}

	cache->mem_ctx = mem_ctx;
}

void cn_protocol_connect_token_cache_cleanup(cn_protocol_connect_token_cache_t* cache)
{
	cn_hashtable_cleanup(&cache->table);
	CN_FREE(cache->node_memory, cache->mem_ctx);
	cache->node_memory = NULL;
}

cn_protocol_connect_token_cache_entry_t* cn_protocol_connect_token_cache_find(cn_protocol_connect_token_cache_t* cache, const uint8_t* hmac_bytes)
{
	void* entry_ptr = cn_hashtable_find(&cache->table, hmac_bytes);
	if (entry_ptr) {
		cn_protocol_connect_token_cache_entry_t* entry = (cn_protocol_connect_token_cache_entry_t*)entry_ptr;
		cn_list_node_t* node = entry->node;
		cn_list_remove(node);
		cn_list_push_front(&cache->list, node);
		return entry;
	} else {
		return NULL;
	}
}

void cn_protocol_connect_token_cache_add(cn_protocol_connect_token_cache_t* cache, const uint8_t* hmac_bytes)
{
	cn_protocol_connect_token_cache_entry_t entry;

	int table_count = cn_hashtable_count(&cache->table);
	CN_ASSERT(table_count <= cache->capacity);
	if (table_count == cache->capacity) {
		cn_list_node_t* oldest_node = cn_list_pop_back(&cache->list);
		cn_protocol_connect_token_cache_node_t* oldest_entry_node = CN_LIST_HOST(cn_protocol_connect_token_cache_node_t, node, oldest_node);
		cn_hashtable_remove(&cache->table, oldest_entry_node->signature.bytes);
		CN_MEMCPY(oldest_entry_node->signature.bytes, hmac_bytes, CN_PROTOCOL_SIGNATURE_SIZE);

		cn_protocol_connect_token_cache_entry_t* entry_ptr = (cn_protocol_connect_token_cache_entry_t*)cn_hashtable_insert(&cache->table, hmac_bytes, &entry);
		CN_ASSERT(entry_ptr);
		entry_ptr->node = oldest_node;
		cn_list_push_front(&cache->list, entry_ptr->node);
	} else {
		cn_protocol_connect_token_cache_entry_t* entry_ptr = (cn_protocol_connect_token_cache_entry_t*)cn_hashtable_insert(&cache->table, hmac_bytes, &entry);
		CN_ASSERT(entry_ptr);
		entry_ptr->node = cn_list_pop_front(&cache->free_list);
		cn_protocol_connect_token_cache_node_t* entry_node = CN_LIST_HOST(cn_protocol_connect_token_cache_node_t, node, entry_ptr->node);
		CN_MEMCPY(entry_node->signature.bytes, hmac_bytes, CN_PROTOCOL_SIGNATURE_SIZE);
		cn_list_push_front(&cache->list, entry_ptr->node);
	}
}

// -------------------------------------------------------------------------------------------------

void cn_protocol_encryption_map_init(cn_protocol_encryption_map_t* map, void* mem_ctx)
{
	cn_hashtable_init(&map->table, sizeof(cn_endpoint_t), sizeof(cn_protocol_encryption_state_t), CN_PROTOCOL_ENCRYPTION_STATES_MAX, mem_ctx);
}

void cn_protocol_encryption_map_cleanup(cn_protocol_encryption_map_t* map)
{
	cn_hashtable_cleanup(&map->table);
}

void cn_protocol_encryption_map_clear(cn_protocol_encryption_map_t* map)
{
	cn_hashtable_clear(&map->table);
}

int cn_protocol_encryption_map_count(cn_protocol_encryption_map_t* map)
{
	return cn_hashtable_count(&map->table);
}

void cn_protocol_encryption_map_insert(cn_protocol_encryption_map_t* map, cn_endpoint_t endpoint, const cn_protocol_encryption_state_t* state)
{
	cn_hashtable_insert(&map->table, &endpoint, state);
}

cn_protocol_encryption_state_t* cn_protocol_encryption_map_find(cn_protocol_encryption_map_t* map, cn_endpoint_t endpoint)
{
	void* ptr = cn_hashtable_find(&map->table, &endpoint);
	if (ptr) {
		cn_protocol_encryption_state_t* state = (cn_protocol_encryption_state_t*)ptr;
		state->last_packet_recieved_time = 0;
		return state;
	} else {
		return NULL;
	}
}

void cn_protocol_encryption_map_remove(cn_protocol_encryption_map_t* map, cn_endpoint_t endpoint)
{
	cn_hashtable_remove(&map->table, &endpoint);
}

cn_endpoint_t* cn_protocol_encryption_map_get_endpoints(cn_protocol_encryption_map_t* map)
{
	return (cn_endpoint_t*)cn_hashtable_keys(&map->table);
}

cn_protocol_encryption_state_t* cn_protocol_encryption_map_get_states(cn_protocol_encryption_map_t* map)
{
	return (cn_protocol_encryption_state_t*)cn_hashtable_items(&map->table);
}

void cn_protocol_encryption_map_look_for_timeouts_or_expirations(cn_protocol_encryption_map_t* map, double dt, uint64_t time)
{
	int index = 0;
	int count = cn_protocol_encryption_map_count(map);
	cn_endpoint_t* endpoints = cn_protocol_encryption_map_get_endpoints(map);
	cn_protocol_encryption_state_t* states = cn_protocol_encryption_map_get_states(map);

	while (index < count)
	{
		cn_protocol_encryption_state_t* state = states + index;
		state->last_packet_recieved_time += dt;
		int timed_out = state->last_packet_recieved_time >= state->handshake_timeout;
		int expired = state->expiration_timestamp <= time;
		if (timed_out | expired) {
			cn_protocol_encryption_map_remove(map, endpoints[index]);
			--count;
		} else {
			++index;
		}
	}
}

// -------------------------------------------------------------------------------------------------

static CN_INLINE const char* s_protocol_client_state_str(cn_protocol_client_state_t state)
{
	switch (state)
	{
	case CN_PROTOCOL_CLIENT_STATE_CONNECT_TOKEN_EXPIRED: return "CONNECT_TOKEN_EXPIRED";
	case CN_PROTOCOL_CLIENT_STATE_INVALID_CONNECT_TOKEN: return "INVALID_CONNECT_TOKEN";
	case CN_PROTOCOL_CLIENT_STATE_CONNECTION_TIMED_OUT: return "CONNECTION_TIMED_OUT";
	case CN_PROTOCOL_CLIENT_STATE_CHALLENGED_RESPONSE_TIMED_OUT: return "CHALLENGED_RESPONSE_TIMED_OUT";
	case CN_PROTOCOL_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT: return "CONNECTION_REQUEST_TIMED_OUT";
	case CN_PROTOCOL_CLIENT_STATE_CONNECTION_DENIED: return "CONNECTION_DENIED";
	case CN_PROTOCOL_CLIENT_STATE_DISCONNECTED: return "DISCONNECTED";
	case CN_PROTOCOL_CLIENT_STATE_SENDING_CONNECTION_REQUEST: return "SENDING_CONNECTION_REQUEST";
	case CN_PROTOCOL_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE: return "SENDING_CHALLENGE_RESPONSE";
	case CN_PROTOCOL_CLIENT_STATE_CONNECTED: return "CONNECTED";
	}

	return NULL;
}

static void s_protocol_client_set_state(cn_protocol_client_t* client, cn_protocol_client_state_t state)
{
	client->state = state;
	//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Client: Switching to state %s.", s_protocol_client_state_str(state));
}

cn_protocol_client_t* cn_protocol_client_create(uint16_t port, uint64_t application_id, bool use_ipv6, void* user_allocator_context)
{
	cn_protocol_client_t* client = (cn_protocol_client_t*)CN_ALLOC(sizeof(cn_protocol_client_t), user_allocator_context);
	CN_MEMSET(client, 0, sizeof(cn_protocol_client_t));
	s_protocol_client_set_state(client, CN_PROTOCOL_CLIENT_STATE_DISCONNECTED);
	client->use_ipv6 = use_ipv6;
	client->port = port;
	client->application_id = application_id;
	client->mem_ctx = user_allocator_context;
	client->packet_queue = cn_circular_buffer_create(CN_MB, client->mem_ctx);
	return client;
}

void cn_protocol_client_destroy(cn_protocol_client_t* client)
{
	// TODO: Detect if disconnect was not called yet.
	cn_simulator_destroy(client->sim);
	cn_circular_buffer_free(&client->packet_queue);
	CN_FREE(client, client->mem_ctx);
}

typedef struct cn_protocol_payload_t
{
	uint64_t sequence;
	int size;
	void* data;
} cn_protocol_payload_t;

cn_error_t cn_protocol_client_connect(cn_protocol_client_t* client, const uint8_t* connect_token)
{
	uint8_t* connect_token_packet = cn_protocol_client_read_connect_token_from_web_service(
		(uint8_t*)connect_token,
		client->application_id,
		client->current_time,
		&client->connect_token
	);
	if (!connect_token_packet) {
		s_protocol_client_set_state(client, CN_PROTOCOL_CLIENT_STATE_INVALID_CONNECT_TOKEN);
		return cn_error_failure("Invalid connect token.");
	}

	CN_MEMCPY(client->connect_token_packet, connect_token_packet, CN_PROTOCOL_CONNECT_TOKEN_PACKET_SIZE);

#ifndef CUTE_NET_NO_IPV6
	cn_address_type_t sock_addr_type = client->use_ipv6 ? CN_ADDRESS_TYPE_IPV6 : CN_ADDRESS_TYPE_IPV4;
#else
	if (client->use_ipv6) {
		return cn_error_failure("Unable to open socket. Cute net compiled without IPV6 support but client configured to use ipv6");
	}
	cn_address_type_t sock_addr_type = CN_ADDRESS_TYPE_IPV4;
#endif
	if (cn_socket_init1(&client->socket, sock_addr_type, client->port, CN_PROTOCOL_CLIENT_SEND_BUFFER_SIZE, CN_PROTOCOL_CLIENT_RECEIVE_BUFFER_SIZE)) {
		return cn_error_failure("Unable to open socket.");
	}

	cn_protocol_replay_buffer_init(&client->replay_buffer);
	client->server_endpoint_index = 0;
	client->last_packet_sent_time = CN_PROTOCOL_SEND_RATE;
	s_protocol_client_set_state(client, CN_PROTOCOL_CLIENT_STATE_SENDING_CONNECTION_REQUEST);
	client->goto_next_server_tentative_state = CN_PROTOCOL_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT;

	return cn_error_success();
}

static CN_INLINE cn_endpoint_t s_protocol_server_endpoint(cn_protocol_client_t* client)
{
	return client->connect_token.endpoints[client->server_endpoint_index];
}

static CN_INLINE const char* s_protocol_packet_str(uint8_t type)
{
	switch (type)
	{
	case CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN: return "CONNECT_TOKEN";
	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED: return "CONNECTION_ACCEPTED";
	case CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED: return "CONNECTION_DENIED";
	case CN_PROTOCOL_PACKET_TYPE_KEEPALIVE: return "KEEPALIVE";
	case CN_PROTOCOL_PACKET_TYPE_DISCONNECT: return "DISCONNECT";
	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST: return "CHALLENGE_REQUEST";
	case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE: return "CHALLENGE_RESPONSE";
	case CN_PROTOCOL_PACKET_TYPE_PAYLOAD: return "PAYLOAD";
	}

	return NULL;
}

static void s_protocol_client_send(cn_protocol_client_t* client, void* packet)
{
	int sz = cn_protocol_packet_write(packet, client->buffer, client->sequence++, &client->connect_token.client_to_server_key);

	if (sz >= 73) {
		cn_socket_send(&client->socket, client->sim, s_protocol_server_endpoint(client), client->buffer, sz);
		client->last_packet_sent_time = 0;
		//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Client: Sent %s packet to server.", s_protocol_packet_str(*(uint8_t*)packet));
	}
}

bool cn_protocol_client_get_packet(cn_protocol_client_t* client, void** data, int* size, uint64_t* sequence)
{
	cn_protocol_payload_t payload;
	if (cn_circular_buffer_pull(&client->packet_queue, &payload, sizeof(cn_protocol_payload_t)) < 0) {
		return false;
	}

	if (sequence) *sequence = payload.sequence;
	if (size) *size = payload.size;
	*data = payload.data;

	return true;
}

void cn_protocol_client_free_packet(cn_protocol_client_t* client, void* packet)
{
	cn_protocol_packet_payload_t* payload_packet = (cn_protocol_packet_payload_t*)((uint8_t*)packet - CN_OFFSET_OF(cn_protocol_packet_payload_t, payload));
	cn_protocol_packet_allocator_free(NULL, (cn_protocol_packet_type_t)payload_packet->packet_type, payload_packet);
}

static void s_protocol_disconnect(cn_protocol_client_t* client, cn_protocol_client_state_t state, int send_packets)
{
	void* packet = NULL;
	while (cn_protocol_client_get_packet(client, &packet, NULL, NULL)) {
		cn_protocol_client_free_packet(client, packet);
	}

	if (send_packets) {
		cn_protocol_packet_disconnect_t disconnect_packet;
		disconnect_packet.packet_type = CN_PROTOCOL_PACKET_TYPE_DISCONNECT;
		for (int i = 0; i < CN_PROTOCOL_REDUNDANT_DISCONNECT_PACKET_COUNT; ++i)
		{
			s_protocol_client_send(client, &disconnect_packet);
		}
	}

	cn_socket_cleanup(&client->socket);
	cn_circular_buffer_reset(&client->packet_queue);

	s_protocol_client_set_state(client, state);
}

void cn_protocol_client_disconnect(cn_protocol_client_t* client)
{
	if (client->state <= 0) return;
	s_protocol_disconnect(client, CN_PROTOCOL_CLIENT_STATE_DISCONNECTED, 1);
}

static void s_protocol_receive_packets(cn_protocol_client_t* client)
{
	uint8_t* buffer = client->buffer;

	while (1)
	{
		// Read packet from UDP stack, and open it.
		cn_endpoint_t from;
		int sz = cn_socket_receive(&client->socket, &from, buffer, CN_PROTOCOL_PACKET_SIZE_MAX);
		if (!sz) break;

		if (!cn_endpoint_equals(s_protocol_server_endpoint(client), from)) {
			continue;
		}

		if (sz < 73) {
			continue;
		}

		uint8_t type = *buffer;
		if (type > 7) {
			continue;
		}

		switch (type)
		{
		case CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN: // fall-thru
		case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE:
			continue;
		}

		uint64_t sequence = ~0u;
		void* packet_ptr = cn_protocol_packet_open(buffer, sz, &client->connect_token.server_to_client_key, NULL, &client->replay_buffer, &sequence);
		if (!packet_ptr) continue;

		// Handle packet based on client's current state.
		int free_packet = 1;
		int should_break = 0;

		switch (client->state)
		{
		case CN_PROTOCOL_CLIENT_STATE_SENDING_CONNECTION_REQUEST:
			if (type == CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST) {
				cn_protocol_packet_challenge_t* packet = (cn_protocol_packet_challenge_t*)packet_ptr;
				client->challenge_nonce = packet->challenge_nonce;
				CN_MEMCPY(client->challenge_data, packet->challenge_data, CN_PROTOCOL_CHALLENGE_DATA_SIZE);
				s_protocol_client_set_state(client, CN_PROTOCOL_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE);
				client->goto_next_server_tentative_state = CN_PROTOCOL_CLIENT_STATE_CHALLENGED_RESPONSE_TIMED_OUT;
				client->last_packet_sent_time = CN_PROTOCOL_SEND_RATE;
				client->last_packet_recieved_time = 0;
			} else if (type == CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED) {
				client->goto_next_server = 1;
				client->goto_next_server_tentative_state = CN_PROTOCOL_CLIENT_STATE_CONNECTION_DENIED;
				should_break = 1;
				//log(CN_LOG_LEVEL_WARNING, "Protocol Client: Received CONNECTION_DENIED packet, attempting to connect to next server.");
			}
			break;

		case CN_PROTOCOL_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE:
			if (type == CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED) {
				cn_protocol_packet_connection_accepted_t* packet = (cn_protocol_packet_connection_accepted_t*)packet_ptr;
				client->client_id = packet->client_id;
				client->max_clients = packet->max_clients;
				client->connection_timeout = (double)packet->connection_timeout;
				s_protocol_client_set_state(client, CN_PROTOCOL_CLIENT_STATE_CONNECTED);
				client->last_packet_recieved_time = 0;
			} else if (type == CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED) {
				client->goto_next_server = 1;
				client->goto_next_server_tentative_state = CN_PROTOCOL_CLIENT_STATE_CONNECTION_DENIED;
				should_break = 1;
				//log(CN_LOG_LEVEL_WARNING, "Protocol Client: Received CONNECTION_DENIED packet, attempting to connect to next server.");
			}
			break;

		case CN_PROTOCOL_CLIENT_STATE_CONNECTED:
			if (type == CN_PROTOCOL_PACKET_TYPE_PAYLOAD) {
				client->last_packet_recieved_time = 0;
				cn_protocol_packet_payload_t* packet = (cn_protocol_packet_payload_t*)packet_ptr;
				cn_protocol_payload_t payload;
				payload.sequence = sequence;
				payload.size = packet->payload_size;
				payload.data = packet->payload;
				if (cn_circular_buffer_push(&client->packet_queue, &payload, sizeof(cn_protocol_payload_t)) < 0) {
					//log(CN_LOG_LEVEL_WARNING, "Protocol Client: Packet queue is full; dropped payload packet.");
					free_packet = 1;
				} else {
					free_packet = 0;
				}
			} else if (type == CN_PROTOCOL_PACKET_TYPE_KEEPALIVE) {
				client->last_packet_recieved_time = 0;
			} else if (type == CN_PROTOCOL_PACKET_TYPE_DISCONNECT) {
				//log(CN_LOG_LEVEL_WARNING, "Protocol Client: Received DISCONNECT packet from server.");
				if (free_packet) {
					cn_protocol_packet_allocator_free(NULL, (cn_protocol_packet_type_t)type, packet_ptr);
					free_packet = 0;
					packet_ptr = NULL;
				}
				s_protocol_disconnect(client, CN_PROTOCOL_CLIENT_STATE_DISCONNECTED, 0);
				should_break = 1;
			}
			break;

		default:
			break;
		}

		if (free_packet) {
			cn_protocol_packet_allocator_free(NULL, (cn_protocol_packet_type_t)type, packet_ptr);
		}

		if (should_break) {
			break;
		}
	}
}

static void s_protocol_send_packets(cn_protocol_client_t* client)
{
	switch (client->state)
	{
	case CN_PROTOCOL_CLIENT_STATE_SENDING_CONNECTION_REQUEST:
		if (client->last_packet_sent_time >= CN_PROTOCOL_SEND_RATE) {
			s_protocol_client_send(client, client->connect_token_packet);
		}
		break;

	case CN_PROTOCOL_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE:
		if (client->last_packet_sent_time >= CN_PROTOCOL_SEND_RATE) {
			cn_protocol_packet_challenge_t packet;
			packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE;
			packet.challenge_nonce = client->challenge_nonce;
			CN_MEMCPY(packet.challenge_data, client->challenge_data, CN_PROTOCOL_CHALLENGE_DATA_SIZE);
			s_protocol_client_send(client, &packet);
		}
		break;

	case CN_PROTOCOL_CLIENT_STATE_CONNECTED:
		if (client->last_packet_sent_time >= CN_PROTOCOL_SEND_RATE) {
			cn_protocol_packet_keepalive_t packet;
			packet.packet_type = CN_PROTOCOL_PACKET_TYPE_KEEPALIVE;
			s_protocol_client_send(client, &packet);
		}
		break;

	default:
		break;
	}
}

static int s_protocol_goto_next_server(cn_protocol_client_t* client)
{
	if (client->server_endpoint_index + 1 == client->connect_token.endpoint_count) {
		s_protocol_disconnect(client, client->goto_next_server_tentative_state, 0);
		//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Client: Unable to connect to any server in the server list.");
		return 0;
	}

	int index = ++client->server_endpoint_index;

	//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Client: Unable to connect to server index %d; now attempting index %d.", index - 1, index);

	client->last_packet_recieved_time = 0;
	client->last_packet_sent_time = CN_PROTOCOL_SEND_RATE;
	client->goto_next_server = 0;
	cn_circular_buffer_reset(&client->packet_queue);

	client->server_endpoint_index = index;
	s_protocol_client_set_state(client, CN_PROTOCOL_CLIENT_STATE_SENDING_CONNECTION_REQUEST);

	return 1;
}

void cn_protocol_client_update(cn_protocol_client_t* client, double dt, uint64_t current_time)
{
	if (client->state <= 0) {
		return;
	}

	client->current_time = current_time;
	client->last_packet_recieved_time += dt;
	client->last_packet_sent_time += dt;

	cn_simulator_update(client->sim, dt);
	s_protocol_receive_packets(client);
	s_protocol_send_packets(client);

	if (client->state <= 0) {
		return;
	}

	int timeout = client->last_packet_recieved_time >= client->connect_token.handshake_timeout;
	int is_handshake = client->state >= CN_PROTOCOL_CLIENT_STATE_SENDING_CONNECTION_REQUEST && client->state <= CN_PROTOCOL_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE;
	if (is_handshake) {
		int expired = client->connect_token.expiration_timestamp <= client->current_time;
		if (expired) {
			s_protocol_disconnect(client, CN_PROTOCOL_CLIENT_STATE_CONNECT_TOKEN_EXPIRED, 1);
		} else if (timeout | client->goto_next_server) {
			if (s_protocol_goto_next_server(client)) {
				return;
			}
			else if (client->state == CN_PROTOCOL_CLIENT_STATE_SENDING_CONNECTION_REQUEST) {
				s_protocol_disconnect(client, CN_PROTOCOL_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT, 1);
			} else if (client->state == CN_PROTOCOL_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE) {
				s_protocol_disconnect(client, CN_PROTOCOL_CLIENT_STATE_CHALLENGED_RESPONSE_TIMED_OUT, 1);
			}
		}
	} else { // CN_PROTOCOL_CLIENT_STATE_CONNECTED
		CN_ASSERT(client->state == CN_PROTOCOL_CLIENT_STATE_CONNECTED);
		timeout = client->last_packet_recieved_time >= client->connection_timeout;
		if (timeout) {
			s_protocol_disconnect(client, CN_PROTOCOL_CLIENT_STATE_CONNECTION_TIMED_OUT, 1);
		}
	}
}

cn_error_t cn_protocol_client_send(cn_protocol_client_t* client, const void* data, int size)
{
	if (size < 0) return cn_error_failure("`size` can not be negative.");
	if (size > CN_PROTOCOL_PACKET_PAYLOAD_MAX) return cn_error_failure("`size` exceeded `CN_PROTOCOL_PACKET_PAYLOAD_MAX`.");
	cn_protocol_packet_payload_t packet;
	packet.packet_type = CN_PROTOCOL_PACKET_TYPE_PAYLOAD;
	packet.payload_size = size;
	CN_MEMCPY(packet.payload, data, size);
	s_protocol_client_send(client, &packet);
	return cn_error_success();
}

cn_protocol_client_state_t cn_protocol_client_get_state(cn_protocol_client_t* client)
{
	return client->state;
}

uint64_t cn_protocol_client_get_id(cn_protocol_client_t* client)
{
	return client->client_id;
}

uint32_t cn_protocol_client_get_max_clients(cn_protocol_client_t* client)
{
	return client->max_clients;
}

cn_endpoint_t cn_protocol_client_get_server_address(cn_protocol_client_t* client)
{
	return s_protocol_server_endpoint(client);
}

uint16_t cn_protocol_client_get_port(cn_protocol_client_t* client)
{
	return client->socket.endpoint.port;
}

void cn_protocol_client_enable_network_simulator(cn_protocol_client_t* client, double latency, double jitter, double drop_chance, double duplicate_chance)
{
	cn_simulator_t* sim = cn_simulator_create(&client->socket, client->mem_ctx);
	sim->latency = latency;
	sim->jitter = jitter;
	sim->drop_chance = drop_chance;
	sim->duplicate_chance = duplicate_chance;
	client->sim = sim;
}

// -------------------------------------------------------------------------------------------------

#define CN_CHECK_BUFFER_GROW(ctx, count, capacity, data, type) \
	do { \
		if (ctx->count == ctx->capacity) \
		{ \
			int new_capacity = ctx->capacity * 2; \
			CN_ASSERT(new_capacity); \
			void* new_data = CN_ALLOC(sizeof(type) * new_capacity, ctx->mem_ctx); \
			if (!new_data) CN_ASSERT(0); \
			CN_MEMCPY(new_data, ctx->data, sizeof(type) * ctx->count); \
			CN_FREE(ctx->data, ctx->mem_ctx); \
			ctx->data = (type*)new_data; \
			ctx->capacity = new_capacity; \
		} \
	} while (0)

// -------------------------------------------------------------------------------------------------

typedef uint64_t cn_handle_t;
#define CN_INVALID_HANDLE (~0ULL)

typedef union cn_handle_entry_t
{
	struct
	{
		uint64_t user_index : 32;
		uint64_t generation : 32;
	} data;
	uint64_t val;
} cn_handle_entry_t;

typedef struct cn_handle_allocator_t
{
	uint32_t freelist;
	int handles_capacity;
	int handles_count;
	cn_handle_entry_t* handles;
	void* mem_ctx;
} cn_handle_allocator_t;

static void s_add_elements_to_freelist(cn_handle_allocator_t* table, int first_index, int last_index)
{
	cn_handle_entry_t* handles = table->handles;
	for (int i = first_index; i < last_index; ++i)
	{
		cn_handle_entry_t handle;
		handle.data.user_index = i + 1;
		handle.data.generation = 0;
		handles[i] = handle;
	}

	cn_handle_entry_t last_handle;
	last_handle.data.user_index = UINT32_MAX;
	last_handle.data.generation = 0;
	handles[last_index] = last_handle;

	table->freelist = first_index;
}

cn_handle_allocator_t* cn_handle_allocator_create(int initial_capacity, void* user_allocator_context)
{
	cn_handle_allocator_t* table = (cn_handle_allocator_t*)CN_ALLOC(sizeof(cn_handle_allocator_t), user_allocator_context);
	if (!table) return NULL;
	table->freelist = ~0u;
	table->handles_capacity = initial_capacity;
	table->handles_count = 0;
	table->handles = NULL;
	table->mem_ctx = user_allocator_context;

	if (initial_capacity) {
		table->handles = (cn_handle_entry_t*)CN_ALLOC(sizeof(cn_handle_entry_t) * initial_capacity, user_allocator_context);
		if (!table->handles) {
			CN_FREE(table, user_allocator_context);
			return NULL;
		}
		int last_index = table->handles_capacity - 1;
		s_add_elements_to_freelist(table, 0, last_index);
	}

	return table;
}

void cn_handle_allocator_destroy(cn_handle_allocator_t* table)
{
	if (!table) return;
	void* mem_ctx = table->mem_ctx;
	CN_FREE(table->handles, mem_ctx);
	CN_FREE(table, mem_ctx);
}

cn_handle_t cn_handle_allocator_alloc(cn_handle_allocator_t* table, uint32_t index)
{
	uint32_t freelist_index = table->freelist;
	if (freelist_index == UINT32_MAX) {
		int first_index = table->handles_capacity;
		if (!first_index) first_index = 1;
		CN_CHECK_BUFFER_GROW(table, handles_count, handles_capacity, handles, cn_handle_entry_t);
		int last_index = table->handles_capacity - 1;
		s_add_elements_to_freelist(table, first_index, last_index);
		freelist_index = table->freelist;
	}

	// Pop freelist.
	cn_handle_entry_t* handles = table->handles;
	table->freelist = (uint32_t)handles[freelist_index].data.user_index;
	table->handles_count++;

	// Setup handle indices.
	handles[freelist_index].data.user_index = index;
	cn_handle_t handle = (((uint64_t)freelist_index) << 32) | handles[freelist_index].data.generation;
	return handle;
}

static CN_INLINE uint32_t s_table_index(cn_handle_t handle)
{
	return (uint32_t)((handle & 0xFFFFFFFF00000000ULL) >> 32);
}

uint32_t cn_handle_allocator_get_index(cn_handle_allocator_t* table, cn_handle_t handle)
{
	cn_handle_entry_t* handles = table->handles;
	uint32_t table_index = s_table_index(handle);
	uint64_t generation = handle & 0xFFFFFFFF;
	CN_ASSERT(handles[table_index].data.generation == generation);
	return (uint32_t)handles[table_index].data.user_index;
}

void cn_handle_allocator_update_index(cn_handle_allocator_t* table, cn_handle_t handle, uint32_t index)
{
	cn_handle_entry_t* handles = table->handles;
	uint32_t table_index = s_table_index(handle);
	uint64_t generation = handle & 0xFFFFFFFF;
	CN_ASSERT(handles[table_index].data.generation == generation);
	handles[table_index].data.user_index = index;
}

void cn_handle_allocator_free(cn_handle_allocator_t* table, cn_handle_t handle)
{
	// Push handle onto freelist.
	cn_handle_entry_t* handles = table->handles;
	uint32_t table_index = s_table_index(handle);
	handles[table_index].data.user_index = table->freelist;
	handles[table_index].data.generation++;
	table->freelist = table_index;
	table->handles_count--;
}

int cn_handle_allocator_is_handle_valid(cn_handle_allocator_t* table, cn_handle_t handle)
{
	cn_handle_entry_t* handles = table->handles;
	uint32_t table_index = s_table_index(handle);
	uint64_t generation = handle & 0xFFFFFFFF;
	return handles[table_index].data.generation == generation;
}

// -------------------------------------------------------------------------------------------------

cn_protocol_server_t* cn_protocol_server_create(uint64_t application_id, const cn_crypto_sign_public_t* public_key, const cn_crypto_sign_secret_t* secret_key, void* mem_ctx)
{
	cn_protocol_server_t* server = (cn_protocol_server_t*)CN_ALLOC(sizeof(cn_protocol_server_t), mem_ctx);
	CN_MEMSET(server, 0, sizeof(cn_protocol_server_t));

	server->running = 0;
	server->application_id = application_id;
	server->packet_allocator = cn_protocol_packet_allocator_create(mem_ctx);
	server->event_queue = cn_circular_buffer_create(CN_MB * 10, mem_ctx);
	server->public_key = *public_key;
	server->secret_key = *secret_key;
	server->mem_ctx = mem_ctx;

	return server;
}

void cn_protocol_server_destroy(cn_protocol_server_t* server)
{
	cn_simulator_destroy(server->sim);
	cn_protocol_packet_allocator_destroy(server->packet_allocator);
	cn_circular_buffer_free(&server->event_queue);
	CN_FREE(server, server->mem_ctx);
}

cn_error_t cn_protocol_server_start(cn_protocol_server_t* server, const char* address, uint32_t connection_timeout)
{
	int cleanup_map = 0;
	int cleanup_cache = 0;
	int cleanup_socket = 0;
	int cleanup_endpoint_table = 0;
	int cleanup_client_id_table = 0;
	int ret = 0;
	cn_protocol_encryption_map_init(&server->encryption_map, server->mem_ctx);
	cleanup_map = 1;
	cn_protocol_connect_token_cache_init(&server->token_cache, CN_PROTOCOL_CONNECT_TOKEN_ENTRIES_MAX, server->mem_ctx);
	cleanup_cache = 1;
	if (cn_socket_init2(&server->socket, address, CN_PROTOCOL_SERVER_SEND_BUFFER_SIZE, CN_PROTOCOL_SERVER_RECEIVE_BUFFER_SIZE)) ret = -1;
	cleanup_socket = 1;
	cn_hashtable_init(&server->client_endpoint_table, sizeof(cn_endpoint_t), sizeof(uint64_t), CN_PROTOCOL_SERVER_MAX_CLIENTS, server->mem_ctx);
	cleanup_endpoint_table = 1;
	cn_hashtable_init(&server->client_id_table, sizeof(uint64_t), sizeof(int), CN_PROTOCOL_SERVER_MAX_CLIENTS, server->mem_ctx);
	cleanup_client_id_table = 1;

	server->running = true;
	server->challenge_nonce = 0;
	server->client_count = 0;
	server->connection_timeout = connection_timeout;

	if (ret) {
		if (cleanup_map) cn_protocol_encryption_map_cleanup(&server->encryption_map);
		if (cleanup_cache) cn_protocol_connect_token_cache_cleanup(&server->token_cache);
		if (cleanup_socket) cn_socket_cleanup(&server->socket);
		if (cleanup_endpoint_table) cn_hashtable_cleanup(&server->client_endpoint_table);
		if (cleanup_client_id_table) cn_hashtable_cleanup(&server->client_id_table);
		return cn_error_failure(NULL); // -- Change this when socket_init is changed to use error_t.
	}

	return cn_error_success();
}

static CN_INLINE int s_protocol_server_event_pull(cn_protocol_server_t* server, cn_protocol_server_event_t* event)
{
	return cn_circular_buffer_pull(&server->event_queue, event, sizeof(cn_protocol_server_event_t));
}

static CN_INLINE int s_protocol_server_event_push(cn_protocol_server_t* server, cn_protocol_server_event_t* event)
{
	if (cn_circular_buffer_push(&server->event_queue, event, sizeof(cn_protocol_server_event_t)) < 0) {
		if (cn_circular_buffer_grow(&server->event_queue, server->event_queue.capacity * 2) < 0) {
			return -1;
		}
		return cn_circular_buffer_push(&server->event_queue, event, sizeof(cn_protocol_server_event_t));
	} else {
		return 0;
	}
}

static void s_protocol_server_disconnect_sequence(cn_protocol_server_t* server, uint32_t index)
{
	for (int i = 0; i < CN_PROTOCOL_DISCONNECT_REDUNDANT_PACKET_COUNT; ++i)
	{
		cn_protocol_packet_disconnect_t packet;
		packet.packet_type = CN_PROTOCOL_PACKET_TYPE_DISCONNECT;
		if (cn_protocol_packet_write(&packet, server->buffer, server->client_sequence[index]++, server->client_server_to_client_key + index) == 73) {
			cn_socket_send(&server->socket, server->sim, server->client_endpoint[index], server->buffer, 73);
		}
	}
}

static void s_protocol_server_disconnect_client(cn_protocol_server_t* server, uint32_t index, bool send_packets)
{
	if (!server->client_is_connected[index]) {
		return;
	}

	if (send_packets) {
		s_protocol_server_disconnect_sequence(server, index);
	}

	// Free client resources.
	server->client_count--;
	server->client_is_connected[index] = false;
	server->client_is_confirmed[index] = false;
	cn_hashtable_remove(&server->client_id_table, server->client_id + index);
	cn_hashtable_remove(&server->client_endpoint_table, server->client_endpoint + index);

	// Create a user notification.
	cn_protocol_server_event_t event;
	event.type = CN_PROTOCOL_SERVER_EVENT_DISCONNECTED;
	event.u.disconnected.client_index = index;
	s_protocol_server_event_push(server, &event);
}

bool cn_protocol_server_pop_event(cn_protocol_server_t* server, cn_protocol_server_event_t* event)
{
	return s_protocol_server_event_pull(server, event) ? false : true;
}

void cn_protocol_server_free_packet(cn_protocol_server_t* server, void* packet)
{
	cn_protocol_packet_payload_t* payload_packet = (cn_protocol_packet_payload_t*)((uint8_t*)packet - CN_OFFSET_OF(cn_protocol_packet_payload_t, payload));
	cn_protocol_packet_allocator_free(server->packet_allocator, (cn_protocol_packet_type_t)payload_packet->packet_type, payload_packet);
}

void cn_protocol_server_stop(cn_protocol_server_t* server)
{
	server->running = false;

	for (int i = 0; i < CN_PROTOCOL_SERVER_MAX_CLIENTS; ++i) {
		s_protocol_server_disconnect_client(server, i, false);
	}

	// Free any lingering payload packets.
	while (1)
	{
		cn_protocol_server_event_t event;
		if (s_protocol_server_event_pull(server, &event) < 0) break;
		if (event.type == CN_PROTOCOL_SERVER_EVENT_PAYLOAD_PACKET) {
			cn_protocol_server_free_packet(server, event.u.payload_packet.data);
		}
	}

	cn_protocol_encryption_map_cleanup(&server->encryption_map);
	cn_protocol_connect_token_cache_cleanup(&server->token_cache);
	cn_socket_cleanup(&server->socket);
	cn_hashtable_cleanup(&server->client_endpoint_table);
	cn_hashtable_cleanup(&server->client_id_table);
	cn_circular_buffer_reset(&server->event_queue);

	if (server->sim) {
		double latency = server->sim->latency;
		double jitter = server->sim->jitter;
		double drop_chance = server->sim->drop_chance;
		double duplicate_chance = server->sim->duplicate_chance;
		cn_simulator_destroy(server->sim);
		server->sim = cn_simulator_create(&server->socket, server->mem_ctx);
		server->sim->latency = latency;
		server->sim->jitter = jitter;
		server->sim->drop_chance = drop_chance;
		server->sim->duplicate_chance = duplicate_chance;
	}
}

bool cn_protocol_server_running(cn_protocol_server_t* server)
{
	return server->running;
}

static void s_protocol_server_connect_client(cn_protocol_server_t* server, cn_endpoint_t endpoint, cn_protocol_encryption_state_t* state)
{
	int index = -1;
	for (int i = 0; i < CN_PROTOCOL_SERVER_MAX_CLIENTS; ++i) {
		if (!server->client_is_connected[i]) {
			index = i;
			break;
		}
	}
	if (index == -1) return;

	server->client_count++;
	CN_ASSERT(server->client_count < CN_PROTOCOL_SERVER_MAX_CLIENTS);

	cn_protocol_server_event_t event;
	event.type = CN_PROTOCOL_SERVER_EVENT_NEW_CONNECTION;
	event.u.new_connection.client_index = index;
	event.u.new_connection.client_id = state->client_id;
	event.u.new_connection.endpoint = endpoint;
	if (s_protocol_server_event_push(server, &event) < 0) return;

	cn_hashtable_insert(&server->client_id_table, &state->client_id, &index);
	cn_hashtable_insert(&server->client_endpoint_table, &endpoint, &state->client_id);

	server->client_id[index] = state->client_id;
	server->client_is_connected[index] = true;
	server->client_is_confirmed[index] = false;
	server->client_last_packet_received_time[index] = 0;
	server->client_last_packet_sent_time[index] = 0;
	server->client_endpoint[index] = endpoint;
	server->client_sequence[index] = state->sequence;
	server->client_client_to_server_key[index] = state->client_to_server_key;
	server->client_server_to_client_key[index] = state->server_to_client_key;
	cn_protocol_replay_buffer_init(&server->client_replay_buffer[index]);

	cn_protocol_connect_token_cache_add(&server->token_cache, state->signature.bytes);
	cn_protocol_encryption_map_remove(&server->encryption_map, endpoint);

	cn_protocol_packet_connection_accepted_t packet;
	packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED;
	packet.client_id = state->client_id;
	packet.max_clients = CN_PROTOCOL_SERVER_MAX_CLIENTS;
	packet.connection_timeout = server->connection_timeout;
	if (cn_protocol_packet_write(&packet, server->buffer, server->client_sequence[index]++, server->client_server_to_client_key + index) == 16 + 73) {
		cn_socket_send(&server->socket, server->sim, server->client_endpoint[index], server->buffer, 16 + 73);
	}
}

static void s_protocol_server_receive_packets(cn_protocol_server_t* server)
{
	uint8_t* buffer = server->buffer;

	while (1)
	{
		cn_endpoint_t from;
		int sz = cn_socket_receive(&server->socket, &from, buffer, CN_PROTOCOL_PACKET_SIZE_MAX);
		if (!sz) break;

		if (sz < 73) {
			continue;
		}

		uint8_t type = *buffer;
		if (type > 7) {
			continue;
		}

		switch (type)
		{
		case CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED: // fall-thru
		case CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED: // fall-thru
		case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST:
			continue;
		}

		if (type == CN_PROTOCOL_PACKET_TYPE_CONNECT_TOKEN) {
			if (sz != 1024) {
				continue;
			}

			cn_protocol_connect_token_decrypted_t token;
			if (cn_is_error(cn_protocol_server_decrypt_connect_token_packet(buffer, &server->public_key, &server->secret_key, server->application_id, server->current_time, &token))) {
				continue;
			}

			cn_endpoint_t server_endpoint = server->socket.endpoint;
			int found = 0;
			for (int i = 0; i < token.endpoint_count; ++i)
			{
				if (cn_endpoint_equals(server_endpoint, token.endpoints[i])) {
					found = 1;
					break;
				}
			}
			if (!found) continue;

			int endpoint_already_connected = !!cn_hashtable_find(&server->client_endpoint_table, &from);
			if (endpoint_already_connected) continue;

			int client_id_already_connected = !!cn_hashtable_find(&server->client_id_table, &token.client_id);
			if (client_id_already_connected) continue;

			int token_already_in_use = !!cn_protocol_connect_token_cache_find(&server->token_cache, token.signature.bytes);
			if (token_already_in_use) continue;

			cn_protocol_encryption_state_t* state = cn_protocol_encryption_map_find(&server->encryption_map, from);
			if (!state) {
				cn_protocol_encryption_state_t encryption_state;
				encryption_state.sequence = 0;
				encryption_state.expiration_timestamp = token.expiration_timestamp;
				encryption_state.handshake_timeout = token.handshake_timeout;
				encryption_state.last_packet_recieved_time = 0;
				encryption_state.last_packet_sent_time = CN_PROTOCOL_SEND_RATE;
				encryption_state.client_to_server_key = token.client_to_server_key;
				encryption_state.server_to_client_key = token.server_to_client_key;
				encryption_state.client_id = token.client_id;
				CN_MEMCPY(encryption_state.signature.bytes, token.signature.bytes, CN_PROTOCOL_SIGNATURE_SIZE);
				cn_protocol_encryption_map_insert(&server->encryption_map, from, &encryption_state);
				state = cn_protocol_encryption_map_find(&server->encryption_map, from);
				CN_ASSERT(state);
			}

			if (server->client_count == CN_PROTOCOL_SERVER_MAX_CLIENTS) {
				cn_protocol_packet_connection_denied_t packet;
				packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED;
				if (cn_protocol_packet_write(&packet, server->buffer, state->sequence++, &token.server_to_client_key) == 73) {
					cn_socket_send(&server->socket, server->sim, from, server->buffer, 73);
					//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Sent %s to potential client (server is full).", s_protocol_packet_str(packet.packet_type));
				}
			}
		} else {
			uint64_t* client_id_ptr = (uint64_t*)cn_hashtable_find(&server->client_endpoint_table, &from);
			cn_protocol_replay_buffer_t* replay_buffer = NULL;
			const cn_crypto_key_t* client_to_server_key;
			cn_protocol_encryption_state_t* state = NULL;
			uint32_t index = ~0u;

			int endpoint_already_connected = !!client_id_ptr;
			if (endpoint_already_connected) {
				if (type == CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE) {
					// Someone already connected with this address.
					continue;
				}

				index = (uint32_t)*(int*)cn_hashtable_find(&server->client_id_table, client_id_ptr);
				replay_buffer = server->client_replay_buffer + index;
				client_to_server_key = server->client_client_to_server_key + index;
			} else {
				state = cn_protocol_encryption_map_find(&server->encryption_map, from);
				if (!state) continue;
				int connect_token_expired = state->expiration_timestamp <= server->current_time;
				if (connect_token_expired) {
					cn_protocol_encryption_map_remove(&server->encryption_map, from);
					continue;
				}
				client_to_server_key = &state->client_to_server_key;
			}

			void* packet_ptr = cn_protocol_packet_open(buffer, sz, client_to_server_key, server->packet_allocator, replay_buffer, NULL);
			if (!packet_ptr) continue;

			int free_packet = 1;

			switch (type)
			{
			case CN_PROTOCOL_PACKET_TYPE_KEEPALIVE:
				if (index == ~0u) break;
				server->client_last_packet_received_time[index] = 0;
				if (!server->client_is_confirmed[index]) //log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Client %" PRIu64 " is now *confirmed*.", server->client_id[index]);
				server->client_is_confirmed[index] = 1;
				break;

			case CN_PROTOCOL_PACKET_TYPE_DISCONNECT:
				if (index == ~0u) break;
				//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Client %" PRIu64 " has sent the server a DISCONNECT packet.", server->client_id[index]);
				s_protocol_server_disconnect_client(server, index, 0);
				break;

			case CN_PROTOCOL_PACKET_TYPE_CHALLENGE_RESPONSE:
			{
				CN_ASSERT(!endpoint_already_connected);
				int client_id_already_connected = !!cn_hashtable_find(&server->client_id_table, &state->client_id);
				if (client_id_already_connected) break;
				if (server->client_count == CN_PROTOCOL_SERVER_MAX_CLIENTS) {
					cn_protocol_packet_connection_denied_t packet;
					packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED;
					if (cn_protocol_packet_write(&packet, server->buffer, state->sequence++, &state->server_to_client_key) == 73) {
						cn_socket_send(&server->socket, server->sim, from, server->buffer, 73);
						//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Sent %s to potential client (server is full).", s_protocol_packet_str(packet.packet_type));
					}
				} else {
					s_protocol_server_connect_client(server, from, state);
				}
			}	break;

			case CN_PROTOCOL_PACKET_TYPE_PAYLOAD:
				if (index == ~0u) break;
				server->client_last_packet_received_time[index] = 0;
				if (!server->client_is_confirmed[index]) //log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Client %" PRIu64 " is now *confirmed*.", server->client_id[index]);
				server->client_is_confirmed[index] = 1;
				free_packet = 0;
				cn_protocol_packet_payload_t* packet = (cn_protocol_packet_payload_t*)packet_ptr;
				cn_protocol_server_event_t event;
				event.type = CN_PROTOCOL_SERVER_EVENT_PAYLOAD_PACKET;
				event.u.payload_packet.client_index = index;
				event.u.payload_packet.size = packet->payload_size;
				event.u.payload_packet.data = packet->payload;
				if (s_protocol_server_event_push(server, &event) < 0) {
					//log(CN_LOG_LEVEL_WARNING, "Protocol Server: Event queue is full; dropping payload packet for client %" PRIu64 ".", server->client_id[index]);
					free_packet = 1;
				}
				break;
			}

			if (free_packet) {
				cn_protocol_packet_allocator_free(server->packet_allocator, (cn_protocol_packet_type_t)type, packet_ptr);
			}
		}
	}
}

static void s_protocol_server_send_packets(cn_protocol_server_t* server, double dt)
{
	CN_ASSERT(server->running);

	// Send challenge request packets.
	int state_count = cn_protocol_encryption_map_count(&server->encryption_map);
	cn_protocol_encryption_state_t* states = cn_protocol_encryption_map_get_states(&server->encryption_map);
	cn_endpoint_t* endpoints = cn_protocol_encryption_map_get_endpoints(&server->encryption_map);
	uint8_t* buffer = server->buffer;
	cn_socket_t* the_socket = &server->socket;
	cn_simulator_t* sim = server->sim;
	for (int i = 0; i < state_count; ++i)
	{
		cn_protocol_encryption_state_t* state = states + i;
		state->last_packet_sent_time += dt;

		if (state->last_packet_sent_time >= CN_PROTOCOL_SEND_RATE) {
			state->last_packet_sent_time = 0;

			cn_protocol_packet_challenge_t packet;
			packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST;
			packet.challenge_nonce = server->challenge_nonce++;
			cn_crypto_random_bytes(packet.challenge_data, sizeof(packet.challenge_data));

			if (cn_protocol_packet_write(&packet, buffer, state->sequence++, &state->server_to_client_key) == 264 + 73) {
				cn_socket_send(the_socket, sim, endpoints[i], buffer, 264 + 73);
				//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Sent %s to potential client %" PRIu64 ".", s_protocol_packet_str(packet.packet_type), state->client_id);
			}
		}
	}

	// Update client timers.
	double* last_received_times = server->client_last_packet_received_time;
	double* last_sent_times = server->client_last_packet_sent_time;
	bool* connected = server->client_is_connected;
	for (int i = 0; i < CN_PROTOCOL_SERVER_MAX_CLIENTS; ++i) {
		if (connected[i]) {
			last_received_times[i] += dt;
			last_sent_times[i] += dt;
		}
	}

	// Send keepalive packets.
	bool* confirmed = server->client_is_confirmed;
	uint64_t* sequences = server->client_sequence;
	cn_crypto_key_t* server_to_client_keys = server->client_server_to_client_key;
	cn_handle_t* ids = server->client_id;
	endpoints = server->client_endpoint;
	uint32_t connection_timeout = server->connection_timeout;
	for (int i = 0; i < CN_PROTOCOL_SERVER_MAX_CLIENTS; ++i) {
		if (connected[i]) {
			if (last_sent_times[i] >= CN_PROTOCOL_SEND_RATE) {
				last_sent_times[i] = 0;

				if (!confirmed[i]) {
					cn_protocol_packet_connection_accepted_t packet;
					packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED;
					packet.client_id = ids[i];
					packet.max_clients = CN_PROTOCOL_SERVER_MAX_CLIENTS;
					packet.connection_timeout = connection_timeout;
					if (cn_protocol_packet_write(&packet, buffer, sequences[i]++, server_to_client_keys + i) == 16 + 73) {
						cn_socket_send(the_socket, sim, endpoints[i], buffer, 16 + 73);
						//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Sent %s to client %" PRIu64 ".", s_protocol_packet_str(packet.packet_type), server->client_id[i]);
					}
				}

				cn_protocol_packet_keepalive_t packet;
				packet.packet_type = CN_PROTOCOL_PACKET_TYPE_KEEPALIVE;
				if (cn_protocol_packet_write(&packet, buffer, sequences[i]++, server_to_client_keys + i) == 73) {
					cn_socket_send(the_socket, sim, endpoints[i], buffer, 73);
					//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Sent %s to client %" PRIu64 ".", s_protocol_packet_str(packet.packet_type), server->client_id[i]);
				}
			}
		}
	}
}

void cn_protocol_server_disconnect_client(cn_protocol_server_t* server, int client_index, bool notify_client)
{
	CN_ASSERT(server->client_count >= 1);
	s_protocol_server_disconnect_client(server, client_index, notify_client);
}

cn_error_t cn_protocol_server_send_to_client(cn_protocol_server_t* server, const void* packet, int size, int client_index)
{
	if (size < 0) return cn_error_failure("`size` is negative.");
	if (size > CN_PROTOCOL_PACKET_PAYLOAD_MAX) return cn_error_failure("`size` exceeds `CN_PROTOCOL_PACKET_PAYLOAD_MAX`.");
	CN_ASSERT(server->client_count >= 1 && client_index >= 0 && client_index < CN_PROTOCOL_SERVER_MAX_CLIENTS);

	int index = client_index;
	if (!server->client_is_confirmed[index]) {
		cn_protocol_packet_connection_accepted_t conn_accepted_packet;
		conn_accepted_packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED;
		conn_accepted_packet.client_id = server->client_id[index];
		conn_accepted_packet.max_clients = CN_PROTOCOL_SERVER_MAX_CLIENTS;
		conn_accepted_packet.connection_timeout = server->connection_timeout;
		if (cn_protocol_packet_write(&conn_accepted_packet, server->buffer, server->client_sequence[index]++, server->client_server_to_client_key + index) == 16 + 73) {
			cn_socket_send(&server->socket, server->sim, server->client_endpoint[index], server->buffer, 16 + 73);
			//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Sent %s to client %" PRIu64 ".", s_protocol_packet_str(packet.packet_type), server->client_id[index]);
			server->client_last_packet_sent_time[index] = 0;
		} else {
			return cn_error_failure("Failed to write packet.");
		}
	}

	cn_protocol_packet_payload_t payload;
	payload.packet_type = CN_PROTOCOL_PACKET_TYPE_PAYLOAD;
	payload.payload_size = size;
	CN_MEMCPY(payload.payload, packet, size);
	int sz = cn_protocol_packet_write(&payload, server->buffer, server->client_sequence[index]++, server->client_server_to_client_key + index);
	if (sz > 73) {
		cn_socket_send(&server->socket, server->sim, server->client_endpoint[index], server->buffer, sz);
		//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Sent %s to client %" PRIu64 ".", s_protocol_packet_str(payload.packet_type), server->client_id[index]);
		server->client_last_packet_sent_time[index] = 0;
	} else {
		return cn_error_failure("Failed to write packet.");
	}

	return cn_error_success();
}

static void s_protocol_server_look_for_timeouts(cn_protocol_server_t* server)
{
	double* last_received_times = server->client_last_packet_received_time;
	for (int i = 0; i < CN_PROTOCOL_SERVER_MAX_CLIENTS;) {
		if (server->client_is_connected[i]) {
			if (last_received_times[i] >= (double)server->connection_timeout) {
				//log(CN_LOG_LEVEL_INFORMATIONAL, "Protocol Server: Client %" PRIu64 " has timed out.", server->client_id[i]);
				cn_protocol_server_disconnect_client(server, i, true);
			} else {
				 ++i;
			}
		} else {
			++i;
		}
	}
}

void cn_protocol_server_update(cn_protocol_server_t* server, double dt, uint64_t current_time)
{
	server->current_time = current_time;

	cn_simulator_update(server->sim, dt);
	s_protocol_server_receive_packets(server);
	s_protocol_server_send_packets(server, dt);
	s_protocol_server_look_for_timeouts(server);
}

int cn_protocol_server_client_count(cn_protocol_server_t* server)
{
	return server->client_count;
}

uint64_t cn_protocol_server_get_client_id(cn_protocol_server_t* server, int client_index)
{
	CN_ASSERT(server->client_count >= 1 && client_index >= 0 && client_index < CN_PROTOCOL_SERVER_MAX_CLIENTS);
	return server->client_id[client_index];
}

bool cn_protocol_server_is_client_connected(cn_protocol_server_t* server, int client_index)
{
	return server->client_is_connected[client_index];
}

void cn_protocol_server_enable_network_simulator(cn_protocol_server_t* server, double latency, double jitter, double drop_chance, double duplicate_chance)
{
	cn_simulator_t* sim = cn_simulator_create(&server->socket, server->mem_ctx);
	sim->latency = latency;
	sim->jitter = jitter;
	sim->drop_chance = drop_chance;
	sim->duplicate_chance = duplicate_chance;
	server->sim = sim;
}

//--------------------------------------------------------------------------------------------------
// RELIABILITY TRANSPORT

typedef struct cn_sequence_buffer_t
{
	uint16_t sequence;
	int capacity;
	int stride;
	uint32_t* entry_sequence;
	uint8_t* entry_data;
	void* udata;
	void* mem_ctx;
} cn_sequence_buffer_t;

// -------------------------------------------------------------------------------------------------

#define CN_ACK_SYSTEM_HEADER_SIZE (2 + 2 + 4)
#define CN_ACK_SYSTEM_MAX_PACKET_SIZE 1180

typedef struct cn_ack_system_config_t
{
	int max_packet_size;
	int initial_ack_capacity;
	int sent_packets_sequence_buffer_size;
	int received_packets_sequence_buffer_size;

	int index;
	cn_error_t (*send_packet_fn)(int client_index, void* packet, int size, void* udata);

	void* udata;
	void* user_allocator_context;
} cn_ack_system_config_t;

cn_ack_system_config_t cn_ack_system_config_defaults()
{
	cn_ack_system_config_t config;
	config.max_packet_size = CN_ACK_SYSTEM_MAX_PACKET_SIZE;
	config.initial_ack_capacity = 256;
	config.sent_packets_sequence_buffer_size = 256;
	config.received_packets_sequence_buffer_size = 256;
	config.index = -1;
	config.send_packet_fn = NULL;
	config.udata = NULL;
	config.user_allocator_context = NULL;
	return config;
}

typedef enum cn_ack_system_counter_t
{
	CN_ACK_SYSTEM_COUNTERS_PACKETS_SENT,
	CN_ACK_SYSTEM_COUNTERS_PACKETS_RECEIVED,
	CN_ACK_SYSTEM_COUNTERS_PACKETS_ACKED,
	CN_ACK_SYSTEM_COUNTERS_PACKETS_STALE,
	CN_ACK_SYSTEM_COUNTERS_PACKETS_INVALID,
	CN_ACK_SYSTEM_COUNTERS_PACKETS_TOO_LARGE_TO_SEND,
	CN_ACK_SYSTEM_COUNTERS_PACKETS_TOO_LARGE_TO_RECEIVE,

	CN_ACK_SYSTEM_COUNTERS_MAX
} cn_ack_system_counter_t;

typedef struct cn_ack_system_t
{
	double time;
	int max_packet_size;

	void* udata;
	void* mem_ctx;

	uint16_t sequence;
	int acks_count;
	int acks_capacity;
	uint16_t* acks;
	cn_sequence_buffer_t sent_packets;
	cn_sequence_buffer_t received_packets;

	double rtt;
	double packet_loss;
	double outgoing_bandwidth_kbps;
	double incoming_bandwidth_kbps;

	int index;
	cn_error_t (*send_packet_fn)(int client_index, void* packet, int size, void* udata);

	uint64_t counters[CN_ACK_SYSTEM_COUNTERS_MAX];
} cn_ack_system_t;

// -------------------------------------------------------------------------------------------------

#define CN_TRANSPORT_HEADER_SIZE (1 + 2 + 2 + 2 + 2)
#define CN_TRANSPORT_MAX_FRAGMENT_SIZE 1100
#define CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES (1024)
#define CN_TRANSPORT_PACKET_PAYLOAD_MAX (1200)

#define CN_CHECK(X) if (X) ret = -1;

typedef void (cn_sequence_buffer_cleanup_entry_fn)(void* data, uint16_t sequence, void* udata, void* mem_ctx);

void cn_sequence_buffer_remove(cn_sequence_buffer_t* buffer, uint16_t sequence, cn_sequence_buffer_cleanup_entry_fn* cleanup_fn)
{
	int index = sequence % buffer->capacity;
	if (buffer->entry_sequence[index] != 0xFFFFFFFF)
	{
		buffer->entry_sequence[index] = 0xFFFFFFFF;
		if (cleanup_fn) cleanup_fn(buffer->entry_data + buffer->stride * index, buffer->entry_sequence[index], buffer->udata, buffer->mem_ctx);
	}
}

void cn_sequence_buffer_reset(cn_sequence_buffer_t* buffer, cn_sequence_buffer_cleanup_entry_fn* cleanup_fn)
{
	for (int i = 0; i < buffer->capacity; ++i)
	{
		cn_sequence_buffer_remove(buffer, i, cleanup_fn);
	}

	buffer->sequence = 0;
	CN_MEMSET(buffer->entry_sequence, ~0, sizeof(uint32_t) * buffer->capacity);
}

int cn_sequence_buffer_init(cn_sequence_buffer_t* buffer, int capacity, int stride, void* udata, void* mem_ctx)
{
	CN_MEMSET(buffer, 0, sizeof(cn_sequence_buffer_t));
	buffer->capacity = capacity;
	buffer->stride = stride;
	buffer->entry_sequence = (uint32_t*)CN_ALLOC(sizeof(uint32_t) * capacity, mem_ctx);
	buffer->entry_data = (uint8_t*)CN_ALLOC(stride * capacity, mem_ctx);
	buffer->udata = udata;
	buffer->mem_ctx = mem_ctx;
	CN_MEMSET(buffer->entry_sequence, ~0, sizeof(uint32_t) * buffer->capacity);
	cn_sequence_buffer_reset(buffer, NULL);
	return 0;
}

void cn_sequence_buffer_cleanup(cn_sequence_buffer_t* buffer, cn_sequence_buffer_cleanup_entry_fn* cleanup_fn)
{
	for (int i = 0; i < buffer->capacity; ++i)
	{
		cn_sequence_buffer_remove(buffer, i, cleanup_fn);
	}

	CN_FREE(buffer->entry_sequence, buffer->mem_ctx);
	CN_FREE(buffer->entry_data, buffer->mem_ctx);
	CN_MEMSET(buffer, 0, sizeof(cn_sequence_buffer_t));
}

static void s_sequence_buffer_remove_entries(cn_sequence_buffer_t* buffer, int sequence_a, int sequence_b, cn_sequence_buffer_cleanup_entry_fn* cleanup_fn)
{
	if (sequence_b < sequence_a) sequence_b += 65536;
	if (sequence_b - sequence_a < buffer->capacity) {
		for (int sequence = sequence_a; sequence <= sequence_b; ++sequence)
		{
			int index = sequence % buffer->capacity;
			if (cleanup_fn && buffer->entry_sequence[index] != 0xFFFFFFFF) {
				cleanup_fn(buffer->entry_data + buffer->stride * index, buffer->entry_sequence[index], buffer->udata, buffer->mem_ctx);
			}
			buffer->entry_sequence[index] = 0xFFFFFFFF;
		}
	} else {
		for (int i = 0; i < buffer->capacity; ++i)
		{
			if (cleanup_fn && buffer->entry_sequence[i] != 0xFFFFFFFF) {
				cleanup_fn(buffer->entry_data + buffer->stride * i, buffer->entry_sequence[i], buffer->udata, buffer->mem_ctx);
			}
			buffer->entry_sequence[i] = 0xFFFFFFFF;
		}
	}
}

static CN_INLINE int s_sequence_greater_than(uint16_t a, uint16_t b)
{
	return ((a > b) && (a - b <= 32768)) |
	       ((a < b) && (b - a  > 32768));
}

static CN_INLINE int s_sequence_less_than(uint16_t a, uint16_t b)
{
	return s_sequence_greater_than(b, a);
}

static CN_INLINE int s_sequence_is_stale(cn_sequence_buffer_t* buffer, uint16_t sequence)
{
	return s_sequence_less_than(sequence, buffer->sequence - ((uint16_t)buffer->capacity));
}

void* cn_sequence_buffer_insert(cn_sequence_buffer_t* buffer, uint16_t sequence, cn_sequence_buffer_cleanup_entry_fn* cleanup_fn)
{
	if (s_sequence_greater_than(sequence + 1, buffer->sequence)) {
		s_sequence_buffer_remove_entries(buffer, buffer->sequence, sequence, cleanup_fn);
		buffer->sequence = sequence + 1;
	} else if (s_sequence_is_stale(buffer, sequence)) {
		return NULL;
	}
	int index = sequence % buffer->capacity;
	if (cleanup_fn && buffer->entry_sequence[index] != 0xFFFFFFFF) {
		cleanup_fn(buffer->entry_data + buffer->stride * (sequence % buffer->capacity), buffer->entry_sequence[index], buffer->udata, buffer->mem_ctx);
	}
	buffer->entry_sequence[index] = sequence;
	return buffer->entry_data + index * buffer->stride;
}

int cn_sequence_buffer_is_empty(cn_sequence_buffer_t* sequence_buffer, uint16_t sequence)
{
	return sequence_buffer->entry_sequence[sequence % sequence_buffer->capacity] == 0xFFFFFFFF;
}

void* cn_sequence_buffer_find(cn_sequence_buffer_t* sequence_buffer, uint16_t sequence)
{
	int index = sequence % sequence_buffer->capacity;
	return ((sequence_buffer->entry_sequence[index] == (uint32_t)sequence)) ? (sequence_buffer->entry_data + index * sequence_buffer->stride) : NULL;
}

void* cn_sequence_buffer_at_index(cn_sequence_buffer_t* sequence_buffer, int index)
{
	CN_ASSERT(index >= 0);
	CN_ASSERT(index < sequence_buffer->capacity);
	return sequence_buffer->entry_sequence[index] != 0xFFFFFFFF ? (sequence_buffer->entry_data + index * sequence_buffer->stride) : NULL;
}

void cn_sequence_buffer_generate_ack_bits(cn_sequence_buffer_t* sequence_buffer, uint16_t* ack, uint32_t* ack_bits)
{
	*ack = sequence_buffer->sequence - 1;
	*ack_bits = 0;
	uint32_t mask = 1;
	for (int i = 0; i < 32; ++i)
	{
		uint16_t sequence = *ack - ((uint16_t)i);
		if (cn_sequence_buffer_find(sequence_buffer, sequence)) {
			*ack_bits |= mask;
		}
		mask <<= 1;
	}
}

// -------------------------------------------------------------------------------------------------

#define CN_PACKET_QUEUE_MAX_ENTRIES (1024)

typedef struct cn_packet_queue_t
{
	int count;
	int index0;
	int index1;
	int sizes[CN_PACKET_QUEUE_MAX_ENTRIES];
	void* packets[CN_PACKET_QUEUE_MAX_ENTRIES];
} cn_packet_queue_t;

void cn_packet_queue_init(cn_packet_queue_t* q)
{
	q->count = 0;
	q->index0 = 0;
	q->index1 = 0;
}

int cn_packet_queue_push(cn_packet_queue_t* q, void* packet, int size)
{
	if (q->count >= CN_PACKET_QUEUE_MAX_ENTRIES) {
		return -1;
	} else {
		q->count++;
		q->sizes[q->index1] = size;
		q->packets[q->index1] = packet;
		q->index1 = (q->index1 + 1) % CN_PACKET_QUEUE_MAX_ENTRIES;
		return 0;
	}
}

int cn_packet_queue_pop(cn_packet_queue_t* q, void** packet, int* size)
{
	if (q->count <= 0) {
		return -1;
	} else {
		q->count--;
		*size = q->sizes[q->index0];
		*packet = q->packets[q->index0];
		q->index0 = (q->index0 + 1) % CN_PACKET_QUEUE_MAX_ENTRIES;
		return 0;
	}
}

// -------------------------------------------------------------------------------------------------

typedef struct cn_socket_send_queue_item_t
{
	int fragment_index;
	int fragment_count;
	int final_fragment_size;

	int size;
	uint8_t* packet;
} cn_socket_send_queue_item_t;

typedef struct cn_socket_send_queue_t
{
	int count;
	int index0;
	int index1;
	cn_socket_send_queue_item_t items[CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES];
} cn_socket_send_queue_t;

static void s_send_queue_init(cn_socket_send_queue_t* q)
{
	q->count = 0;
	q->index0 = 0;
	q->index1 = 0;
}

static int s_send_queue_push(cn_socket_send_queue_t* q, const cn_socket_send_queue_item_t* item)
{
	CN_ASSERT(q->count >= 0);
	CN_ASSERT(q->index0 >= 0 && q->index0 < CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES);
	CN_ASSERT(q->index1 >= 0 && q->index1 < CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES);

	if (q->count < CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES) {
		int next_index = (q->index1 + 1) % CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES;
		q->items[next_index] = *item;
		q->index1 = next_index;
		++q->count;
		return 0;
	} else {
		return -1;
	}
}

static void s_send_queue_pop(cn_socket_send_queue_t* q)
{
	CN_ASSERT(q->count >= 0 && q->count < CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES);
	CN_ASSERT(q->index0 >= 0 && q->index0 < CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES);
	CN_ASSERT(q->index1 >= 0 && q->index1 < CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES);

	int next_index = (q->index0 + 1) % CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES;
	q->index0 = next_index;
	--q->count;
}

static int s_send_queue_peek(cn_socket_send_queue_t* q, cn_socket_send_queue_item_t** item)
{
	CN_ASSERT(q->count >= 0 && q->count < CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES);
	CN_ASSERT(q->index0 >= 0 && q->index0 < CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES);
	CN_ASSERT(q->index1 >= 0 && q->index1 < CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES);

	if (q->count > 0) {
		int next_index = (q->index0 + 1) % CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES;
		*item = q->items + next_index;
		return 0;
	} else {
		return -1;
	}
}

// -------------------------------------------------------------------------------------------------

typedef struct cn_fragment_t
{
	int index;
	double timestamp;
	cn_handle_t handle;
	uint8_t* data;
	int size;
} cn_fragment_t;

typedef struct cn_fragment_reassembly_entry_t
{
	int received_final_fragment;
	int packet_size;
	uint8_t* packet;

	int fragment_count_so_far;
	int fragments_total;
	uint8_t* fragment_received;
} cn_fragment_reassembly_entry_t;

typedef struct cn_packet_assembly_t
{
	uint16_t reassembly_sequence;
	cn_sequence_buffer_t fragment_reassembly;
	cn_packet_queue_t assembled_packets;
} cn_packet_assembly_t;

static void s_fragment_reassembly_entry_cleanup(void* data, uint16_t sequence, void* udata, void* mem_ctx)
{
	cn_fragment_reassembly_entry_t* reassembly = (cn_fragment_reassembly_entry_t*)data;
	CN_FREE(reassembly->packet, mem_ctx);
	CN_FREE(reassembly->fragment_received, mem_ctx);
}

static int s_packet_assembly_init(cn_packet_assembly_t* assembly, int max_fragments_in_flight, void* mem_ctx)
{
	int ret = 0;
	int reassembly_init = 0;

	assembly->reassembly_sequence = 0;

	CN_CHECK(cn_sequence_buffer_init(&assembly->fragment_reassembly, max_fragments_in_flight, sizeof(cn_fragment_reassembly_entry_t), NULL, mem_ctx));
	reassembly_init = 1;
	cn_packet_queue_init(&assembly->assembled_packets);

	if (ret) {
		if (reassembly_init) cn_sequence_buffer_cleanup(&assembly->fragment_reassembly, NULL);
	}

	return ret;
}

static void s_packet_assembly_cleanup(cn_packet_assembly_t* assembly)
{
	cn_sequence_buffer_cleanup(&assembly->fragment_reassembly, s_fragment_reassembly_entry_cleanup);
}

// -------------------------------------------------------------------------------------------------

typedef struct cn_transport_t
{
	int fragment_size;
	int max_packet_size;
	int max_fragments_in_flight;
	int max_size_single_send;

	cn_socket_send_queue_t send_queue;

	int fragments_count;
	int fragments_capacity;
	cn_fragment_t* fragments;
	cn_handle_allocator_t* fragment_handle_table;
	cn_sequence_buffer_t sent_fragments;

	cn_ack_system_t* ack_system;
	cn_packet_assembly_t reliable_and_in_order_assembly;
	cn_packet_assembly_t fire_and_forget_assembly;

	void* mem_ctx;
	void* udata;

	uint8_t fire_and_forget_buffer[CN_TRANSPORT_MAX_FRAGMENT_SIZE + CN_TRANSPORT_HEADER_SIZE];
} cn_transport_t;

// -------------------------------------------------------------------------------------------------

typedef struct cn_sent_packet_t
{
	double timestamp;
	int acked;
	int size;
} cn_sent_packet_t;

static void s_sent_packet_cleanup(void* data, uint16_t sequence, void* udata, void* mem_ctx)
{
	cn_sent_packet_t* packet = (cn_sent_packet_t*)data;
	packet->acked = 0;
}

typedef struct cn_received_packet_t
{
	double timestamp;
	int size;
} cn_received_packet_t;

cn_ack_system_t* cn_ack_system_create(cn_ack_system_config_t config)
{
	int ret = 0;
	int sent_packets_init = 0;
	int received_packets_init = 0;
	void* mem_ctx = config.user_allocator_context;

	if (!config.send_packet_fn) return NULL;
	if (config.max_packet_size > CN_TRANSPORT_PACKET_PAYLOAD_MAX) return NULL;

	cn_ack_system_t* ack_system = (cn_ack_system_t*)CN_ALLOC(sizeof(cn_ack_system_t), mem_ctx);
	if (!ack_system) return NULL;

	ack_system->time = 0;
	ack_system->max_packet_size = config.max_packet_size;
	ack_system->index = config.index;
	ack_system->send_packet_fn = config.send_packet_fn;
	ack_system->udata = config.udata;
	ack_system->mem_ctx = config.user_allocator_context;

	ack_system->sequence = 0;
	ack_system->acks_count = 0;
	ack_system->acks_capacity = config.initial_ack_capacity;
	ack_system->acks = (uint16_t*)CN_ALLOC(sizeof(uint16_t) * ack_system->acks_capacity, mem_ctx);
	CN_CHECK(cn_sequence_buffer_init(&ack_system->sent_packets, config.sent_packets_sequence_buffer_size, sizeof(cn_sent_packet_t), NULL, mem_ctx));
	sent_packets_init = 1;
	CN_CHECK(cn_sequence_buffer_init(&ack_system->received_packets, config.received_packets_sequence_buffer_size, sizeof(cn_received_packet_t), NULL, mem_ctx));
	received_packets_init = 1;

	ack_system->rtt = 0;
	ack_system->packet_loss = 0;
	ack_system->outgoing_bandwidth_kbps = 0;
	ack_system->incoming_bandwidth_kbps = 0;

	for (int i = 0; i < CN_ACK_SYSTEM_COUNTERS_MAX; ++i) {
		ack_system->counters[i] = 0;
	}

	if (ret) {
		if (ack_system)
		{
			if (sent_packets_init) cn_sequence_buffer_cleanup(&ack_system->sent_packets, NULL);
			if (received_packets_init) cn_sequence_buffer_cleanup(&ack_system->received_packets, NULL);
		}
		CN_FREE(ack_system->acks, mem_ctx);
		CN_FREE(ack_system, mem_ctx);
	}

	return ret ? NULL : ack_system;
}

void cn_ack_system_destroy(cn_ack_system_t* ack_system)
{
	if (!ack_system) return;
	void* mem_ctx = ack_system->mem_ctx;
	cn_sequence_buffer_cleanup(&ack_system->sent_packets, NULL);
	cn_sequence_buffer_cleanup(&ack_system->received_packets, NULL);
	CN_FREE(ack_system->acks, mem_ctx);
	CN_FREE(ack_system, mem_ctx);
}

void cn_ack_system_reset(cn_ack_system_t* ack_system)
{
	ack_system->sequence = 0;
	ack_system->acks_count = 0;
	cn_sequence_buffer_reset(&ack_system->sent_packets, NULL);
	cn_sequence_buffer_reset(&ack_system->received_packets, NULL);

	ack_system->rtt = 0;
	ack_system->packet_loss = 0;
	ack_system->outgoing_bandwidth_kbps = 0;
	ack_system->incoming_bandwidth_kbps = 0;

	for (int i = 0; i < CN_ACK_SYSTEM_COUNTERS_MAX; ++i) {
		ack_system->counters[i] = 0;
	}
}

static int s_write_ack_system_header(uint8_t* buffer, uint16_t sequence, uint16_t ack, uint32_t ack_bits)
{
	uint8_t* buffer_start = buffer;
	cn_write_uint16(&buffer, sequence);
	cn_write_uint16(&buffer, ack);
	cn_write_uint32(&buffer, ack_bits);
	return (int)(buffer - buffer_start);
}

cn_error_t cn_ack_system_send_packet(cn_ack_system_t* ack_system, void* data, int size, uint16_t* sequence_out)
{
	if (size > ack_system->max_packet_size || size > CN_ACK_SYSTEM_MAX_PACKET_SIZE) {
		ack_system->counters[CN_ACK_SYSTEM_COUNTERS_PACKETS_TOO_LARGE_TO_SEND]++;
		return cn_error_failure("Exceeded max packet size in ack system.");
	}

	uint16_t sequence = ack_system->sequence++;
	uint16_t ack;
	uint32_t ack_bits;

	cn_sequence_buffer_generate_ack_bits(&ack_system->received_packets, &ack, &ack_bits);
	cn_sent_packet_t* packet = (cn_sent_packet_t*)cn_sequence_buffer_insert(&ack_system->sent_packets, sequence, s_sent_packet_cleanup);
	CN_ASSERT(packet);

	packet->timestamp = ack_system->time;
	packet->acked = 0;
	packet->size = size + CN_ACK_SYSTEM_HEADER_SIZE;

	uint8_t buffer[CN_TRANSPORT_PACKET_PAYLOAD_MAX];
	int header_size = s_write_ack_system_header(buffer, sequence, ack, ack_bits);
	CN_ASSERT(header_size == CN_ACK_SYSTEM_HEADER_SIZE);
	CN_ASSERT(size + header_size < CN_TRANSPORT_PACKET_PAYLOAD_MAX);
	CN_MEMCPY(buffer + header_size, data, size);
	if (sequence_out) *sequence_out = sequence;
	cn_error_t err = ack_system->send_packet_fn(ack_system->index, buffer, size + header_size, ack_system->udata);
	if (cn_is_error(err)) {
		ack_system->counters[CN_ACK_SYSTEM_COUNTERS_PACKETS_INVALID]++;
		return err;
	}

	ack_system->counters[CN_ACK_SYSTEM_COUNTERS_PACKETS_SENT]++;

	return cn_error_success();
}

uint16_t cn_ack_system_get_sequence(cn_ack_system_t* ack_system)
{
	return ack_system->sequence;
}

static int s_read_ack_system_header(uint8_t* buffer, int size, uint16_t* sequence, uint16_t* ack, uint32_t* ack_bits)
{
	if (size < CN_ACK_SYSTEM_HEADER_SIZE) return -1;
	uint8_t* buffer_start = buffer;
	*sequence = cn_read_uint16(&buffer);
	*ack = cn_read_uint16(&buffer);
	*ack_bits = cn_read_uint32(&buffer);
	return (int)(buffer - buffer_start);
}

cn_error_t cn_ack_system_receive_packet(cn_ack_system_t* ack_system, void* data, int size)
{
	if (size > ack_system->max_packet_size || size > CN_ACK_SYSTEM_MAX_PACKET_SIZE) {
		ack_system->counters[CN_ACK_SYSTEM_COUNTERS_PACKETS_TOO_LARGE_TO_RECEIVE]++;
		return cn_error_failure("Exceeded max packet size in ack system.");
	}

	ack_system->counters[CN_ACK_SYSTEM_COUNTERS_PACKETS_RECEIVED]++;

	uint16_t sequence;
	uint16_t ack;
	uint32_t ack_bits;
	uint8_t* buffer = (uint8_t*)data;

	int header_size = s_read_ack_system_header(buffer, size, &sequence, &ack, &ack_bits);
	if (header_size < 0) {
		ack_system->counters[CN_ACK_SYSTEM_COUNTERS_PACKETS_INVALID]++;
		return cn_error_failure("Failed to write ack header.");
	}
	CN_ASSERT(header_size == CN_ACK_SYSTEM_HEADER_SIZE);

	if (s_sequence_is_stale(&ack_system->received_packets, sequence)) {
		ack_system->counters[CN_ACK_SYSTEM_COUNTERS_PACKETS_STALE]++;
		return cn_error_failure("The provided sequence number was stale.");
	}

	cn_received_packet_t* packet = (cn_received_packet_t*)cn_sequence_buffer_insert(&ack_system->received_packets, sequence, NULL);
	CN_ASSERT(packet);
	packet->timestamp = ack_system->time;
	packet->size = size;

	for (int i = 0; i < 32; ++i)
	{
		int bit_was_set = ack_bits & 1;
		ack_bits >>= 1;

		if (bit_was_set) {
			uint16_t ack_sequence = ack - ((uint16_t)i);
			cn_sent_packet_t* sent_packet = (cn_sent_packet_t*)cn_sequence_buffer_find(&ack_system->sent_packets, ack_sequence);

			if (sent_packet && !sent_packet->acked) {
				CN_CHECK_BUFFER_GROW(ack_system, acks_count, acks_capacity, acks, uint16_t);
				ack_system->acks[ack_system->acks_count++] = ack_sequence;
				ack_system->counters[CN_ACK_SYSTEM_COUNTERS_PACKETS_ACKED]++;
				sent_packet->acked = 1;

				float rtt = (float)(ack_system->time - sent_packet->timestamp);
				ack_system->rtt += (rtt - ack_system->rtt) * 0.001f;
				if (ack_system->rtt < 0) ack_system->rtt = 0;
			}
		}
	}

	return cn_error_success();
}

uint16_t* cn_ack_system_get_acks(cn_ack_system_t* ack_system)
{
	return ack_system->acks;
}

int cn_ack_system_get_acks_count(cn_ack_system_t* ack_system)
{
	return ack_system->acks_count;
}

void cn_ack_system_clear_acks(cn_ack_system_t* ack_system)
{
	ack_system->acks_count = 0;
}

static CN_INLINE double s_calc_packet_loss(double packet_loss, cn_sequence_buffer_t* sent_packets)
{
	int packet_count = 0;
	int packet_drop_count = 0;

	for (int i = 0; i < sent_packets->capacity; ++i)
	{
		cn_sent_packet_t* packet = (cn_sent_packet_t*)cn_sequence_buffer_at_index(sent_packets, i);
		if (packet) {
			packet_count++;
			if (!packet->acked) packet_drop_count++;
		}
	}

	double loss = (double)packet_drop_count / (double)packet_count;
	packet_loss += (loss - packet_loss) * 0.1;
	if (packet_loss < 0) packet_loss = 0;
	return packet_loss;
}

#include <float.h>

static CN_INLINE double s_calc_bandwidth(double bandwidth, cn_sequence_buffer_t* sent_packets)
{
	int bytes_sent = 0;
	double start_timestamp = DBL_MAX;
	double end_timestamp = 0;

	for (int i = 0; i < sent_packets->capacity; ++i)
	{
		cn_sent_packet_t* packet = (cn_sent_packet_t*)cn_sequence_buffer_at_index(sent_packets, i);
		if (packet) {
			bytes_sent += packet->size;
			if (packet->timestamp < start_timestamp) start_timestamp = packet->timestamp;
			if (packet->timestamp > end_timestamp) end_timestamp = packet->timestamp;
		}
	}

	if (start_timestamp != DBL_MAX) {
		double sent_bandwidth = (((double)bytes_sent / 1024.0) / (end_timestamp - start_timestamp));
		bandwidth += (sent_bandwidth - bandwidth) * 0.1f;
		if (bandwidth < 0) bandwidth = 0;
	}

	return bandwidth;
}

void cn_ack_system_update(cn_ack_system_t* ack_system, double dt)
{
	ack_system->time += dt;
	ack_system->packet_loss = s_calc_packet_loss(ack_system->packet_loss, &ack_system->sent_packets);
	ack_system->incoming_bandwidth_kbps = s_calc_bandwidth(ack_system->incoming_bandwidth_kbps, &ack_system->sent_packets);
	ack_system->outgoing_bandwidth_kbps = s_calc_bandwidth(ack_system->outgoing_bandwidth_kbps, &ack_system->received_packets);
}

double cn_ack_system_rtt(cn_ack_system_t* ack_system)
{
	return ack_system->rtt;
}

double cn_ack_system_packet_loss(cn_ack_system_t* ack_system)
{
	return ack_system->packet_loss;
}

double cn_ack_system_bandwidth_outgoing_kbps(cn_ack_system_t* ack_system)
{
	return ack_system->outgoing_bandwidth_kbps;
}

double cn_ack_system_bandwidth_incoming_kbps(cn_ack_system_t* ack_system)
{
	return ack_system->incoming_bandwidth_kbps;
}

uint64_t cn_ack_system_get_counter(cn_ack_system_t* ack_system, cn_ack_system_counter_t counter)
{
	return ack_system->counters[counter];
}

// -------------------------------------------------------------------------------------------------

typedef struct cn_fragment_entry_t
{
	cn_handle_t fragment_handle;
} cn_fragment_entry_t;

typedef struct cn_transport_config_t
{
	void* mem_ctx;
	int fragment_size;
	int max_packet_size;
	int max_fragments_in_flight;
	int max_size_single_send;
	int send_receive_queue_size;
	void* user_allocator_context;
	void* udata;

	int index;
	cn_error_t (*send_packet_fn)(int client_index, void* packet, int size, void* udata);
} cn_transport_config_t;

CN_INLINE cn_transport_config_t cn_transport_config_defaults()
{
	cn_transport_config_t config;
	config.mem_ctx = 0;
	config.fragment_size = CN_TRANSPORT_MAX_FRAGMENT_SIZE;
	config.max_packet_size = CN_TRANSPORT_MAX_FRAGMENT_SIZE * 4;
	config.max_fragments_in_flight = 8;
	config.max_size_single_send = (CN_MB) * 20;
	config.send_receive_queue_size = 1024;
	config.udata = NULL;
	config.index = -1;
	config.send_packet_fn = NULL;
	return config;
}

cn_transport_t* cn_transport_create(cn_transport_config_t config)
{
	if (!config.send_packet_fn) return NULL;

	int ret = 0;
	int table_init = 0;
	int sequence_sent_fragments_init = 0;
	int assembly_reliable_init = 0;
	int assembly_unreliable_init = 0;

	cn_transport_t* transport = (cn_transport_t*)CN_ALLOC(sizeof(cn_transport_t), config.user_allocator_context);
	if (!transport) return NULL;
	transport->fragment_size = config.fragment_size;
	transport->max_packet_size = config.max_packet_size;
	transport->max_fragments_in_flight = config.max_fragments_in_flight;
	transport->max_size_single_send = config.max_size_single_send;
	transport->udata = config.udata;

	transport->fragments_capacity = 256;
	transport->fragments_count = 0;
	transport->fragments = (cn_fragment_t*)CN_ALLOC(sizeof(cn_fragment_t) * transport->fragments_capacity, config.mem_ctx);

	cn_ack_system_config_t ack_config = cn_ack_system_config_defaults();
	ack_config.index = config.index;
	ack_config.send_packet_fn = config.send_packet_fn;
	ack_config.udata = config.udata;
	transport->ack_system = cn_ack_system_create(ack_config);
	transport->mem_ctx = config.user_allocator_context;

	transport->fragment_handle_table = cn_handle_allocator_create(config.send_receive_queue_size, transport->mem_ctx);
	table_init = 1;
	CN_CHECK(cn_sequence_buffer_init(&transport->sent_fragments, config.send_receive_queue_size, sizeof(cn_fragment_entry_t), transport, transport->mem_ctx));
	sequence_sent_fragments_init = 1;

	CN_CHECK(s_packet_assembly_init(&transport->reliable_and_in_order_assembly, config.send_receive_queue_size, transport->mem_ctx));
	assembly_reliable_init = 1;
	CN_CHECK(s_packet_assembly_init(&transport->fire_and_forget_assembly, config.send_receive_queue_size, transport->mem_ctx));
	assembly_unreliable_init = 1;

	s_send_queue_init(&transport->send_queue);

	if (ret) {
		if (table_init) cn_handle_allocator_destroy(transport->fragment_handle_table);
		if (sequence_sent_fragments_init) cn_sequence_buffer_cleanup(&transport->sent_fragments, NULL);
		if (assembly_reliable_init) s_packet_assembly_cleanup(&transport->reliable_and_in_order_assembly);
		if (assembly_unreliable_init) s_packet_assembly_cleanup(&transport->fire_and_forget_assembly);
		CN_FREE(transport, config.user_allocator_context);
	}

	return ret ? NULL : transport;
}

static void s_transport_cleanup_packet_queue(cn_transport_t* transport, cn_packet_queue_t* q)
{
	int index = q->index0;
	while (q->count--)
	{
		int next_index = index + 1 % CN_PACKET_QUEUE_MAX_ENTRIES;
		CN_FREE(q->packets[index], transport->mem_ctx);
		index = next_index;
	}
}

void cn_transport_destroy(cn_transport_t* transport)
{
	if (!transport) return;
	void* mem_ctx = transport->mem_ctx;

	s_transport_cleanup_packet_queue(transport, &transport->reliable_and_in_order_assembly.assembled_packets);
	s_transport_cleanup_packet_queue(transport, &transport->fire_and_forget_assembly.assembled_packets);

	cn_sequence_buffer_cleanup(&transport->sent_fragments, NULL);
	cn_handle_allocator_destroy(transport->fragment_handle_table);
	s_packet_assembly_cleanup(&transport->reliable_and_in_order_assembly);
	s_packet_assembly_cleanup(&transport->fire_and_forget_assembly);
	for (int i = 0; i < transport->fragments_count; ++i)
	{
		cn_fragment_t* fragment = transport->fragments + i;
		CN_FREE(fragment->data, mem_ctx);
	}
	CN_FREE(transport->fragments, mem_ctx);
	cn_ack_system_destroy(transport->ack_system);
	CN_FREE(transport, mem_ctx);
}

static CN_INLINE int s_transport_write_header(uint8_t* buffer, int size, uint8_t prefix, uint16_t sequence, uint16_t fragment_count, uint16_t fragment_index, uint16_t fragment_size)
{
	if (size < CN_TRANSPORT_HEADER_SIZE) return -1;
	uint8_t* buffer_start = buffer;
	cn_write_uint8(&buffer, prefix);
	cn_write_uint16(&buffer, sequence);
	cn_write_uint16(&buffer, fragment_count);
	cn_write_uint16(&buffer, fragment_index);
	cn_write_uint16(&buffer, fragment_size);
	return (int)(buffer - buffer_start);
}

static cn_error_t s_transport_send_fragments(cn_transport_t* transport)
{
	CN_ASSERT(transport->fragments_count <= transport->max_fragments_in_flight);
	if (transport->fragments_count == transport->max_fragments_in_flight) {
		return cn_error_failure("Too many fragments already in flight.");
	}

	double timestamp = transport->ack_system->time;
	uint16_t reassembly_sequence = transport->reliable_and_in_order_assembly.reassembly_sequence;
	int fragments_space_available_send = transport->max_fragments_in_flight - transport->fragments_count;
	int fragment_size = transport->fragment_size;

	while (fragments_space_available_send)
	{
		cn_socket_send_queue_item_t* item;
		if (s_send_queue_peek(&transport->send_queue, &item) < 0) {
			break;
		}

		uint8_t* data_ptr = item->packet + item->fragment_index * fragment_size;
		int fragment_count_left = item->fragment_count - item->fragment_index;
		int fragment_count_to_send = fragments_space_available_send < fragment_count_left ? fragments_space_available_send : fragment_count_left;
		//if (item->fragment_index + fragment_count_to_send > item->fragment_count) __debugbreak();
		CN_ASSERT(item->fragment_index + fragment_count_to_send <= item->fragment_count);

		for (int i = 0; i < fragment_count_to_send; ++i)
		{
			// Allocate fragment.
			uint16_t fragment_header_index = (uint16_t)(item->fragment_index + i);
			int this_fragment_size = fragment_header_index != item->fragment_count - 1 ? fragment_size : item->final_fragment_size;
			uint8_t* fragment_src = data_ptr + fragment_size * i;
			CN_ASSERT(this_fragment_size <= CN_ACK_SYSTEM_MAX_PACKET_SIZE);

			int fragment_index = transport->fragments_count;
			CN_CHECK_BUFFER_GROW(transport, fragments_count, fragments_capacity, fragments, cn_fragment_t);
			cn_fragment_t* fragment = transport->fragments + transport->fragments_count++;
			cn_handle_t fragment_handle = cn_handle_allocator_alloc(transport->fragment_handle_table, fragment_index);

			fragment->index = fragment_header_index;
			fragment->timestamp = timestamp;
			fragment->handle = fragment_handle;
			fragment->data = (uint8_t*)CN_ALLOC(fragment_size + CN_TRANSPORT_HEADER_SIZE, transport->mem_ctx);
			fragment->size = this_fragment_size;
			// TODO: Memory pool on sent fragments.

			// Write the transport header.
			int header_size = s_transport_write_header(fragment->data, this_fragment_size + CN_TRANSPORT_HEADER_SIZE, 1, reassembly_sequence, item->fragment_count, fragment_header_index, (uint16_t)this_fragment_size);
			if (header_size != CN_TRANSPORT_HEADER_SIZE) {
				CN_FREE(fragment->data, transport->mem_ctx);
				transport->fragments_count--;
				return cn_error_failure("Failed to write transport header.");
			}

			// Copy over the `data` from user.
			CN_MEMCPY(fragment->data + header_size, fragment_src, this_fragment_size);

			// Send to ack system.
			uint16_t sequence;
			cn_error_t err = cn_ack_system_send_packet(transport->ack_system, fragment->data, this_fragment_size + CN_TRANSPORT_HEADER_SIZE, &sequence);
			if (cn_is_error(err)) {
				CN_FREE(fragment->data, transport->mem_ctx);
				transport->fragments_count--;
				return err;
			}

			// If all succeeds, record fragment entry. Hopefully it will be acked later.
			cn_fragment_entry_t* fragment_entry = (cn_fragment_entry_t*)cn_sequence_buffer_insert(&transport->sent_fragments, sequence, NULL);
			CN_ASSERT(fragment_entry);
			fragment_entry->fragment_handle = fragment_handle;
		}

		if (item->fragment_index + fragment_count_to_send == item->fragment_count) {
			s_send_queue_pop(&transport->send_queue);
			CN_FREE(item->packet, transport->mem_ctx);
			transport->reliable_and_in_order_assembly.reassembly_sequence++;
		} else {
			item->fragment_index += fragment_count_to_send;
		}

		fragments_space_available_send -= fragment_count_to_send;
	}

	return cn_error_success();
}

cn_error_t s_transport_send_reliably(cn_transport_t* transport, const void* data, int size)
{
	if (size < 0) return cn_error_failure("Negative `size` not allowed.");
	if (size > transport->max_size_single_send) return cn_error_failure("`size` exceeded `max_size_single_send` from `transport->config`.");

	int fragment_size = transport->fragment_size;
	int fragment_count = size / fragment_size;
	int final_fragment_size = size - (fragment_count * fragment_size);
	if (final_fragment_size > 0) fragment_count++;

	cn_socket_send_queue_item_t send_item;
	send_item.fragment_index = 0;
	send_item.fragment_count = fragment_count;
	send_item.final_fragment_size = final_fragment_size;
	send_item.size = size;
	send_item.packet = (uint8_t*)CN_ALLOC(size, transport->mem_ctx);
	if (!send_item.packet) return cn_error_failure("Failed allocation.");
	CN_MEMCPY(send_item.packet, data, size);

	if (s_send_queue_push(&transport->send_queue, &send_item) < 0) {
		return cn_error_failure("Send queue for reliable-and-in-order packets is full. Increase `CN_TRANSPORT_SEND_QUEUE_MAX_ENTRIES` or send packets less frequently.");
	}

	s_transport_send_fragments(transport);

	return cn_error_success();
}

cn_error_t s_transport_send(cn_transport_t* transport, const void* data, int size)
{
	if (size < 0) return cn_error_failure("Negative `size` is not valid.");
	if (size > transport->max_size_single_send) return cn_error_failure("`size` exceeded `max_size_single_send` config param.");

	int fragment_size = transport->fragment_size;
	int fragment_count = size / fragment_size;
	int final_fragment_size = size - (fragment_count * fragment_size);
	if (final_fragment_size > 0) fragment_count++;

	uint16_t reassembly_sequence = transport->fire_and_forget_assembly.reassembly_sequence++;
	uint8_t* buffer = transport->fire_and_forget_buffer;

	uint8_t* data_ptr = (uint8_t*)data;
	for (int i = 0; i < fragment_count; ++i)
	{
		int this_fragment_size = i != fragment_count - 1 ? fragment_size : final_fragment_size;
		uint8_t* fragment_src = data_ptr + fragment_size * i;
		CN_ASSERT(this_fragment_size <= CN_ACK_SYSTEM_MAX_PACKET_SIZE);

		// Write the transport header.
		int header_size = s_transport_write_header(buffer, this_fragment_size + CN_TRANSPORT_HEADER_SIZE, 0, reassembly_sequence, fragment_count, (uint16_t)i, (uint16_t)this_fragment_size);
		if (header_size != CN_TRANSPORT_HEADER_SIZE) {
			return cn_error_failure("Failed writing transport header -- incorrect size of bytes written (this is probably a bug).");
		}

		// Copy over fragment data from src.
		CN_MEMCPY(buffer + CN_TRANSPORT_HEADER_SIZE, fragment_src, this_fragment_size);

		// Send to ack system.
		uint16_t sequence;
		cn_error_t err = cn_ack_system_send_packet(transport->ack_system, buffer, this_fragment_size + CN_TRANSPORT_HEADER_SIZE, &sequence);
		if (cn_is_error(err)) return err;
	}

	return cn_error_success();
}

cn_error_t cn_transport_send(cn_transport_t* transport, const void* data, int size, bool send_reliably)
{
	if (send_reliably) {
		return s_transport_send_reliably(transport, data, size);
	} else {
		return s_transport_send(transport, data, size);
	}
}

cn_error_t cn_transport_receive_reliably_and_in_order(cn_transport_t* transport, void** data, int* size)
{
	cn_packet_assembly_t* assembly = &transport->reliable_and_in_order_assembly;
	if (cn_packet_queue_pop(&assembly->assembled_packets, data, size) < 0) {
		*data = NULL;
		*size = 0;
		return cn_error_failure("No data.");
	} else {
		return cn_error_success();
	}
}

cn_error_t cn_transport_receive_fire_and_forget(cn_transport_t* transport, void** data, int* size)
{
	cn_packet_assembly_t* assembly = &transport->fire_and_forget_assembly;
	if (cn_packet_queue_pop(&assembly->assembled_packets, data, size) < 0) {
		*data = NULL;
		*size = 0;
		return cn_error_failure("No data.");
	} else {
		return cn_error_success();
	}
}

void cn_transport_free_packet(cn_transport_t* transport, void* data)
{
	CN_FREE(data, transport->mem_ctx);
}

cn_error_t cn_transport_process_packet(cn_transport_t* transport, void* data, int size)
{
	if (size < CN_TRANSPORT_HEADER_SIZE) return cn_error_failure("`size` is too small to fit `CN_TRANSPORT_HEADER_SIZE`.");
	cn_error_t err = cn_ack_system_receive_packet(transport->ack_system, data, size);
	if (cn_is_error(err)) return err;

	// Read transport header.
	uint8_t* buffer = (uint8_t*)data + CN_ACK_SYSTEM_HEADER_SIZE;
	uint8_t prefix = cn_read_uint8(&buffer);
	uint16_t reassembly_sequence = cn_read_uint16(&buffer);
	uint16_t fragment_count = cn_read_uint16(&buffer);
	uint16_t fragment_index = cn_read_uint16(&buffer);
	uint16_t fragment_size = cn_read_uint16(&buffer);
	int total_packet_size = fragment_count * transport->fragment_size;

	if (total_packet_size > transport->max_size_single_send) {
		return cn_error_failure("Packet exceeded `max_size_single_send` limit.");
	}

	if (fragment_index > fragment_count) {
		return cn_error_failure("Fragment index out of bounds.");
	}

	if (fragment_size > transport->fragment_size) {
		return cn_error_failure("Fragment size somehow didn't match `transport->fragment_size`.");
	}

	cn_packet_assembly_t* assembly;
	if (prefix) {
		assembly = &transport->reliable_and_in_order_assembly;
	} else {
		assembly = &transport->fire_and_forget_assembly;
	}

	// Build reassembly if it doesn't exist yet.
	cn_fragment_reassembly_entry_t* reassembly = (cn_fragment_reassembly_entry_t*)cn_sequence_buffer_find(&assembly->fragment_reassembly, reassembly_sequence);
	if (!reassembly) {
		if (s_sequence_less_than(reassembly_sequence, assembly->fragment_reassembly.sequence)) {
			return cn_error_failure("Old sequence encountered (this packet was already reassembled fully).");
		}
		reassembly = (cn_fragment_reassembly_entry_t*)cn_sequence_buffer_insert(&assembly->fragment_reassembly, reassembly_sequence, s_fragment_reassembly_entry_cleanup);
		if (!reassembly) {
			return cn_error_failure("Sequence for this reassembly is stale.");
		}
		reassembly->received_final_fragment = 0;
		reassembly->packet_size = total_packet_size;
		reassembly->packet = (uint8_t*)CN_ALLOC(total_packet_size, transport->mem_ctx);
		if (!reassembly->packet) return cn_error_failure("Failed allocation.");
		reassembly->fragment_received = (uint8_t*)CN_ALLOC(fragment_count, transport->mem_ctx);
		if (!reassembly->fragment_received) {
			CN_FREE(reassembly->packet, transport->mem_ctx);
			return cn_error_failure("Full packet not yet received.");
		}
		CN_MEMSET(reassembly->fragment_received, 0, fragment_count);
		reassembly->fragment_count_so_far = 0;
		reassembly->fragments_total = fragment_count;
	}

	if (fragment_count != reassembly->fragments_total) {
		return cn_error_failure("Full packet not yet received.");
	}

	if (reassembly->fragment_received[fragment_index]) {
		return cn_error_success();
	}

	// Copy in fragment pieces into a single large packet buffer.
	reassembly->fragment_count_so_far++;
	reassembly->fragment_received[fragment_index] = 1;

	uint8_t* packet_fragment = reassembly->packet + fragment_index * transport->fragment_size;
	CN_MEMCPY(packet_fragment, buffer, fragment_size);

	if (fragment_index == fragment_count - 1) {
		reassembly->received_final_fragment = 1;
		reassembly->packet_size -= transport->fragment_size - fragment_size;
		total_packet_size -= transport->fragment_size - fragment_size;
	}

	// Store completed packet for retrieval by user.
	if (reassembly->fragment_count_so_far == fragment_count) {
		if (cn_packet_queue_push(&assembly->assembled_packets, reassembly->packet, reassembly->packet_size) < 0) {
			//TODO: Log. Dropped packet since reassembly buffer was too small.
			CN_FREE(reassembly->packet, transport->mem_ctx);
			CN_ASSERT(false); // ??? Is this allowed ???
		}
		reassembly->packet = NULL;
		cn_sequence_buffer_remove(&assembly->fragment_reassembly, reassembly_sequence, s_fragment_reassembly_entry_cleanup);
	}

	return cn_error_success();
}

void cn_transport_process_acks(cn_transport_t* transport)
{
	uint16_t* acks = cn_ack_system_get_acks(transport->ack_system);
	int acks_count = cn_ack_system_get_acks_count(transport->ack_system);

	for (int i = 0; i < acks_count; ++i)
	{
		uint16_t sequence = acks[i];
		cn_fragment_entry_t* fragment_entry = (cn_fragment_entry_t*)cn_sequence_buffer_find(&transport->sent_fragments, sequence);
		if (fragment_entry) {
			cn_handle_t h = fragment_entry->fragment_handle;
			if (cn_handle_allocator_is_handle_valid(transport->fragment_handle_table, h)) {
				uint32_t index = cn_handle_allocator_get_index(transport->fragment_handle_table, h);
				CN_ASSERT((int)index < transport->fragments_count);
				cn_handle_allocator_free(transport->fragment_handle_table, h);
				cn_fragment_t* fragment = transport->fragments + index;
				CN_FREE(fragment->data, transport->mem_ctx);
				cn_handle_t last_handle = transport->fragments[transport->fragments_count - 1].handle;
				if (cn_handle_allocator_is_handle_valid(transport->fragment_handle_table, last_handle)) {
					cn_handle_allocator_update_index(transport->fragment_handle_table, last_handle, index);
				}
				transport->fragments[index] = transport->fragments[--(transport->fragments_count)];
				cn_sequence_buffer_remove(&transport->sent_fragments, sequence, NULL);
			}
		}
	}

	cn_ack_system_clear_acks(transport->ack_system);
}

void cn_transport_resend_unacked_fragments(cn_transport_t* transport)
{
	// Resend unacked fragments which were previously sent.
	double timestamp = transport->ack_system->time;
	int count = transport->fragments_count;
	cn_fragment_t* fragments = transport->fragments;

	for (int i = 0; i < count;)
	{
		cn_fragment_t* fragment = fragments + i;
		if (fragment->timestamp + 0.01f >= timestamp) {
			++i;
			continue;
		}

		// Send to ack system.
		uint16_t sequence;
		cn_error_t err = cn_ack_system_send_packet(transport->ack_system, fragment->data, fragment->size + CN_TRANSPORT_HEADER_SIZE, &sequence);
		if (cn_is_error(err)) {
			// Remove failed fragments (this should never happen, and is only here for safety).
			cn_handle_allocator_free(transport->fragment_handle_table, fragment->handle);
			CN_FREE(fragment->data, transport->mem_ctx);
			transport->fragments[i] = transport->fragments[--(transport->fragments_count)];
			--count;
			continue;
		} else {
			 ++i;
		}

		fragment->timestamp = timestamp;
		cn_fragment_entry_t* fragment_entry = (cn_fragment_entry_t*)cn_sequence_buffer_insert(&transport->sent_fragments, sequence, NULL);
		CN_ASSERT(fragment_entry);
		fragment_entry->fragment_handle = fragment->handle;
	}

	// Send off any available fragments from the send queue.
	s_transport_send_fragments(transport);
}

int cn_transport_unacked_fragment_count(cn_transport_t* transport)
{
	return transport->fragments_count;
}

void cn_transport_update(cn_transport_t* transport, double dt)
{
	cn_ack_system_update(transport->ack_system, dt);
	cn_transport_process_acks(transport);
	cn_transport_resend_unacked_fragments(transport);
}

//--------------------------------------------------------------------------------------------------
// CLIENT

struct cn_client_t
{
	cn_protocol_client_t* p_client;
	cn_transport_t* transport;
	void* mem_ctx;
};

static cn_error_t s_send(int client_index, void* packet, int size, void* udata)
{
	cn_client_t* client = (cn_client_t*)udata;
	return cn_protocol_client_send(client->p_client, packet, size);
}

cn_client_t* cn_client_create(uint16_t port, uint64_t application_id, bool use_ipv6, void* user_allocator_context)
{
	cn_error_t err = s_cn_init_check();
	if (cn_is_error(err)) return NULL;

	cn_protocol_client_t* p_client = cn_protocol_client_create(port, application_id, use_ipv6, user_allocator_context);
	if (!p_client) return NULL;

	cn_client_t* client = (cn_client_t*)CN_ALLOC(sizeof(cn_client_t), user_allocator_context);
	CN_MEMSET(client, 0, sizeof(*client));
	client->p_client = p_client;
	client->mem_ctx = user_allocator_context;

	cn_transport_config_t config = cn_transport_config_defaults();
	config.send_packet_fn = s_send;
	config.user_allocator_context = user_allocator_context;
	config.udata = client;
	client->transport = cn_transport_create(config);

	return client;
}

void cn_client_destroy(cn_client_t* client)
{
	if (!client) return;
	cn_protocol_client_destroy(client->p_client);
	cn_transport_destroy(client->transport);
	void* mem_ctx = client->mem_ctx;
	CN_FREE(client, mem_ctx);
}

cn_error_t cn_client_connect(cn_client_t* client, const uint8_t* connect_token)
{
	return cn_protocol_client_connect(client->p_client, connect_token);
}

void cn_client_disconnect(cn_client_t* client)
{
	cn_protocol_client_disconnect(client->p_client);
}

void cn_client_update(cn_client_t* client, double dt, uint64_t current_time)
{
	cn_protocol_client_update(client->p_client, dt, current_time);

	if (cn_protocol_client_get_state(client->p_client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) {
		cn_transport_update(client->transport, dt);

		void* packet;
		int size;
		uint64_t sequence;
		while (cn_protocol_client_get_packet(client->p_client, &packet, &size, &sequence)) {
			cn_transport_process_packet(client->transport, packet, size);
			cn_protocol_client_free_packet(client->p_client, packet);
		}
	}
}

bool cn_client_pop_packet(cn_client_t* client, void** packet, int* size, bool* was_sent_reliably)
{
	if (cn_protocol_client_get_state(client->p_client) != CN_PROTOCOL_CLIENT_STATE_CONNECTED) {
		return false;
	}

	bool got = !cn_is_error(cn_transport_receive_reliably_and_in_order(client->transport, packet, size));
	if (was_sent_reliably && got) *was_sent_reliably = true;
	if (!got) {
		got = !cn_is_error(cn_transport_receive_fire_and_forget(client->transport, packet, size));
		if (was_sent_reliably) *was_sent_reliably = false;
	}
	return got;
}

void cn_client_free_packet(cn_client_t* client, void* packet)
{
	cn_transport_free_packet(client->transport, packet);
}

cn_error_t cn_client_send(cn_client_t* client, const void* packet, int size, bool send_reliably)
{
	if (cn_protocol_client_get_state(client->p_client) != CN_PROTOCOL_CLIENT_STATE_CONNECTED) {
		return cn_error_failure("Client is not connected.");
	}

	return cn_transport_send(client->transport, packet, size, send_reliably);
}

cn_client_state_t cn_client_state_get(const cn_client_t* client)
{
	return (cn_client_state_t)cn_protocol_client_get_state(client->p_client);
}

const char* cn_client_state_string(cn_client_state_t state)
{
	switch (state) {
	case CN_CLIENT_STATE_CONNECT_TOKEN_EXPIRED: return "CN_CLIENT_STATE_CONNECT_TOKEN_EXPIRED";
	case CN_CLIENT_STATE_INVALID_CONNECT_TOKEN: return "CN_CLIENT_STATE_INVALID_CONNECT_TOKEN";
	case CN_CLIENT_STATE_CONNECTION_TIMED_OUT: return "CN_CLIENT_STATE_CONNECTION_TIMED_OUT";
	case CN_CLIENT_STATE_CHALLENGE_RESPONSE_TIMED_OUT: return "CN_CLIENT_STATE_CHALLENGE_RESPONSE_TIMED_OUT";
	case CN_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT: return "CN_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT";
	case CN_CLIENT_STATE_CONNECTION_DENIED: return "CN_CLIENT_STATE_CONNECTION_DENIED";
	case CN_CLIENT_STATE_DISCONNECTED: return "CN_CLIENT_STATE_DISCONNECTED";
	case CN_CLIENT_STATE_SENDING_CONNECTION_REQUEST: return "CN_CLIENT_STATE_SENDING_CONNECTION_REQUEST";
	case CN_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE: return "CN_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE";
	case CN_CLIENT_STATE_CONNECTED: return "CN_CLIENT_STATE_CONNECTED";
	}
	CN_ASSERT(false);
	return NULL;
}

float cn_client_time_of_last_packet_recieved(const cn_client_t* client)
{
	return 0;
}

void cn_client_enable_network_simulator(cn_client_t* client, double latency, double jitter, double drop_chance, double duplicate_chance)
{
	cn_protocol_client_enable_network_simulator(client->p_client, latency, jitter, drop_chance, duplicate_chance);
}

//--------------------------------------------------------------------------------------------------
// SERVER

struct cn_server_t
{
	bool running;
	cn_endpoint_t endpoint;
	cn_crypto_sign_public_t public_key;
	cn_crypto_sign_secret_t secret_key;
	cn_server_config_t config;
	cn_socket_t socket;
	uint8_t buffer[CN_PROTOCOL_PACKET_SIZE_MAX];
	cn_circular_buffer_t event_queue;
	cn_transport_t* client_transports[CN_SERVER_MAX_CLIENTS];
	cn_protocol_server_t* p_server;
	void* mem_ctx;
};

static cn_error_t s_send_packet_fn(int client_index, void* packet, int size, void* udata)
{
	cn_server_t* server = (cn_server_t*)udata;
	return cn_protocol_server_send_to_client(server->p_server, packet, size, client_index);
}

cn_server_t* cn_server_create(cn_server_config_t config)
{
	cn_error_t err = s_cn_init_check();
	if (cn_is_error(err)) return NULL;

	cn_server_t* server = (cn_server_t*)CN_ALLOC(sizeof(cn_server_t), config.user_allocator_context);
	CN_MEMSET(server, 0, sizeof(*server));
	server->mem_ctx = config.user_allocator_context;
	server->config = config;
	server->event_queue = cn_circular_buffer_create(CN_MB * 10, config.user_allocator_context);
	server->p_server = cn_protocol_server_create(config.application_id, &server->config.public_key, &server->config.secret_key, server->mem_ctx);

	return server;
}

void cn_server_destroy(cn_server_t* server)
{
	if (!server) return;
	cn_server_stop(server);
	cn_protocol_server_destroy(server->p_server);
	void* mem_ctx = server->mem_ctx;
	cn_circular_buffer_free(&server->event_queue);
	CN_FREE(server, mem_ctx);
}

cn_error_t cn_server_start(cn_server_t* server, const char* address_and_port)
{
	cn_error_t err = cn_protocol_server_start(server->p_server, address_and_port, (uint32_t)server->config.connection_timeout);
	if (cn_is_error(err)) return err;

	for (int i = 0; i < CN_SERVER_MAX_CLIENTS; ++i) {
		cn_transport_config_t transport_config = cn_transport_config_defaults();
		transport_config.index = i;
		transport_config.send_packet_fn = s_send_packet_fn;
		transport_config.udata = server;
		transport_config.user_allocator_context = server->mem_ctx;
		server->client_transports[i] = cn_transport_create(transport_config);
	}

	return cn_error_success();
}

void cn_server_stop(cn_server_t* server)
{
	if (!server) return;
	cn_circular_buffer_reset(&server->event_queue);
	cn_protocol_server_stop(server->p_server);
	for (int i = 0; i < CN_SERVER_MAX_CLIENTS; ++i) {
		cn_transport_destroy(server->client_transports[i]);
		server->client_transports[i] = NULL;
	}
}

static CN_INLINE int s_server_event_pull(cn_server_t* server, cn_server_event_t* event)
{
	return cn_circular_buffer_pull(&server->event_queue, event, sizeof(cn_server_event_t));
}

static CN_INLINE int s_server_event_push(cn_server_t* server, cn_server_event_t* event)
{
	if (cn_circular_buffer_push(&server->event_queue, event, sizeof(cn_server_event_t)) < 0) {
		if (cn_circular_buffer_grow(&server->event_queue, server->event_queue.capacity * 2) < 0) {
			return -1;
		}
		return cn_circular_buffer_push(&server->event_queue, event, sizeof(cn_server_event_t));
	} else {
		return 0;
	}
}

void cn_server_update(cn_server_t* server, double dt, uint64_t current_time)
{
	// Update the protocol server.
	cn_protocol_server_update(server->p_server, dt, current_time);

	// Capture any events from the protocol server and process them.
	cn_protocol_server_event_t p_event;
	while (cn_protocol_server_pop_event(server->p_server, &p_event)) {
		switch (p_event.type) {
		case CN_PROTOCOL_SERVER_EVENT_NEW_CONNECTION:
		{
			cn_server_event_t e;
			e.type = CN_SERVER_EVENT_TYPE_NEW_CONNECTION;
			e.u.new_connection.client_index = p_event.u.new_connection.client_index;
			e.u.new_connection.client_id = p_event.u.new_connection.client_id;
			e.u.new_connection.endpoint = p_event.u.new_connection.endpoint;
			s_server_event_push(server, &e);
		}	break;

		case CN_PROTOCOL_SERVER_EVENT_DISCONNECTED:
		{
			cn_server_event_t e;
			e.type = CN_SERVER_EVENT_TYPE_DISCONNECTED;
			e.u.disconnected.client_index = p_event.u.disconnected.client_index;
			s_server_event_push(server, &e);
			cn_transport_destroy(server->client_transports[e.u.disconnected.client_index]);
			cn_transport_config_t transport_config = cn_transport_config_defaults();
			transport_config.index = e.u.disconnected.client_index;
			transport_config.send_packet_fn = s_send_packet_fn;
			transport_config.udata = server;
			transport_config.user_allocator_context = server->mem_ctx;
			server->client_transports[e.u.disconnected.client_index] = cn_transport_create(transport_config);
		}	break;

		// Protocol packets are processed by the reliability transport layer before they
		// are converted into user-facing server events.
		case CN_PROTOCOL_SERVER_EVENT_PAYLOAD_PACKET:
		{
			int index = p_event.u.payload_packet.client_index;
			void* data = p_event.u.payload_packet.data;
			int size = p_event.u.payload_packet.size;
			cn_transport_process_packet(server->client_transports[index], data, size);
			cn_protocol_server_free_packet(server->p_server, data);
		}	break;
		}
	}

	// Update all client reliability transports.
	for (int i = 0; i < CN_SERVER_MAX_CLIENTS; ++i) {
		if (cn_protocol_server_is_client_connected(server->p_server, i)) {
			cn_transport_update(server->client_transports[i], dt);
		}
	}

	// Look for any packets to receive from the reliability layer.
	// Convert these into server payload events.
	for (int i = 0; i < CN_SERVER_MAX_CLIENTS; ++i) {
		if (cn_protocol_server_is_client_connected(server->p_server, i)) {
			void* data;
			int size;
			while (!cn_is_error(cn_transport_receive_reliably_and_in_order(server->client_transports[i], &data, &size))) {
				cn_server_event_t e;
				e.type = CN_SERVER_EVENT_TYPE_PAYLOAD_PACKET;
				e.u.payload_packet.client_index = i;
				e.u.payload_packet.data = data;
				e.u.payload_packet.size = size;
				s_server_event_push(server, &e);
			}
			while (!cn_is_error(cn_transport_receive_fire_and_forget(server->client_transports[i], &data, &size))) {
				cn_server_event_t e;
				e.type = CN_SERVER_EVENT_TYPE_PAYLOAD_PACKET;
				e.u.payload_packet.client_index = i;
				e.u.payload_packet.data = data;
				e.u.payload_packet.size = size;
				s_server_event_push(server, &e);
			}
		}
	}
}

bool cn_server_pop_event(cn_server_t* server, cn_server_event_t* event)
{
	return s_server_event_pull(server, event) ? false : true;
}

void cn_server_free_packet(cn_server_t* server, int client_index, void* data)
{
	CN_ASSERT(client_index >= 0 && client_index < CN_SERVER_MAX_CLIENTS);
	CN_ASSERT(cn_protocol_server_is_client_connected(server->p_server, client_index));
	cn_transport_free_packet(server->client_transports[client_index], data);
}

void cn_server_disconnect_client(cn_server_t* server, int client_index, bool notify_client)
{
	CN_ASSERT(client_index >= 0 && client_index < CN_SERVER_MAX_CLIENTS);
	CN_ASSERT(cn_protocol_server_is_client_connected(server->p_server, client_index));
	cn_protocol_server_disconnect_client(server->p_server, client_index, notify_client);
}

void cn_server_send(cn_server_t* server, const void* packet, int size, int client_index, bool send_reliably)
{
	CN_ASSERT(client_index >= 0 && client_index < CN_SERVER_MAX_CLIENTS);
	CN_ASSERT(cn_protocol_server_is_client_connected(server->p_server, client_index));
	cn_transport_send(server->client_transports[client_index], packet, size, send_reliably);
}

bool cn_server_is_client_connected(cn_server_t* server, int client_index)
{
	return cn_protocol_server_is_client_connected(server->p_server, client_index);
}

void cn_server_send_to_all_clients(cn_server_t* server, const void* packet, int size, bool send_reliably)
{
	for (int i = 0; i < CN_SERVER_MAX_CLIENTS; ++i) {
		if (cn_server_is_client_connected(server, i)) {
			cn_server_send(server, packet, size, i, send_reliably);
		}
	}
}

void cn_server_send_to_all_but_one_client(cn_server_t* server, const void* packet, int size, int client_index, bool send_reliably)
{
	CN_ASSERT(client_index >= 0 && client_index < CN_SERVER_MAX_CLIENTS);
	CN_ASSERT(cn_protocol_server_is_client_connected(server->p_server, client_index));

	for (int i = 0; i < CN_SERVER_MAX_CLIENTS; ++i) {
		if (i == client_index) continue;
		if (cn_server_is_client_connected(server, i)) {
			cn_server_send(server, packet, size, i, send_reliably);
		}
	}
}

void cn_server_enable_network_simulator(cn_server_t* server, double latency, double jitter, double drop_chance, double duplicate_chance)
{
	cn_protocol_server_enable_network_simulator(server->p_server, latency, jitter, drop_chance, duplicate_chance);
}

// -------------------------------------------------------------------------------------------------
// TEST HARNESS
#ifdef CUTE_NET_TESTS
#ifndef CUTE_NET_TESTS_ONCE
#define CUTE_NET_TESTS_ONCE

typedef int (cn_test_fn)();

typedef struct cn_test_t
{
	const char* test_name;
	const char* description;
	cn_test_fn* fn_ptr;
} cn_test_t;

#ifndef CN_TEST_IO_STREAM
#	include <stdio.h>
#	define CN_TEST_IO_STREAM stderr
#endif

#ifdef _MSC_VER
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>

// At the time of writing, this define requires fairly recent windows version, so it's
// safest to just define it ourselves... Should be harmless!
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#	define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

// https://docs.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences
void windows_turn_on_console_color()
{
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD flags = 0;
	GetConsoleMode(h, &flags);
	flags |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
	SetConsoleMode(h, flags);
}
#endif

int cn_do_test(cn_test_t* test, int i)
{
	const char* test_name = test->test_name;
	const char* description = test->description;
	CN_FPRINTF(CN_TEST_IO_STREAM, "Running test #%d\n\tName:         %s\n\tDescription:  %s\n\t", i, test_name, description);
	int result = test->fn_ptr();
	const char* result_string = result ? "\033[31mFAILED\033[0m\n\n" : "\033[32mPASSED\033[0m\n\n";
	CN_FPRINTF(CN_TEST_IO_STREAM, "Result:       %s", result_string);

#ifdef _MSC_VER
	_CrtDumpMemoryLeaks();
#endif

	return result;
}

#define CN_TEST_PRINT_FILE_LINE(s) do { CN_FPRINTF(CN_TEST_IO_STREAM, "Extra info:   %s\n\tLine number:  %d\n\tFile:         %s\n\t", s, __LINE__, __FILE__); } while (0)
#define CN_TEST_ASSERT(x) do { if (!(x)) { CN_TEST_PRINT_FILE_LINE("Assertion was false."); return -1; } } while (0)
#define CN_TEST_CHECK(x) do { if (x) { CN_TEST_PRINT_FILE_LINE("Return code failed check."); return -1; } } while (0)
#define CN_TEST_CHECK_POINTER(x) do { if (!(x)) { CN_TEST_PRINT_FILE_LINE("Pointer failed check."); return -1; } } while (0)

#define CN_TEST_CASE(function, description) int function(); cn_test_t cn_test_##function = { #function, description, function }
#define CN_TEST_CASE_ENTRY(function) cn_test_##function

#define CN_STATIC_ASSERT(cond, message) typedef char cn_static_assert_##message[(cond) ? 1 : -1]

// -------------------------------------------------------------------------------------------------
// UNIT TESTS

void cn_static_asserts()
{
	CN_STATIC_ASSERT(sizeof(cn_crypto_key_t) == 32, cute_protocol_standard_calls_for_encryption_keys_to_be_32_bytes);
	CN_STATIC_ASSERT(CN_PROTOCOL_VERSION_STRING_LEN == 10, cute_protocol_standard_calls_for_the_version_string_to_be_10_bytes);
	CN_STATIC_ASSERT(CN_PROTOCOL_CONNECT_TOKEN_PACKET_SIZE == 1024, cute_protocol_standard_calls_for_connect_token_packet_to_be_exactly_1024_bytes);
	CN_STATIC_ASSERT(CN_PROTOCOL_SIGNATURE_SIZE == sizeof(cn_crypto_signature_t), must_be_equal);

	CN_STATIC_ASSERT(CN_CRYPTO_HEADER_BYTES == (int)hydro_secretbox_HEADERBYTES, must_be_equal);
	CN_STATIC_ASSERT(sizeof(cn_crypto_signature_t) == hydro_sign_BYTES, must_be_equal);
	CN_STATIC_ASSERT(sizeof(uint64_t) == sizeof(long long unsigned int), must_be_equal);

	CN_STATIC_ASSERT(CN_TRANSPORT_PACKET_PAYLOAD_MAX < 1207, cute_rotocol_max_payload_is_1207);
	CN_STATIC_ASSERT(CN_ACK_SYSTEM_MAX_PACKET_SIZE + CN_TRANSPORT_HEADER_SIZE < CN_TRANSPORT_PACKET_PAYLOAD_MAX, must_fit_within_cute_protocols_payload_limit);

	CN_STATIC_ASSERT(CN_SERVER_MAX_CLIENTS == CN_PROTOCOL_SERVER_MAX_CLIENTS, must_be_equal_for_a_simple_implementation);
	CN_STATIC_ASSERT(CN_CONNECT_TOKEN_SIZE == CN_PROTOCOL_CONNECT_TOKEN_SIZE, must_be_equal);
	CN_STATIC_ASSERT(CN_CONNECT_TOKEN_USER_DATA_SIZE == CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE, must_be_equal);

	CN_STATIC_ASSERT(sizeof(s_crypto_ctx) >= hydro_secretbox_CONTEXTBYTES, must_be_equal);
}

CN_TEST_CASE(cn_socket_init_send_recieve_shutdown, "Test sending one packet on an ipv4 socket, and then retrieve it.");
int cn_socket_init_send_recieve_shutdown()
{
	cn_socket_t socket;
	CN_TEST_CHECK(cn_socket_init2(&socket, "127.0.0.1:5000", CN_MB, CN_MB));

	const char* message_string = "The message.";
	int message_length = (int)CN_STRLEN(message_string) + 1;
	uint8_t* message_buffer = (uint8_t*)malloc(sizeof(uint8_t) * message_length);
	CN_MEMCPY(message_buffer, message_string, message_length);

	int bytes_sent = cn_socket_send_internal(&socket, socket.endpoint, message_buffer, message_length);
	CN_TEST_ASSERT(bytes_sent == message_length);
	CN_MEMSET(message_buffer, 0, message_length);
	CN_TEST_ASSERT(CN_MEMCMP(message_buffer, message_string, message_length));
	// cn_sleep(1); // TODO - Might need this on some platforms?

	cn_endpoint_t from;
	int bytes_recieved = cn_socket_receive(&socket, &from, message_buffer, message_length);
	CN_TEST_ASSERT(bytes_recieved == message_length);
	CN_TEST_ASSERT(cn_endpoint_equals(socket.endpoint, from));
	CN_TEST_ASSERT(!CN_MEMCMP(message_buffer, message_string, message_length));

	free(message_buffer);

	cn_socket_cleanup(&socket);

	return 0;
}

CN_TEST_CASE(cn_sequence_buffer_basic, "Create sequence buffer, insert a few entries, find them, remove them.");
int cn_sequence_buffer_basic()
{
	cn_sequence_buffer_t sequence_buffer;
	cn_sequence_buffer_t* buffer = &sequence_buffer;
	CN_TEST_CHECK(cn_sequence_buffer_init(buffer, 256, sizeof(int), NULL, NULL));

	int entries[3];
	int count = sizeof(entries) / sizeof(entries[0]);
	for (int i = 0; i < count; ++i)
	{
		int* entry = (int*)cn_sequence_buffer_insert(buffer, i, NULL);
		CN_TEST_CHECK_POINTER(entry);
		*entry = entries[i] = i;
	}

	for (int i = 0; i < count; ++i)
	{
		int* entry = (int*)cn_sequence_buffer_find(buffer, i);
		CN_TEST_CHECK_POINTER(entry);
		CN_TEST_ASSERT(*entry == entries[i]);
	}

	for (int i = 0; i < count; ++i)
	{
		cn_sequence_buffer_remove(buffer, i, NULL);
		int* entry = (int*)cn_sequence_buffer_find(buffer, i);
		CN_TEST_ASSERT(entry == NULL);
	}

	cn_sequence_buffer_cleanup(buffer, NULL);

	return 0;
}

CN_TEST_CASE(cn_replay_buffer_valid_packets, "Typical use-case example, should pass all sequences.");
int cn_replay_buffer_valid_packets()
{
	cn_protocol_replay_buffer_t buffer;
	cn_protocol_replay_buffer_init(&buffer);

	CN_TEST_ASSERT(buffer.max == 0);

	for (int i = 0; i < CN_PROTOCOL_REPLAY_BUFFER_SIZE; ++i)
	{
		uint64_t sequence = buffer.entries[i];
		CN_TEST_ASSERT(sequence == ~0ULL);
	}

	for (int i = 0; i < CN_PROTOCOL_REPLAY_BUFFER_SIZE; ++i)
	{
		CN_TEST_CHECK(cn_protocol_replay_buffer_cull_duplicate(&buffer, (uint64_t)i));
		cn_protocol_replay_buffer_update(&buffer, (uint64_t)i);
	}

	return 0;
}

CN_TEST_CASE(cn_replay_buffer_old_packet_out_of_range, "Replay buffer should cull packets of sequence older than `CN_REPLAY_BUFFER_SIZE`.");
int cn_replay_buffer_old_packet_out_of_range()
{
	cn_protocol_replay_buffer_t buffer;
	cn_protocol_replay_buffer_init(&buffer);

	for (int i = 0; i < CN_PROTOCOL_REPLAY_BUFFER_SIZE * 2; ++i)
	{
		CN_TEST_CHECK(cn_protocol_replay_buffer_cull_duplicate(&buffer, (uint64_t)i));
		cn_protocol_replay_buffer_update(&buffer, (uint64_t)i);
	}

	CN_TEST_CHECK(!cn_protocol_replay_buffer_cull_duplicate(&buffer, 0));

	return 0;
}

CN_TEST_CASE(cn_replay_buffer_duplicate, "Pass in some valid nonces, and then assert the duplicate fails.");
int cn_replay_buffer_duplicate()
{
	cn_protocol_replay_buffer_t buffer;
	cn_protocol_replay_buffer_init(&buffer);

	for (int i = 0; i < CN_PROTOCOL_REPLAY_BUFFER_SIZE; ++i)
	{
		CN_TEST_CHECK(cn_protocol_replay_buffer_cull_duplicate(&buffer, (uint64_t)i));
		cn_protocol_replay_buffer_update(&buffer, (uint64_t)i);
	}

	CN_TEST_CHECK(!cn_protocol_replay_buffer_cull_duplicate(&buffer, 100));

	return 0;
}

CN_TEST_CASE(cn_hash_table_basic, "Create table, insert some elements, remove them, free table.");
int cn_hash_table_basic()
{
	cn_hashtable_t table;
	cn_hashtable_init(&table, 8, 8, 20, NULL);

	uint64_t key = 5;
	uint64_t item = 10;

	cn_hashtable_insert(&table, &key, &item);

	void* item_ptr = cn_hashtable_find(&table, &key);
	CN_TEST_CHECK_POINTER(item_ptr);
	CN_TEST_ASSERT(*(uint64_t*)item_ptr == item);

	cn_hashtable_cleanup(&table);

	return 0;
}

CN_TEST_CASE(cn_hash_table_set, "Make sure the table also operates as a set (zero size value).");
int cn_hash_table_set()
{
	cn_hashtable_t table;
	cn_hashtable_init(&table, 8, 0, 20, NULL);

	uint64_t key = 5;

	cn_hashtable_insert(&table, &key, NULL);

	void* item_ptr = cn_hashtable_find(&table, &key);
	CN_TEST_CHECK_POINTER(item_ptr);

	cn_hashtable_cleanup(&table);

	return 0;
}

CN_TEST_CASE(cn_hash_table_hammer, "Fill up table, many lookups, and reset, all a few times.");
int cn_hash_table_hammer()
{
	cn_hashtable_t table;
	cn_hashtable_init(&table, 8, 8, 128, NULL);

	uint64_t keys[128];
	uint64_t items[128];

	for (int i = 0; i < 128; ++i)
	{
		keys[i] = i;
		items[i] = i * 2;
	}

	for (int iters = 0; iters < 10; ++iters)
	{
		for (int i = 0; i < 128; ++i)
			cn_hashtable_insert(&table, keys + i, items + i);

		for (int i = 0; i < 128; ++i)
		{
			void* item_ptr = cn_hashtable_find(&table, keys + i);
			CN_TEST_CHECK_POINTER(item_ptr);
			CN_TEST_ASSERT(*(uint64_t*)item_ptr == items[i]);
		}

		for (int i = 0; i < 128; ++i)
			cn_hashtable_remove(&table, keys + i);
	}

	cn_hashtable_cleanup(&table);

	return 0;
}


CN_TEST_CASE(cn_handle_basic, "Typical use-case example, alloc and free some handles.");
int cn_handle_basic()
{
	cn_handle_allocator_t* table = cn_handle_allocator_create(1024, NULL);
	CN_TEST_CHECK_POINTER(table);

	cn_handle_t h0 = cn_handle_allocator_alloc(table, 7);
	cn_handle_t h1 = cn_handle_allocator_alloc(table, 13);
	CN_TEST_ASSERT(h0 != CN_INVALID_HANDLE);
	CN_TEST_ASSERT(h1 != CN_INVALID_HANDLE);
	uint32_t index0 = cn_handle_allocator_get_index(table, h0);
	uint32_t index1 = cn_handle_allocator_get_index(table, h1);
	CN_TEST_ASSERT(index0 == 7);
	CN_TEST_ASSERT(index1 == 13);

	cn_handle_allocator_free(table, h0);
	cn_handle_allocator_free(table, h1);

	h0 = cn_handle_allocator_alloc(table, 4);
	h1 = cn_handle_allocator_alloc(table, 267);
	CN_TEST_ASSERT(h0 != CN_INVALID_HANDLE);
	CN_TEST_ASSERT(h1 != CN_INVALID_HANDLE);
	index0 = cn_handle_allocator_get_index(table, h0);
	index1 = cn_handle_allocator_get_index(table, h1);
	CN_TEST_ASSERT(index0 == 4);
	CN_TEST_ASSERT(index1 == 267);

	cn_handle_allocator_update_index(table, h1, 9);
	index1 = cn_handle_allocator_get_index(table, h1);
	CN_TEST_ASSERT(index1 == 9);

	cn_handle_allocator_destroy(table);

	return 0;
}

CN_TEST_CASE(cn_handle_large_loop, "Allocate right up the maximum size possible for the table.");
int cn_handle_large_loop()
{
	cn_handle_allocator_t* table = cn_handle_allocator_create(1024, NULL);
	CN_TEST_CHECK_POINTER(table);

	for (int i = 0; i < 1024; ++i)
	{
		cn_handle_t h = cn_handle_allocator_alloc(table, i);
		CN_TEST_ASSERT(h != CN_INVALID_HANDLE);
		CN_ASSERT(cn_handle_allocator_get_index(table, h) == (uint32_t)i);
	}

	cn_handle_allocator_destroy(table);

	return 0;
}

CN_TEST_CASE(cn_handle_large_loop_and_free, "\"Soak test\" to fill up the handle buffer and empty it a few times.");
int cn_handle_large_loop_and_free()
{
	cn_handle_allocator_t* table = cn_handle_allocator_create(1024, NULL);
	CN_TEST_CHECK_POINTER(table);
	cn_handle_t* handles = (cn_handle_t*)malloc(sizeof(cn_handle_t) * 2014);

	for (int iters = 0; iters < 5; ++iters)
	{
		for (int i = 0; i < 1024; ++i)
		{
			cn_handle_t h = cn_handle_allocator_alloc(table, i);
			CN_TEST_ASSERT(h != CN_INVALID_HANDLE);
			CN_ASSERT(cn_handle_allocator_get_index(table, h) == (uint32_t)i);
			handles[i] = h;
		}

		for (int i = 0; i < 1024; ++i)
		{
			cn_handle_t h = handles[i];
			cn_handle_allocator_free(table, h);
		}
	}

	cn_handle_allocator_destroy(table);
	free(handles);

	return 0;
}

CN_TEST_CASE(cn_handle_alloc_too_many, "Allocating over 1024 entries should not result in failure.");
int cn_handle_alloc_too_many()
{
	cn_handle_allocator_t* table = cn_handle_allocator_create(1024, NULL);
	CN_TEST_CHECK_POINTER(table);

	for (int i = 0; i < 1024; ++i)
	{
		cn_handle_t h = cn_handle_allocator_alloc(table, i);
		CN_TEST_ASSERT(h != CN_INVALID_HANDLE);
		CN_ASSERT(cn_handle_allocator_get_index(table, h) == (uint32_t)i);
	}

	cn_handle_t h = cn_handle_allocator_alloc(table, 0);
	CN_TEST_ASSERT(h != CN_INVALID_HANDLE);

	cn_handle_allocator_destroy(table);

	return 0;
}

CN_TEST_CASE(cn_encryption_map_basic, "Create map, make entry, lookup entry, remove, and cleanup.");
int cn_encryption_map_basic()
{
	cn_protocol_encryption_map_t map;
	cn_protocol_encryption_map_init(&map, NULL);

	cn_protocol_encryption_state_t state;
	state.sequence = 0;
	state.expiration_timestamp = 10;
	state.handshake_timeout = 5;
	state.last_packet_recieved_time = 0;
	state.last_packet_sent_time = 0;
	state.client_to_server_key = cn_crypto_generate_key();
	state.server_to_client_key = cn_crypto_generate_key();
	state.client_id = 0;

	cn_endpoint_t endpoint;
	CN_TEST_CHECK(cn_endpoint_init(&endpoint, "[::]:5000"));

	cn_protocol_encryption_map_insert(&map, endpoint, &state);

	cn_protocol_encryption_state_t* state_looked_up = cn_protocol_encryption_map_find(&map, endpoint);
	CN_TEST_CHECK_POINTER(state_looked_up);

	CN_TEST_ASSERT(!CN_MEMCMP(&state, state_looked_up, sizeof(state)));

	cn_protocol_encryption_map_cleanup(&map);

	return 0;
}

CN_TEST_CASE(cn_encryption_map_timeout_and_expiration, "Ensure timeouts and expirations remove entries properly.");
int cn_encryption_map_timeout_and_expiration()
{
	cn_protocol_encryption_map_t map;
	cn_protocol_encryption_map_init(&map, NULL);

	cn_protocol_encryption_state_t state0;
	state0.sequence = 0;
	state0.expiration_timestamp = 10;
	state0.handshake_timeout = 5;
	state0.last_packet_recieved_time = 0;
	state0.last_packet_sent_time = 0;
	state0.client_to_server_key = cn_crypto_generate_key();
	state0.server_to_client_key = cn_crypto_generate_key();
	state0.client_id = 0;

	cn_protocol_encryption_state_t state1;
	state1.sequence = 0;
	state1.expiration_timestamp = 10;
	state1.handshake_timeout = 6;
	state1.last_packet_recieved_time = 0;
	state1.last_packet_sent_time = 0;
	state1.client_to_server_key = cn_crypto_generate_key();
	state1.server_to_client_key = cn_crypto_generate_key();
	state1.client_id = 0;

	cn_endpoint_t endpoint0;
	CN_TEST_CHECK(cn_endpoint_init(&endpoint0, "[::]:5000"));

	cn_endpoint_t endpoint1;
	CN_TEST_CHECK(cn_endpoint_init(&endpoint1, "[::]:5001"));

	cn_protocol_encryption_map_insert(&map, endpoint0, &state0);
	cn_protocol_encryption_map_insert(&map, endpoint1, &state1);

	cn_protocol_encryption_state_t* state_looked_up = cn_protocol_encryption_map_find(&map, endpoint0);
	CN_TEST_CHECK_POINTER(state_looked_up);
	CN_TEST_ASSERT(!CN_MEMCMP(&state0, state_looked_up, sizeof(state0)));

	state_looked_up = cn_protocol_encryption_map_find(&map, endpoint1);
	CN_TEST_CHECK_POINTER(state_looked_up);
	CN_TEST_ASSERT(!CN_MEMCMP(&state1, state_looked_up, sizeof(state1)));

	// Nothing should timeout or expire just yet.
	cn_protocol_encryption_map_look_for_timeouts_or_expirations(&map, 4.0f, 9ULL);

	state_looked_up = cn_protocol_encryption_map_find(&map, endpoint0);
	CN_TEST_CHECK_POINTER(state_looked_up);
	CN_TEST_ASSERT(!CN_MEMCMP(&state0, state_looked_up, sizeof(state0)));

	state_looked_up = cn_protocol_encryption_map_find(&map, endpoint1);
	CN_TEST_CHECK_POINTER(state_looked_up);
	CN_TEST_ASSERT(!CN_MEMCMP(&state1, state_looked_up, sizeof(state1)));

	// Now timeout state0.
	cn_protocol_encryption_map_look_for_timeouts_or_expirations(&map, 6.0f, 9ULL);
	CN_TEST_CHECK_POINTER(!cn_protocol_encryption_map_find(&map, endpoint0));

	// Now expire state1.
	cn_protocol_encryption_map_look_for_timeouts_or_expirations(&map, 0, 10ULL);
	CN_TEST_CHECK_POINTER(!cn_protocol_encryption_map_find(&map, endpoint1));

	// Assert that there are no present entries.
	CN_TEST_ASSERT(cn_protocol_encryption_map_count(&map) == 0);

	cn_protocol_encryption_map_cleanup(&map);

	return 0;
}

CN_TEST_CASE(cn_doubly_list, "Make list of three elements, perform all operations on it, assert correctness.");
int cn_doubly_list()
{
	cn_list_t list;

	cn_list_node_t a;
	cn_list_node_t b;
	cn_list_node_t c;

	cn_list_init(&list);
	cn_list_init_node(&a);
	cn_list_init_node(&b);
	cn_list_init_node(&c);

	CN_TEST_ASSERT(list.nodes.next == &list.nodes);
	CN_TEST_ASSERT(list.nodes.prev == &list.nodes);
	CN_TEST_ASSERT(a.next == &a);
	CN_TEST_ASSERT(a.prev == &a);
	CN_TEST_ASSERT(cn_list_empty(&list));

	cn_list_push_front(&list, &a);
	CN_TEST_ASSERT(!cn_list_empty(&list));
	CN_TEST_ASSERT(list.nodes.next == &a);
	CN_TEST_ASSERT(list.nodes.prev == &a);
	CN_TEST_ASSERT(list.nodes.next->next == &list.nodes);
	CN_TEST_ASSERT(list.nodes.prev->prev == &list.nodes);
	CN_TEST_ASSERT(cn_list_front(&list) == &a);
	CN_TEST_ASSERT(cn_list_back(&list) == &a);

	cn_list_push_front(&list, &b);
	CN_TEST_ASSERT(cn_list_front(&list) == &b);
	CN_TEST_ASSERT(cn_list_back(&list) == &a);

	cn_list_push_back(&list, &c);
	CN_TEST_ASSERT(cn_list_front(&list) == &b);
	CN_TEST_ASSERT(cn_list_back(&list) == &c);

	cn_list_node_t* nodes[3] = { &b, &a, &c };
	int index = 0;
	for (cn_list_node_t* n = cn_list_begin(&list); n != cn_list_end(&list); n = n->next)
	{
		CN_TEST_ASSERT(n == nodes[index++]);
	}

	CN_TEST_ASSERT(cn_list_pop_front(&list) == &b);
	CN_TEST_ASSERT(cn_list_pop_back(&list) == &c);
	CN_TEST_ASSERT(cn_list_pop_back(&list) == &a);

	CN_TEST_ASSERT(cn_list_empty(&list));

	return 0;
}

CN_TEST_CASE(cn_crypto_encrypt_decrypt, "Generate key, encrypt a message, decrypt the message.");
int cn_crypto_encrypt_decrypt()
{
	cn_crypto_key_t k = cn_crypto_generate_key();

	const char* message_string = "The message.";
	int message_length = (int)CN_STRLEN(message_string) + 1;
	uint8_t* message_buffer = (uint8_t*)malloc(sizeof(uint8_t) * (message_length + CN_CRYPTO_HEADER_BYTES));
	CN_MEMCPY(message_buffer, message_string, message_length);

	cn_crypto_encrypt(&k, message_buffer, message_length, 0);
	CN_TEST_ASSERT(CN_MEMCMP(message_buffer, message_string, message_length));
	CN_TEST_ASSERT(!cn_is_error(cn_crypto_decrypt(&k, message_buffer, message_length + CN_CRYPTO_HEADER_BYTES, 0)));
	CN_TEST_ASSERT(!CN_MEMCMP(message_buffer, message_string, message_length));

	free(message_buffer);

	return 0;
}

CN_TEST_CASE(cn_connect_token_cache, "Add tokens, overflow (eject oldest), ensure LRU correctness.");
int cn_connect_token_cache()
{
	int capacity = 3;
	cn_protocol_connect_token_cache_t cache;
	cn_protocol_connect_token_cache_init(&cache, capacity, NULL);

	cn_endpoint_t endpoint;
	CN_TEST_CHECK(cn_endpoint_init(&endpoint, "[::]:5000"));

	uint8_t hmac_bytes_a[sizeof(cn_crypto_signature_t)];
	uint8_t hmac_bytes_b[sizeof(cn_crypto_signature_t)];
	uint8_t hmac_bytes_c[sizeof(cn_crypto_signature_t)];
	uint8_t hmac_bytes_d[sizeof(cn_crypto_signature_t)];
	uint8_t hmac_bytes_e[sizeof(cn_crypto_signature_t)];
	cn_crypto_random_bytes(hmac_bytes_a, sizeof(hmac_bytes_a));
	cn_crypto_random_bytes(hmac_bytes_b, sizeof(hmac_bytes_b));
	cn_crypto_random_bytes(hmac_bytes_c, sizeof(hmac_bytes_c));
	cn_crypto_random_bytes(hmac_bytes_d, sizeof(hmac_bytes_d));
	cn_crypto_random_bytes(hmac_bytes_e, sizeof(hmac_bytes_e));

	cn_protocol_connect_token_cache_add(&cache, hmac_bytes_a);
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_a));
	cn_protocol_connect_token_cache_add(&cache, hmac_bytes_b);
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_b));
	cn_protocol_connect_token_cache_add(&cache, hmac_bytes_c);
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_c));
	cn_protocol_connect_token_cache_add(&cache, hmac_bytes_d);
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_d));

	CN_TEST_ASSERT(!cn_protocol_connect_token_cache_find(&cache, hmac_bytes_a));
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_b));
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_c));
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_d));

	cn_protocol_connect_token_cache_add(&cache, hmac_bytes_e);
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_e));

	CN_TEST_ASSERT(!cn_protocol_connect_token_cache_find(&cache, hmac_bytes_a));
	CN_TEST_ASSERT(!cn_protocol_connect_token_cache_find(&cache, hmac_bytes_b));
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_c));
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_d));
	CN_TEST_ASSERT(cn_protocol_connect_token_cache_find(&cache, hmac_bytes_e));

	cn_protocol_connect_token_cache_cleanup(&cache);

	return 0;
}

CN_TEST_CASE(cn_test_generate_connect_token, "Basic test to generate a connect token and assert the expected token.");
int cn_test_generate_connect_token()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
		"[::1]:5001",
		"[::1]:5002"
	};

	uint64_t application_id = ~0ULL;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 10;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t token_buffer[CN_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		3,
		endpoints,
		client_id,
		user_data,
		&sk,
		token_buffer
	)));

	// Assert reading token from web service as a client.
	cn_protocol_connect_token_t token;
	uint8_t* connect_token_packet = cn_protocol_client_read_connect_token_from_web_service(token_buffer, application_id, current_timestamp, &token);
	CN_TEST_CHECK_POINTER(connect_token_packet);

	CN_TEST_ASSERT(token.creation_timestamp == current_timestamp);
	CN_TEST_ASSERT(!CN_MEMCMP(&client_to_server_key, &token.client_to_server_key, sizeof(cn_crypto_key_t)));
	CN_TEST_ASSERT(!CN_MEMCMP(&server_to_client_key, &token.server_to_client_key, sizeof(cn_crypto_key_t)));
	CN_TEST_ASSERT(token.expiration_timestamp == expiration_timestamp);
	CN_TEST_ASSERT(token.handshake_timeout == handshake_timeout);
	CN_TEST_ASSERT(token.endpoint_count == 3);
	for (int i = 0; i < token.endpoint_count; ++i)
	{
		cn_endpoint_t endpoint;
		CN_TEST_CHECK(cn_endpoint_init(&endpoint, endpoints[i]));
		CN_TEST_ASSERT(cn_endpoint_equals(token.endpoints[i], endpoint));
	}

	// Assert reading *connect token packet* as server, and decrypting it successfully.
	cn_protocol_connect_token_decrypted_t decrypted_token;
	CN_TEST_CHECK(cn_is_error(cn_protocol_server_decrypt_connect_token_packet(connect_token_packet, &pk, &sk, application_id, current_timestamp, &decrypted_token)));
	CN_TEST_ASSERT(decrypted_token.expiration_timestamp == expiration_timestamp);
	CN_TEST_ASSERT(decrypted_token.handshake_timeout == handshake_timeout);
	CN_TEST_ASSERT(decrypted_token.endpoint_count == 3);
	for (int i = 0; i < token.endpoint_count; ++i)
	{
		cn_endpoint_t endpoint;
		CN_TEST_CHECK(cn_endpoint_init(&endpoint, endpoints[i]));
		CN_TEST_ASSERT(cn_endpoint_equals(decrypted_token.endpoints[i], endpoint));
	}
	CN_TEST_ASSERT(decrypted_token.client_id == client_id);
	CN_TEST_ASSERT(!CN_MEMCMP(&client_to_server_key, &decrypted_token.client_to_server_key, sizeof(cn_crypto_key_t)));
	CN_TEST_ASSERT(!CN_MEMCMP(&server_to_client_key, &decrypted_token.server_to_client_key, sizeof(cn_crypto_key_t)));

	return 0;
}

CN_TEST_CASE(cn_client_server, "Connect a client to server, then disconnect and shutdown both.");
int cn_client_server()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	uint64_t application_id = 333;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;
	const char* endpoints[] = {
		"[::1]:5000",
	};
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_server_config_t config = cn_server_config_defaults();
	config.public_key = pk;
	config.secret_key = sk;
	config.application_id = application_id;
	cn_server_t* server = cn_server_create(config);
	cn_client_t* client = cn_client_create(0, application_id, true, NULL);
	CN_TEST_ASSERT(server);
	CN_TEST_ASSERT(client);

	CN_TEST_CHECK(cn_is_error(cn_server_start(server, "[::1]:5000")));
	CN_TEST_CHECK(cn_is_error(cn_client_connect(client, connect_token)));

	int iters = 0;
	while (1) {
		cn_client_update(client, 0, 0);
		cn_server_update(server, 0, 0);

		if (cn_client_state_get(client) < 0 || ++iters == 100) {
			CN_TEST_ASSERT(false);
			break;
		}

		if (cn_client_state_get(client) == CN_CLIENT_STATE_CONNECTED) {
			break;
		}
	}
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_server_is_client_connected(server, 0));

	cn_client_disconnect(client);
	cn_server_update(server, 0, 0);
	CN_TEST_ASSERT(!cn_server_is_client_connected(server, 0));

	cn_client_destroy(client);
	cn_server_stop(server);
	cn_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_client_server_payload, "Connect a client to server, send some packets, then disconnect and shutdown both.");
int cn_client_server_payload()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	uint64_t application_id = 333;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;
	const char* endpoints[] = {
		"[::1]:5000",
	};
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_server_config_t config = cn_server_config_defaults();
	config.public_key = pk;
	config.secret_key = sk;
	config.application_id = application_id;
	cn_server_t* server = cn_server_create(config);
	cn_client_t* client = cn_client_create(0, application_id, true, NULL);
	CN_TEST_ASSERT(server);
	CN_TEST_ASSERT(client);

	CN_TEST_CHECK(cn_is_error(cn_server_start(server, "[::1]:5000")));
	CN_TEST_CHECK(cn_is_error(cn_client_connect(client, connect_token)));

	int iters = 0;
	while (1) {
		cn_client_update(client, 0, 0);
		cn_server_update(server, 0, 0);

		if (cn_client_state_get(client) < 0 || ++iters == 100) {
			CN_TEST_ASSERT(false);
			break;
		}

		if (cn_client_state_get(client) == CN_CLIENT_STATE_CONNECTED) {
			break;
		}
	}
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_server_is_client_connected(server, 0));

	uint64_t packet = 12345678;
	CN_TEST_CHECK(cn_is_error(cn_client_send(client, &packet, sizeof(packet), false)));
	cn_client_update(client, 0, 0);
	cn_server_update(server, 0, 0);

	cn_server_event_t e;
	CN_TEST_ASSERT(cn_server_pop_event(server, &e));
	CN_TEST_ASSERT(e.type == CN_SERVER_EVENT_TYPE_NEW_CONNECTION);
	CN_TEST_ASSERT(e.u.new_connection.client_index == 0);
	CN_TEST_ASSERT(e.u.new_connection.client_id == client_id);

	CN_TEST_ASSERT(cn_server_pop_event(server, &e));
	CN_TEST_ASSERT(e.type == CN_SERVER_EVENT_TYPE_PAYLOAD_PACKET);
	CN_TEST_ASSERT(e.u.payload_packet.size == sizeof(packet));
	CN_TEST_ASSERT(*(uint64_t*)e.u.payload_packet.data == packet);
	cn_server_free_packet(server, e.u.payload_packet.client_index, e.u.payload_packet.data);

	cn_client_disconnect(client);
	cn_server_update(server, 0, 0);
	CN_TEST_ASSERT(!cn_server_is_client_connected(server, 0));
	CN_TEST_ASSERT(cn_server_pop_event(server, &e));
	CN_TEST_ASSERT(e.type == CN_SERVER_EVENT_TYPE_DISCONNECTED);
	CN_TEST_ASSERT(e.u.disconnected.client_index == 0);

	cn_client_destroy(client);
	cn_server_stop(server);
	cn_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_client_server_sim, "Run network simulator between a client and server.");
int cn_client_server_sim()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	uint64_t application_id = 333;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;
	const char* endpoints[] = {
		"[::1]:5000",
	};
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_server_config_t config = cn_server_config_defaults();
	config.public_key = pk;
	config.secret_key = sk;
	config.application_id = application_id;
	cn_server_t* server = cn_server_create(config);
	cn_client_t* client = cn_client_create(0, application_id, true, NULL);
	CN_TEST_ASSERT(server);
	CN_TEST_ASSERT(client);

	CN_TEST_CHECK(cn_is_error(cn_server_start(server, "[::1]:5000")));
	CN_TEST_CHECK(cn_is_error(cn_client_connect(client, connect_token)));

	int iters = 0;
	while (1) {
		cn_client_update(client, 0, 0);
		cn_server_update(server, 0, 0);

		if (cn_client_state_get(client) < 0 || ++iters == 100) {
			CN_TEST_ASSERT(false);
			break;
		}

		if (cn_client_state_get(client) == CN_CLIENT_STATE_CONNECTED) {
			break;
		}
	}
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_server_is_client_connected(server, 0));

	cn_server_event_t e;
	CN_TEST_ASSERT(cn_server_pop_event(server, &e));
	CN_TEST_ASSERT(e.type == CN_SERVER_EVENT_TYPE_NEW_CONNECTION);
	CN_TEST_ASSERT(e.u.new_connection.client_index == 0);
	CN_TEST_ASSERT(e.u.new_connection.client_id == client_id);

	cn_client_enable_network_simulator(client, 0.02f, 0.005f, 0.5, 0.05f);
	cn_server_enable_network_simulator(server, 0.02f, 0.005f, 0.5, 0.05f);

	bool soak = false;
	bool do_send = true;
	int packet_size = 1024 * 3;
	void* packet = CN_ALLOC(packet_size, NULL);
	double dt = 1.0/60.0;
	iters = 0;

	uint64_t keepalive = ~0ULL;

	while (1) {
		if (do_send) {
			cn_crypto_random_bytes(packet, packet_size);
			cn_error_t err = cn_client_send(client, packet, packet_size, true);
			CN_TEST_ASSERT(!cn_is_error(err));
			do_send = false;
		}

		cn_client_update(client, dt, 0);
		cn_server_update(server, dt, 0);
		cn_server_send(server, &keepalive, sizeof(keepalive), 0, false);

		void* client_packet;
		int client_packet_size;
		if (cn_client_pop_packet(client, &client_packet, &client_packet_size, NULL)) {
			CN_TEST_ASSERT(client_packet_size == sizeof(keepalive));
			CN_TEST_ASSERT(*(uint64_t*)client_packet == keepalive);
			cn_client_free_packet(client, client_packet);
		}

		cn_server_event_t e;
		if (cn_server_pop_event(server, &e)) {
			CN_TEST_ASSERT(e.type == CN_SERVER_EVENT_TYPE_PAYLOAD_PACKET);
			void* data = e.u.payload_packet.data;
			int size = e.u.payload_packet.size;
			CN_TEST_ASSERT(size == packet_size);
			CN_TEST_ASSERT(!CN_MEMCMP(data, packet, packet_size));
			do_send = true;
			++iters;
			cn_server_free_packet(server, 0, data);
		}

		if (!soak && iters == 3) {
			break;
		}
	}

	CN_FREE(packet, NULL);
	cn_client_destroy(client);
	cn_server_destroy(server);

	return 0;
}

typedef struct cn_test_transport_data_t
{
	int drop_packet;
	int id;
	cn_ack_system_t* ack_system_a;
	cn_ack_system_t* ack_system_b;
	cn_transport_t* transport_a;
	cn_transport_t* transport_b;
} cn_test_transport_data_t;

cn_test_transport_data_t cn_test_transport_data_defaults()
{
	cn_test_transport_data_t data;
	data.drop_packet = 0;
	data.id = ~0;
	data.ack_system_a = NULL;
	data.ack_system_b = NULL;
	data.transport_a = NULL;
	data.transport_b = NULL;
	return data;
}

cn_error_t cn_test_send_packet_fn(int index, void* packet, int size, void* udata)
{
	cn_test_transport_data_t* data = (cn_test_transport_data_t*)udata;
	if (data->drop_packet) {
		return cn_error_success();
	}

	if (data->id) {
		return cn_ack_system_receive_packet(data->ack_system_a, packet, size);
	} else {
		return cn_ack_system_receive_packet(data->ack_system_b, packet, size);
	}
}

CN_TEST_CASE(cn_ack_system_basic, "Create ack system, send a few packets, and receive them. Make sure some drop. Assert acks.");
int cn_ack_system_basic()
{
	cn_test_transport_data_t data_a = cn_test_transport_data_defaults();
	cn_test_transport_data_t data_b = cn_test_transport_data_defaults();
	data_a.id = 0;
	data_b.id = 1;

	cn_ack_system_config_t config = cn_ack_system_config_defaults();
	config.send_packet_fn = cn_test_send_packet_fn;
	config.udata = &data_a;
	cn_ack_system_t* ack_system_a = cn_ack_system_create(config);
	config.udata = &data_b;
	cn_ack_system_t* ack_system_b = cn_ack_system_create(config);
	data_a.ack_system_a = ack_system_a;
	data_a.ack_system_b = ack_system_b;
	data_b.ack_system_a = ack_system_a;
	data_b.ack_system_b = ack_system_b;

	CN_TEST_CHECK_POINTER(ack_system_a);
	CN_TEST_CHECK_POINTER(ack_system_b);

	uint64_t packet_data = 100;

	for (int i = 0; i < 10; ++i)
	{
		if ((i % 3) == 0) {
			data_a.drop_packet = 1;
			data_b.drop_packet = 1;
		} else {
			data_a.drop_packet = 0;
			data_b.drop_packet = 0;
		}
		uint16_t sequence_a, sequence_b;
		CN_TEST_CHECK(cn_is_error(cn_ack_system_send_packet(ack_system_a, &packet_data, sizeof(packet_data), &sequence_a)));
		CN_TEST_CHECK(cn_is_error(cn_ack_system_send_packet(ack_system_b, &packet_data, sizeof(packet_data), &sequence_b)));
	}

	uint64_t a_sent = cn_ack_system_get_counter(ack_system_a, CN_ACK_SYSTEM_COUNTERS_PACKETS_SENT);
	uint64_t b_sent = cn_ack_system_get_counter(ack_system_b, CN_ACK_SYSTEM_COUNTERS_PACKETS_SENT);
	CN_TEST_ASSERT(a_sent == b_sent);

	uint64_t a_received = cn_ack_system_get_counter(ack_system_a, CN_ACK_SYSTEM_COUNTERS_PACKETS_RECEIVED);
	uint64_t b_received = cn_ack_system_get_counter(ack_system_b, CN_ACK_SYSTEM_COUNTERS_PACKETS_RECEIVED);
	CN_TEST_ASSERT(a_received == b_received);

	CN_TEST_ASSERT(a_sent > a_received);
	CN_TEST_ASSERT(b_sent > b_received);

	uint16_t* acks_a = cn_ack_system_get_acks(ack_system_a);
	uint16_t* acks_b = cn_ack_system_get_acks(ack_system_b);
	int count_a = cn_ack_system_get_acks_count(ack_system_a);
	int count_b = cn_ack_system_get_acks_count(ack_system_b);
	CN_TEST_ASSERT(count_a - 1 == count_b);
	for (int i = 0; i < count_b; ++i)
	{
		CN_TEST_ASSERT(acks_a[i] == acks_b[i]);
		CN_TEST_ASSERT(acks_a[i] != 0);
		CN_TEST_ASSERT(acks_a[i] != 3);
		CN_TEST_ASSERT(acks_a[i] != 6);
		CN_TEST_ASSERT(acks_a[i] != 9);
	}

	cn_ack_system_destroy(ack_system_a);
	cn_ack_system_destroy(ack_system_b);

	return 0;
}

cn_error_t cn_test_transport_send_packet_fn(int index, void* packet, int size, void* udata)
{
	cn_test_transport_data_t* data = (cn_test_transport_data_t*)udata;
	if (data->drop_packet) {
		return cn_error_success();
	}
	
	if (data->id) {
		return cn_transport_process_packet(data->transport_a, packet, size);
	} else {
		return cn_transport_process_packet(data->transport_b, packet, size);
	}
}

cn_error_t cn_test_transport_open_packet_fn(int index, void* packet, int size, void* udata)
{
	cn_test_transport_data_t* data = (cn_test_transport_data_t*)udata;
	return cn_error_success();
}

CN_TEST_CASE(cn_transport_basic, "Create transport, send a couple packets, receive them.");
int cn_transport_basic()
{
	cn_test_transport_data_t data_a = cn_test_transport_data_defaults();
	cn_test_transport_data_t data_b = cn_test_transport_data_defaults();
	data_a.id = 0;
	data_b.id = 1;

	cn_transport_config_t config = cn_transport_config_defaults();
	config.send_packet_fn = cn_test_transport_send_packet_fn;
	config.udata = &data_a;
	cn_transport_t* transport_a = cn_transport_create(config);
	config.udata = &data_b;
	cn_transport_t* transport_b = cn_transport_create(config);
	data_a.transport_a = transport_a;
	data_a.transport_b = transport_b;
	data_b.transport_a = transport_a;
	data_b.transport_b = transport_b;
	double dt = 1.0/60.0;

	int packet_size = 4000;
	uint8_t* packet = (uint8_t*)CN_ALLOC(packet_size, NULL);
	CN_MEMSET(packet, 0xFF, packet_size);

	CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_a, packet, packet_size, true)));
	CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_b, packet, packet_size, true)));

	cn_transport_update(transport_a, dt);
	cn_transport_update(transport_b, dt);

	void* packet_received;
	int packet_received_size;

	CN_TEST_CHECK(cn_is_error(cn_transport_receive_reliably_and_in_order(transport_a, &packet_received, &packet_received_size)));
	CN_TEST_ASSERT(packet_size == packet_received_size);
	CN_TEST_ASSERT(!CN_MEMCMP(packet, packet_received, packet_size));
	cn_transport_free_packet(transport_a, packet_received);

	CN_TEST_CHECK(cn_is_error(cn_transport_receive_reliably_and_in_order(transport_b, &packet_received, &packet_received_size)));
	CN_TEST_ASSERT(packet_size == packet_received_size);
	CN_TEST_ASSERT(!CN_MEMCMP(packet, packet_received, packet_size));
	cn_transport_free_packet(transport_b, packet_received);

	CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_a, packet, packet_size, false)));
	CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_b, packet, packet_size, false)));

	cn_transport_update(transport_a, dt);
	cn_transport_update(transport_b, dt);

	CN_TEST_CHECK(cn_is_error(cn_transport_receive_fire_and_forget(transport_a, &packet_received, &packet_received_size)));
	CN_TEST_ASSERT(packet_size == packet_received_size);
	CN_TEST_ASSERT(!CN_MEMCMP(packet, packet_received, packet_size));
	cn_transport_free_packet(transport_a, packet_received);

	CN_TEST_CHECK(cn_is_error(cn_transport_receive_fire_and_forget(transport_b, &packet_received, &packet_received_size)));
	CN_TEST_ASSERT(packet_size == packet_received_size);
	CN_TEST_ASSERT(!CN_MEMCMP(packet, packet_received, packet_size));
	cn_transport_free_packet(transport_b, packet_received);

	CN_FREE(packet, NULL);

	cn_transport_destroy(transport_a);
	cn_transport_destroy(transport_b);

	return 0;
}

CN_TEST_CASE(cn_transport_drop_fragments, "Create transport, send a couple packets, receive them under packet loss.");
int cn_transport_drop_fragments()
{
	cn_test_transport_data_t data_a = cn_test_transport_data_defaults();
	cn_test_transport_data_t data_b = cn_test_transport_data_defaults();
	data_a.id = 0;
	data_b.id = 1;

	cn_transport_config_t config = cn_transport_config_defaults();
	config.send_packet_fn = cn_test_transport_send_packet_fn;
	config.udata = &data_a;
	cn_transport_t* transport_a = cn_transport_create(config);
	config.udata = &data_b;
	cn_transport_t* transport_b = cn_transport_create(config);
	data_a.transport_a = transport_a;
	data_a.transport_b = transport_b;
	data_b.transport_a = transport_a;
	data_b.transport_b = transport_b;
	double dt = 1.0/60.0;

	int packet_size = 4000;
	uint8_t* packet = (uint8_t*)CN_ALLOC(packet_size, NULL);
	CN_MEMSET(packet, 0xFF, packet_size);

	data_b.drop_packet = 1;

	CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_a, packet, packet_size, true)));
	CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_b, packet, packet_size, true)));

	cn_transport_update(transport_a, dt);
	cn_transport_update(transport_b, dt);

	void* packet_received;
	int packet_received_size;

	CN_TEST_ASSERT(cn_is_error(cn_transport_receive_reliably_and_in_order(transport_a, &packet_received, &packet_received_size)));
	CN_TEST_ASSERT(0 == packet_received_size);
	CN_TEST_ASSERT(packet_received == NULL);

	CN_TEST_CHECK(cn_is_error(cn_transport_receive_reliably_and_in_order(transport_b, &packet_received, &packet_received_size)));
	CN_TEST_ASSERT(packet_size == packet_received_size);
	CN_TEST_ASSERT(!CN_MEMCMP(packet, packet_received, packet_size));
	cn_transport_free_packet(transport_b, packet_received);

	data_b.drop_packet = 0;
	cn_transport_update(transport_b, dt);

	CN_TEST_CHECK(cn_is_error(cn_transport_receive_reliably_and_in_order(transport_a, &packet_received, &packet_received_size)));
	CN_TEST_ASSERT(packet_size == packet_received_size);
	CN_TEST_ASSERT(!CN_MEMCMP(packet, packet_received, packet_size));
	cn_transport_free_packet(transport_a, packet_received);

	data_a.drop_packet = 1;

	CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_a, packet, packet_size, false)));
	CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_b, packet, packet_size, false)));

	cn_transport_update(transport_a, dt);
	cn_transport_update(transport_b, dt);

	CN_TEST_CHECK(cn_is_error(cn_transport_receive_fire_and_forget(transport_a, &packet_received, &packet_received_size)));
	CN_TEST_ASSERT(packet_size == packet_received_size);
	CN_TEST_ASSERT(!CN_MEMCMP(packet, packet_received, packet_size));
	cn_transport_free_packet(transport_a, packet_received);

	CN_TEST_ASSERT(cn_is_error(cn_transport_receive_reliably_and_in_order(transport_b, &packet_received, &packet_received_size)));
	CN_TEST_ASSERT(0 == packet_received_size);
	CN_TEST_ASSERT(packet_received == NULL);

	CN_FREE(packet, NULL);

	cn_transport_destroy(transport_a);
	cn_transport_destroy(transport_b);

	return 0;
}

int cn_send_packet_many_drops_fn(int index, void* packet, int size, void* udata)
{
	cn_test_transport_data_t* data = (cn_test_transport_data_t*)udata;
	if (rand() % 100 != 0) return 0;

	return 0;
}

CN_TEST_CASE(cn_transport_drop_fragments_reliable_hammer, "Create and send many fragments under extreme packet loss.");
int cn_transport_drop_fragments_reliable_hammer()
{
	srand(0);

	cn_test_transport_data_t data_a = cn_test_transport_data_defaults();
	cn_test_transport_data_t data_b = cn_test_transport_data_defaults();
	data_a.id = 0;
	data_b.id = 1;
	double dt = 1.0/60.0;

	cn_transport_config_t config = cn_transport_config_defaults();
	config.send_packet_fn = cn_test_transport_send_packet_fn;
	config.udata = &data_a;
	cn_transport_t* transport_a = cn_transport_create(config);
	config.udata = &data_b;
	cn_transport_t* transport_b = cn_transport_create(config);
	data_a.transport_a = transport_a;
	data_a.transport_b = transport_b;
	data_b.transport_a = transport_a;
	data_b.transport_b = transport_b;

	int packet_size = CN_KB * 10;
	uint8_t* packet = (uint8_t*)CN_ALLOC(packet_size, NULL);
	for (int i = 0; i < packet_size; ++i) {
		packet[i] = (uint8_t)i;
	}

	int fire_and_forget_packet_size = 64;
	uint8_t fire_and_forget_packet[64] = { 0 };

	CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_a, packet, packet_size, true)));

	void* packet_received;
	int packet_received_size;

	int iters = 0;
	int received = 0;

	while (1)
	{
		CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_a, fire_and_forget_packet, fire_and_forget_packet_size, false)));
		CN_TEST_CHECK(cn_is_error(cn_transport_send(transport_b, fire_and_forget_packet, fire_and_forget_packet_size, false)));

		cn_transport_update(transport_a, dt);
		cn_transport_update(transport_b, dt);

		if (!cn_is_error(cn_transport_receive_reliably_and_in_order(transport_b, &packet_received, &packet_received_size))) {
			CN_TEST_ASSERT(packet_size == packet_received_size);
			CN_TEST_ASSERT(!CN_MEMCMP(packet, packet_received, packet_size));
			received = 1;
			cn_transport_free_packet(transport_b, packet_received);
		}
	
		if (received && cn_transport_unacked_fragment_count(transport_a) == 0) {
			break;
		}

		if (iters++ == 100) {
			CN_TEST_ASSERT(false);
			break;
		}
	}

	CN_TEST_ASSERT(received);
	CN_FREE(packet, NULL);

	cn_transport_destroy(transport_a);
	cn_transport_destroy(transport_b);

	return 0;
}

CN_TEST_CASE(cn_packet_connection_accepted, "Write, encrypt, decrypt, and assert the *connection accepted packet*.");
int cn_packet_connection_accepted()
{
	cn_crypto_key_t key = cn_crypto_generate_key();
	uint64_t sequence = 100;

	cn_protocol_packet_connection_accepted_t packet;
	packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CONNECTION_ACCEPTED;
	packet.client_id = 7;
	packet.max_clients = 32;
	packet.connection_timeout = 10;

	uint8_t buffer[CN_PROTOCOL_PACKET_SIZE_MAX];

	int bytes_written = cn_protocol_packet_write(&packet, buffer, sequence, &key);
	CN_TEST_ASSERT(bytes_written > 0);

	void* packet_ptr = cn_protocol_packet_open(buffer, bytes_written, &key, NULL, NULL, NULL);
	CN_TEST_CHECK_POINTER(packet_ptr);
	cn_protocol_packet_connection_accepted_t* packet_val = (cn_protocol_packet_connection_accepted_t*)packet_ptr;

	CN_TEST_ASSERT(packet_val->packet_type == packet.packet_type);
	CN_TEST_ASSERT(packet_val->client_id == packet.client_id);
	CN_TEST_ASSERT(packet_val->max_clients == packet.max_clients);
	CN_TEST_ASSERT(packet_val->connection_timeout == packet.connection_timeout);

	cn_protocol_packet_allocator_free(NULL, (cn_protocol_packet_type_t)packet.packet_type, packet_ptr);

	return 0;
}

CN_TEST_CASE(cn_packet_connection_denied, "Write, encrypt, decrypt, and assert the *connection denied packet*.");
int cn_packet_connection_denied()
{
	cn_crypto_key_t key = cn_crypto_generate_key();
	uint64_t sequence = 100;

	cn_protocol_packet_connection_denied_t packet;
	packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CONNECTION_DENIED;

	uint8_t buffer[CN_PROTOCOL_PACKET_SIZE_MAX];

	int bytes_written = cn_protocol_packet_write(&packet, buffer, sequence, &key);
	CN_TEST_ASSERT(bytes_written > 0);

	void* packet_ptr = cn_protocol_packet_open(buffer, bytes_written, &key, NULL, NULL, NULL);
	CN_TEST_CHECK_POINTER(packet_ptr);
	cn_protocol_packet_connection_denied_t* packet_val = (cn_protocol_packet_connection_denied_t*)packet_ptr;

	CN_TEST_ASSERT(packet_val->packet_type == packet.packet_type);

	cn_protocol_packet_allocator_free(NULL, (cn_protocol_packet_type_t)packet.packet_type, packet_ptr);

	return 0;
}

CN_TEST_CASE(cn_packet_keepalive, "Write, encrypt, decrypt, and assert the *keepalive packet*.");
int cn_packet_keepalive()
{
	cn_crypto_key_t key = cn_crypto_generate_key();
	uint64_t sequence = 100;

	cn_protocol_packet_keepalive_t packet;
	packet.packet_type = CN_PROTOCOL_PACKET_TYPE_KEEPALIVE;

	uint8_t buffer[CN_PROTOCOL_PACKET_SIZE_MAX];

	int bytes_written = cn_protocol_packet_write(&packet, buffer, sequence, &key);
	CN_TEST_ASSERT(bytes_written > 0);

	void* packet_ptr = cn_protocol_packet_open(buffer, bytes_written, &key, NULL, NULL, NULL);
	CN_TEST_CHECK_POINTER(packet_ptr);
	cn_protocol_packet_keepalive_t* packet_val = (cn_protocol_packet_keepalive_t*)packet_ptr;

	CN_TEST_ASSERT(packet_val->packet_type == packet.packet_type);

	cn_protocol_packet_allocator_free(NULL, (cn_protocol_packet_type_t)packet.packet_type, packet_ptr);

	return 0;
}

CN_TEST_CASE(cn_packet_disconnect, "Write, encrypt, decrypt, and assert the *disconnect packet*.");
int cn_packet_disconnect()
{
	cn_crypto_key_t key = cn_crypto_generate_key();
	uint64_t sequence = 100;

	cn_protocol_packet_disconnect_t packet;
	packet.packet_type = CN_PROTOCOL_PACKET_TYPE_DISCONNECT;

	uint8_t buffer[CN_PROTOCOL_PACKET_SIZE_MAX];

	int bytes_written = cn_protocol_packet_write(&packet, buffer, sequence, &key);
	CN_TEST_ASSERT(bytes_written > 0);

	void* packet_ptr = cn_protocol_packet_open(buffer, bytes_written, &key, NULL, NULL, NULL);
	CN_TEST_CHECK_POINTER(packet_ptr);
	cn_protocol_packet_disconnect_t* packet_val = (cn_protocol_packet_disconnect_t*)packet_ptr;

	CN_TEST_ASSERT(packet_val->packet_type == packet.packet_type);

	cn_protocol_packet_allocator_free(NULL, (cn_protocol_packet_type_t)packet.packet_type, packet_ptr);

	return 0;
}

CN_TEST_CASE(cn_packet_challenge, "Write, encrypt, decrypt, and assert the *challenge request packet* and *challenge response packet*.");
int cn_packet_challenge()
{
	cn_crypto_key_t key = cn_crypto_generate_key();
	uint64_t sequence = 100;

	cn_protocol_packet_challenge_t packet;
	packet.packet_type = CN_PROTOCOL_PACKET_TYPE_CHALLENGE_REQUEST;
	packet.challenge_nonce = 30;
	cn_crypto_random_bytes(packet.challenge_data, sizeof(packet.challenge_data));

	uint8_t buffer[CN_PROTOCOL_PACKET_SIZE_MAX];

	int bytes_written = cn_protocol_packet_write(&packet, buffer, sequence, &key);
	CN_TEST_ASSERT(bytes_written > 0);

	void* packet_ptr = cn_protocol_packet_open(buffer, bytes_written, &key, NULL, NULL, NULL);
	CN_TEST_CHECK_POINTER(packet_ptr);
	cn_protocol_packet_challenge_t* packet_val = (cn_protocol_packet_challenge_t*)packet_ptr;

	CN_TEST_ASSERT(packet_val->packet_type == packet.packet_type);
	CN_TEST_ASSERT(packet_val->challenge_nonce == packet.challenge_nonce);
	CN_TEST_ASSERT(!(CN_MEMCMP(packet_val->challenge_data, packet.challenge_data, sizeof(packet.challenge_data))));

	cn_protocol_packet_allocator_free(NULL, (cn_protocol_packet_type_t)packet.packet_type, packet_ptr);

	return 0;
}

CN_TEST_CASE(cn_packet_payload, "Write, encrypt, decrypt, and assert the *payload packet*.");
int cn_packet_payload()
{
	cn_crypto_key_t key = cn_crypto_generate_key();
	uint64_t sequence = 100;

	cn_protocol_packet_payload_t packet;
	packet.packet_type = CN_PROTOCOL_PACKET_TYPE_PAYLOAD;
	packet.payload_size = CN_PROTOCOL_PACKET_PAYLOAD_MAX;
	cn_crypto_random_bytes(packet.payload, sizeof(packet.payload));

	uint8_t buffer[CN_PROTOCOL_PACKET_SIZE_MAX];

	int bytes_written = cn_protocol_packet_write(&packet, buffer, sequence, &key);
	CN_TEST_ASSERT(bytes_written > 0);

	void* packet_ptr = cn_protocol_packet_open(buffer, bytes_written, &key, NULL, NULL, NULL);
	CN_TEST_CHECK_POINTER(packet_ptr);
	cn_protocol_packet_payload_t* packet_val = (cn_protocol_packet_payload_t*)packet_ptr;

	CN_TEST_ASSERT(packet_val->packet_type == packet.packet_type);
	CN_TEST_ASSERT(packet_val->payload_size == packet.payload_size);
	CN_TEST_ASSERT(!(CN_MEMCMP(packet_val->payload, packet.payload, sizeof(packet.payload))));

	cn_protocol_packet_allocator_free(NULL, (cn_protocol_packet_type_t)packet.packet_type, packet_ptr);

	return 0;
}

CN_TEST_CASE(cn_protocol_client_server, "Create client and server, perform connection handshake, then disconnect.");
int cn_protocol_client_server()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);

	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	int iters = 0;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client, 0, 0);
		cn_protocol_server_update(server, 0, 0);

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_client_no_server_responses, "Client tries to connect to servers, but none respond at all.");
int cn_protocol_client_no_server_responses()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
		"[::1]:5001",
		"[::1]:5002",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	int iters = 0;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client, 10, 0);

		if (cn_protocol_client_get_state(client) <= 0) break;
	}
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	return 0;
}

CN_TEST_CASE(cn_protocol_client_server_list, "Client tries to connect to servers, but only third responds.");
int cn_protocol_client_server_list()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
		"[::1]:5001",
		"[::1]:5002",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 15;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);

	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5002", 5)));
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	int iters = 0;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client, 1, 0);
		cn_protocol_server_update(server, 0, 0);

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_server_challenge_response_timeout, "Client times out when sending challenge response.");
int cn_protocol_server_challenge_response_timeout()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);

	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	int iters = 0;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client, 0.1, 0);

		if (cn_protocol_client_get_state(client) != CN_PROTOCOL_CLIENT_STATE_SENDING_CHALLENGE_RESPONSE) {
			cn_protocol_server_update(server, 0, 0);
		}

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CHALLENGED_RESPONSE_TIMED_OUT);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_client_expired_token, "Client gets an expired token before connecting.");
int cn_protocol_client_expired_token()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));
	cn_protocol_client_update(client, 0, 1);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECT_TOKEN_EXPIRED);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	return 0;
}

CN_TEST_CASE(cn_protocol_client_connect_expired_token, "Client detects its own token expires in the middle of a handshake.");
int cn_protocol_client_connect_expired_token()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);

	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	int iters = 0;
	uint64_t time = 0;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client, 0, time++);
		cn_protocol_server_update(server, 0, 0);

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECT_TOKEN_EXPIRED);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_server_connect_expired_token, "Server detects token expires in the middle of a handshake.");
int cn_protocol_server_connect_expired_token()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);

	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	int iters = 0;
	uint64_t time = 0;
	while (iters++ < 100)
	{
		++time;
		cn_protocol_client_update(client, 0, time - 1);
		cn_protocol_server_update(server, 0, time);

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECT_TOKEN_EXPIRED);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_client_bad_keys, "Client attempts to connect without keys from REST SECTION of connect token.");
int cn_protocol_client_bad_keys()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);

	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	// Invalidate client keys.
	client->connect_token.client_to_server_key = cn_crypto_generate_key();
	client->connect_token.server_to_client_key = cn_crypto_generate_key();

	int iters = 0;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client, 1, 0);
		cn_protocol_server_update(server, 1, 0);

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_server_not_in_list_but_gets_request, "Client tries to connect to server, but token does not contain server endpoint.");
int cn_protocol_server_not_in_list_but_gets_request()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5001",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);

	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	// This will make packets arrive to correct server address, but connect token has the wrong address.
	CN_TEST_CHECK(cn_endpoint_init(client->connect_token.endpoints, "[::1]:5000"));

	int iters = 0;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client, 1, 0);
		cn_protocol_server_update(server, 1, 0);

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_connect_a_few_clients, "Multiple clients connecting to one server.");
int cn_protocol_connect_a_few_clients()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 1;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];

	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));
	cn_protocol_client_t* client0 = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client0);
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client0, connect_token)));

	client_to_server_key = cn_crypto_generate_key();
	server_to_client_key = cn_crypto_generate_key();
	client_id = 2;
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));
	cn_protocol_client_t* client1 = cn_protocol_client_create(5002, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client1);
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client1, connect_token)));

	client_to_server_key = cn_crypto_generate_key();
	server_to_client_key = cn_crypto_generate_key();
	client_id = 3;
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));
	cn_protocol_client_t* client2 = cn_protocol_client_create(5003, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client2);
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client2, connect_token)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);
	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));

	int iters = 0;
	float dt = 1.0f / 60.0f;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client0, dt, 0);
		cn_protocol_client_update(client1, dt, 0);
		cn_protocol_client_update(client2, dt, 0);
		cn_protocol_server_update(server, dt, 0);

		if (cn_protocol_client_get_state(client0) <= 0) break;
		if (cn_protocol_client_get_state(client1) <= 0) break;
		if (cn_protocol_client_get_state(client2) <= 0) break;
		if (cn_protocol_client_get_state(client0) == CN_PROTOCOL_CLIENT_STATE_CONNECTED &&
		    cn_protocol_client_get_state(client1) == CN_PROTOCOL_CLIENT_STATE_CONNECTED &&
		    cn_protocol_client_get_state(client2) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client0) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client1) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client2) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);

	cn_protocol_client_disconnect(client0);
	cn_protocol_client_disconnect(client1);
	cn_protocol_client_disconnect(client2);
	cn_protocol_client_destroy(client0);
	cn_protocol_client_destroy(client1);
	cn_protocol_client_destroy(client2);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_keepalive, "Client and server setup connection and maintain it through keepalive packets.");
int cn_protocol_keepalive()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 1;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];

	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));
	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);
	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));

	int iters = 0;
	float dt = 1.0f / 60.0f;
	while (iters++ < 1000)
	{
		cn_protocol_client_update(client, dt, 0);
		cn_protocol_server_update(server, dt, 0);

		if (cn_protocol_client_get_state(client) <= 0) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters == 1001);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_client_initiated_disconnect, "Client initiates disconnect, assert disconnect occurs cleanly.");
int cn_protocol_client_initiated_disconnect()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 1;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];

	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));
	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);
	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));

	int iters = 0;
	float dt = 1.0f / 60.0f;
	while (iters++ < 1000)
	{
		if (cn_protocol_client_get_state(client) > 0) {
			cn_protocol_client_update(client, dt, 0);
		}
		cn_protocol_server_update(server, dt, 0);

		if (iters == 100) {
			CN_TEST_ASSERT(cn_protocol_server_client_count(server) == 1);
			cn_protocol_client_disconnect(client);
		}

		if (iters == 110) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(cn_protocol_server_client_count(server) == 0);
	CN_TEST_ASSERT(iters == 110);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_DISCONNECTED);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_server_initiated_disconnect, "Server initiates disconnect, assert disconnect occurs cleanly.");
int cn_protocol_server_initiated_disconnect()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 1;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];

	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));
	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);
	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));

	int client_index;

	int iters = 0;
	float dt = 1.0f / 60.0f;
	while (iters++ < 1000)
	{
		cn_protocol_client_update(client, dt, 0);
		cn_protocol_server_update(server, dt, 0);

		if (iters == 100) {
			CN_TEST_ASSERT(cn_protocol_server_client_count(server) == 1);
			cn_protocol_server_event_t event;
			CN_TEST_ASSERT(cn_protocol_server_pop_event(server, &event));
			CN_TEST_ASSERT(event.type == CN_PROTOCOL_SERVER_EVENT_NEW_CONNECTION);
			client_index = event.u.new_connection.client_index;
			cn_protocol_server_disconnect_client(server, client_index, true);
		}

		if (iters == 110) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(cn_protocol_server_client_count(server) == 0);
	CN_TEST_ASSERT(iters == 110);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_DISCONNECTED);
	cn_protocol_server_event_t event;
	CN_TEST_ASSERT(cn_protocol_server_pop_event(server, &event));
	CN_TEST_ASSERT(event.type == CN_PROTOCOL_SERVER_EVENT_DISCONNECTED);
	CN_TEST_ASSERT(event.u.disconnected.client_index == client_index);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_client_server_payloads, "Client and server connect and send payload packets. Server should confirm client.");
int cn_protocol_client_server_payloads()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);

	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	int client_index;
	uint64_t to_server_data = 3;
	uint64_t to_client_data = 4;

	int iters = 0;
	float dt = 1.0f / 60.0f;
	int payloads_received_by_server = 0;
	int payloads_received_by_client = 0;
	while (iters++ < 1000)
	{
		cn_protocol_client_update(client, dt, 0);
		cn_protocol_server_update(server, dt, 0);

		cn_protocol_server_event_t event;
		if (cn_protocol_server_pop_event(server, &event)) {
			CN_TEST_ASSERT(event.type != CN_PROTOCOL_SERVER_EVENT_DISCONNECTED);
			if (event.type == CN_PROTOCOL_SERVER_EVENT_NEW_CONNECTION) {
				client_index = event.u.new_connection.client_index;
				CN_TEST_ASSERT(cn_protocol_server_get_client_id(server, client_index) == client_id);
			} else {
				CN_TEST_ASSERT(client_index == event.u.payload_packet.client_index);
				CN_TEST_ASSERT(sizeof(uint64_t) == event.u.payload_packet.size);
				uint64_t* data = (uint64_t*)event.u.payload_packet.data;
				CN_TEST_ASSERT(*data == to_server_data);
				cn_protocol_server_free_packet(server, data);
				++payloads_received_by_server;
			}
		}

		void* packet = NULL;
		uint64_t sequence = ~0ULL;
		int size;
		if (cn_protocol_client_get_packet(client, &packet, &size, &sequence)) {
			CN_TEST_ASSERT(sizeof(uint64_t) == size);
			uint64_t* data = (uint64_t*)packet;
			CN_TEST_ASSERT(*data == to_client_data);
			cn_protocol_client_free_packet(client, packet);
			++payloads_received_by_client;
		}

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) {
			CN_TEST_CHECK(cn_is_error(cn_protocol_client_send(client, &to_server_data, sizeof(uint64_t))));
			CN_TEST_CHECK(cn_is_error(cn_protocol_server_send_to_client(server, &to_client_data, sizeof(uint64_t), client_index)));
		}

		if (payloads_received_by_server >= 10 && payloads_received_by_client >= 10) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 1000);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

CN_TEST_CASE(cn_protocol_multiple_connections_and_payloads, "A server hosts multiple simultaneous clients with payloads and random disconnects/connects.");
int cn_protocol_multiple_connections_and_payloads()
{
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	const int max_clients = 5;
	uint64_t application_id = 100;

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);
	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 2)));

	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 2;
	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	cn_protocol_client_t** clients = (cn_protocol_client_t**)CN_ALLOC(sizeof(cn_protocol_client_t*) * max_clients, NULL);

	for (int i = 0; i < max_clients; ++i)
	{
		cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
		cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
		uint64_t client_id = (uint64_t)i;

		CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
			application_id,
			current_timestamp,
			&client_to_server_key,
			&server_to_client_key,
			expiration_timestamp,
			handshake_timeout,
			sizeof(endpoints) / sizeof(endpoints[0]),
			endpoints,
			client_id,
			user_data,
			&sk,
			connect_token
		)));
		cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
		CN_TEST_CHECK_POINTER(client);
		CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));
		clients[i] = client;
	}

	uint64_t to_server_data = 3;
	uint64_t to_client_data = 4;

	int iters = 0;
	float dt = 1.0f / 20.0f;
	int payloads_received_by_server = 0;
	int* payloads_received_by_client = (int*)CN_ALLOC(sizeof(int) * max_clients, NULL);
	CN_MEMSET(payloads_received_by_client, 0, sizeof(int) * max_clients);
	int client_count = 2;
	while (iters++ < 100) {
		for (int i = 0; i < client_count; ++i)
			cn_protocol_client_update(clients[i], dt, 0);

		cn_protocol_server_update(server, dt, 0);

		for (int i = 0; i < client_count; ++i)
			if (cn_protocol_client_get_state(clients[i]) <= 0)
				break;

		if (iters == 4) {
			client_count += 3;
		}

		if (iters == 8) {
			client_count -= 2;
		}

		cn_protocol_server_event_t event;
		while (cn_protocol_server_pop_event(server, &event)) {
			if (event.type == CN_PROTOCOL_SERVER_EVENT_PAYLOAD_PACKET) {
				CN_TEST_ASSERT(sizeof(uint64_t) == event.u.payload_packet.size);
				uint64_t* data = (uint64_t*)event.u.payload_packet.data;
				CN_TEST_ASSERT(*data == to_server_data);
				cn_protocol_server_free_packet(server, data);
				++payloads_received_by_server;
			}
		}

		for (int i = 0; i < client_count; ++i) {
			void* packet = NULL;
			uint64_t sequence = ~0ULL;
			int size;
			if (cn_protocol_client_get_packet(clients[i], &packet, &size, &sequence)) {
				CN_TEST_ASSERT(sizeof(uint64_t) == size);
				uint64_t* data = (uint64_t*)packet;
				CN_TEST_ASSERT(*data == to_client_data);
				cn_protocol_client_free_packet(clients[i], packet);
				payloads_received_by_client[i]++;
			}
		}

		for (int i = 0; i < client_count; ++i) {
			if (cn_protocol_client_get_state(clients[i]) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) {
				CN_TEST_CHECK(cn_is_error(cn_protocol_client_send(clients[i], &to_server_data, sizeof(uint64_t))));
				CN_TEST_CHECK(cn_is_error(cn_protocol_server_send_to_client(server, &to_client_data, sizeof(uint64_t), i)));
			}
		}
	}
	for (int i = 0; i < client_count; ++i)
	{
		CN_TEST_ASSERT(payloads_received_by_client[i] >= 1);
	}
	for (int i = 0; i < max_clients; ++i)
	{
		cn_protocol_client_update(clients[i], 0, 0);
		if (i >= client_count) {
			CN_TEST_ASSERT(cn_protocol_client_get_state(clients[i]) == CN_PROTOCOL_CLIENT_STATE_DISCONNECTED);
		} else {
			CN_TEST_ASSERT(cn_protocol_client_get_state(clients[i]) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);
		}
		cn_protocol_client_disconnect(clients[i]);
		cn_protocol_client_destroy(clients[i]);
	}

	cn_protocol_server_update(server, dt, 0);
	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	CN_FREE(clients, NULL);
	CN_FREE(payloads_received_by_client, NULL);

	return 0;
}

CN_TEST_CASE(cn_protocol_client_reconnect, "Client connects to server, disconnects, and reconnects.");
int cn_protocol_client_reconnect()
{
	cn_crypto_key_t client_to_server_key = cn_crypto_generate_key();
	cn_crypto_key_t server_to_client_key = cn_crypto_generate_key();
	cn_crypto_sign_public_t pk;
	cn_crypto_sign_secret_t sk;
	cn_crypto_sign_keygen(&pk, &sk);

	const char* endpoints[] = {
		"[::1]:5000",
	};

	uint64_t application_id = 100;
	uint64_t current_timestamp = 0;
	uint64_t expiration_timestamp = 1;
	uint32_t handshake_timeout = 5;
	uint64_t client_id = 17;

	uint8_t user_data[CN_PROTOCOL_CONNECT_TOKEN_USER_DATA_SIZE];
	cn_crypto_random_bytes(user_data, sizeof(user_data));

	uint8_t connect_token[CN_PROTOCOL_CONNECT_TOKEN_SIZE];
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	cn_protocol_server_t* server = cn_protocol_server_create(application_id, &pk, &sk, NULL);
	CN_TEST_CHECK_POINTER(server);

	cn_protocol_client_t* client = cn_protocol_client_create(0, application_id, true, NULL);
	CN_TEST_CHECK_POINTER(client);

	CN_TEST_CHECK(cn_is_error(cn_protocol_server_start(server, "[::1]:5000", 5)));
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));

	// Connect client.
	int iters = 0;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client, 0, 0);
		cn_protocol_server_update(server, 0, 0);

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);

	// Disonnect client.
	cn_protocol_client_disconnect(client);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_DISCONNECTED);

	iters = 0;
	while (iters++ < 100)
	{
		cn_protocol_server_update(server, 0, 0);
		if (cn_protocol_server_client_count(server) == 0) break;
	}
	CN_TEST_ASSERT(iters < 100);

	// Generate new connect token.
	CN_TEST_CHECK(cn_is_error(cn_generate_connect_token(
		application_id,
		current_timestamp,
		&client_to_server_key,
		&server_to_client_key,
		expiration_timestamp,
		handshake_timeout,
		sizeof(endpoints) / sizeof(endpoints[0]),
		endpoints,
		client_id,
		user_data,
		&sk,
		connect_token
	)));

	// Reconnect client.
	CN_TEST_CHECK(cn_is_error(cn_protocol_client_connect(client, connect_token)));
	iters = 0;
	while (iters++ < 100)
	{
		cn_protocol_client_update(client, 0, 0);
		cn_protocol_server_update(server, 0, 0);

		if (cn_protocol_client_get_state(client) <= 0) break;
		if (cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED) break;
	}
	CN_TEST_ASSERT(cn_protocol_server_running(server));
	CN_TEST_ASSERT(iters < 100);
	CN_TEST_ASSERT(cn_protocol_client_get_state(client) == CN_PROTOCOL_CLIENT_STATE_CONNECTED);

	cn_protocol_client_disconnect(client);
	cn_protocol_client_destroy(client);

	cn_protocol_server_update(server, 0, 0);
	cn_protocol_server_stop(server);
	cn_protocol_server_destroy(server);

	return 0;
}

// -------------------------------------------------------------------------------------------------

int cn_run_tests(int which_test, bool soak)
{
	if (cn_is_error(cn_init())) {
		return -1;
	}

#ifdef _MSC_VER
	_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG | _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	windows_turn_on_console_color();
#endif

	cn_test_t tests[] = {
		CN_TEST_CASE_ENTRY(cn_socket_init_send_recieve_shutdown),
		CN_TEST_CASE_ENTRY(cn_sequence_buffer_basic),
		CN_TEST_CASE_ENTRY(cn_replay_buffer_valid_packets),
		CN_TEST_CASE_ENTRY(cn_replay_buffer_old_packet_out_of_range),
		CN_TEST_CASE_ENTRY(cn_replay_buffer_duplicate),
		CN_TEST_CASE_ENTRY(cn_hash_table_basic),
		CN_TEST_CASE_ENTRY(cn_hash_table_set),
		CN_TEST_CASE_ENTRY(cn_hash_table_hammer),
		CN_TEST_CASE_ENTRY(cn_handle_basic),
		CN_TEST_CASE_ENTRY(cn_handle_large_loop),
		CN_TEST_CASE_ENTRY(cn_handle_large_loop_and_free),
		CN_TEST_CASE_ENTRY(cn_handle_alloc_too_many),
		CN_TEST_CASE_ENTRY(cn_encryption_map_basic),
		CN_TEST_CASE_ENTRY(cn_encryption_map_timeout_and_expiration),
		CN_TEST_CASE_ENTRY(cn_doubly_list),
		CN_TEST_CASE_ENTRY(cn_crypto_encrypt_decrypt),
		CN_TEST_CASE_ENTRY(cn_connect_token_cache),
		CN_TEST_CASE_ENTRY(cn_test_generate_connect_token),
		CN_TEST_CASE_ENTRY(cn_client_server),
		CN_TEST_CASE_ENTRY(cn_client_server_payload),
		CN_TEST_CASE_ENTRY(cn_client_server_sim),
		CN_TEST_CASE_ENTRY(cn_ack_system_basic),
		CN_TEST_CASE_ENTRY(cn_transport_basic),
		CN_TEST_CASE_ENTRY(cn_replay_buffer_duplicate),
		CN_TEST_CASE_ENTRY(cn_transport_drop_fragments),
		CN_TEST_CASE_ENTRY(cn_transport_drop_fragments_reliable_hammer),
		CN_TEST_CASE_ENTRY(cn_packet_connection_accepted),
		CN_TEST_CASE_ENTRY(cn_packet_connection_denied),
		CN_TEST_CASE_ENTRY(cn_packet_keepalive),
		CN_TEST_CASE_ENTRY(cn_packet_disconnect),
		CN_TEST_CASE_ENTRY(cn_packet_challenge),
		CN_TEST_CASE_ENTRY(cn_packet_payload),
		CN_TEST_CASE_ENTRY(cn_protocol_client_server),
		CN_TEST_CASE_ENTRY(cn_protocol_client_no_server_responses),
		CN_TEST_CASE_ENTRY(cn_protocol_client_server_list),
		CN_TEST_CASE_ENTRY(cn_protocol_server_challenge_response_timeout),
		CN_TEST_CASE_ENTRY(cn_protocol_client_expired_token),
		CN_TEST_CASE_ENTRY(cn_protocol_client_connect_expired_token),
		CN_TEST_CASE_ENTRY(cn_protocol_server_connect_expired_token),
		CN_TEST_CASE_ENTRY(cn_protocol_client_bad_keys),
		CN_TEST_CASE_ENTRY(cn_protocol_server_not_in_list_but_gets_request),
		CN_TEST_CASE_ENTRY(cn_protocol_connect_a_few_clients),
		CN_TEST_CASE_ENTRY(cn_protocol_keepalive),
		CN_TEST_CASE_ENTRY(cn_protocol_client_initiated_disconnect),
		CN_TEST_CASE_ENTRY(cn_protocol_server_initiated_disconnect),
		CN_TEST_CASE_ENTRY(cn_protocol_client_server_payloads),
		CN_TEST_CASE_ENTRY(cn_protocol_multiple_connections_and_payloads),
		CN_TEST_CASE_ENTRY(cn_protocol_client_reconnect),
	};
	
	int test_count = sizeof(tests) / sizeof(*tests);
	int fail_count = 0;

	if (soak) {
		while (1)
		{
			for (int i = 0; i < test_count; ++i)
			{
				cn_test_t* test = tests + i;
				int result = cn_do_test(test, i + 1);
				if (result) goto break_soak;
			}
		}
	}

	// Run all tests.
	if (which_test == -1) {
		for (int i = 0; i < test_count; ++i)
		{
			cn_test_t* test = tests + i;
			if (cn_do_test(test, i + 1)) fail_count++;
		}
		if (fail_count) {
			CN_FPRINTF(CN_TEST_IO_STREAM, "\033[31mFAILED\033[0m %d test case%s.\n\n", fail_count, fail_count > 1 ? "s" : "");
		} else {
			CN_FPRINTF(CN_TEST_IO_STREAM, "All %d tests \033[32mPASSED\033[0m.\n\n", test_count);
		}
	} else {
		cn_do_test(tests + which_test, 1);
	}

	return fail_count ? -1 : 0;

break_soak:
	return -1;
}

#endif // CUTE_NET_TESTS_ONCE
#endif // CUTE_NET_TESTS

#endif // CUTE_NET_IMPLEMENTATION_ONCE
#endif // CUTE_NET_IMPLEMENTATION

/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2022 Randy Gaul http://www.randygaul.net
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
