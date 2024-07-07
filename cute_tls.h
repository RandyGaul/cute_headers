/*
	------------------------------------------------------------------------------
		Licensing information can be found at the end of the file.
	------------------------------------------------------------------------------

	cute_tls.h - v1.01

	To create implementation (the function definitions)
		#define CUTE_TLS_IMPLEMENTATION
	in *one* C/CPP file (translation unit) that includes this file


	SUMMARY

		cute_tls.h is a single-file header that implements some functions to
		make a connection over TLS 1.2 and send some data back and forth over a
		TCP socket. It's meant mainly for making some simple HTTPS requsts to a web
		server, but nothing heavy-duty requiring extreme performance. It uses native
		APIs on Windows and Apple machines to get access to highly robust TLS
		implementations for security. This header merely facilitates these native APIs
		through a TCP socket.

		On Windows Secure Channel is used. On Apple machines the Network.framework is used.
		For *Nix s2n can be used a third-party solution, since no good OS-level TLS
		handshake is currently available on Linux.


	GENERAL INFORMATION ABOUT HTTPS

		This header is great for providing a TLS tunnel to hook up to your favorite
		https writer. Simply pipe the finalized HTTP buffer through a TLS connection
		created by this header, and boom -- you have HTTPS.

		The general theory behind how it works: websites out in the world ask a special
		entity called a Certificate Authority (CA) to recognize them as a valid website.
		The CA gives them a certificate (usually an X.509 cert). It usually lasts for some
		number of years, and is used to validate that a message came from the website you
		think it did. This prevents anyone from pretending to who they are not, and also
		prevents anyone from reading or tampering with packets between yourself and the website.

		This works by requesting the website provide you with their certificate. Everyone's
		machines have built-in mechanisms to ask a trusted CA if the website's certificate
		is valid. These are usually installed along with the operating system or browser.

		HTTPS is HTTP over the TLS protocol, using high-powered cryptography via certificates.
		By opening a connection with cute_tls.h we immediately have access to making HTTPS
		requests!


	LIMITATIONS

		Emscripten/Android platforms are not currently supported.

		Client credentials are not supported.

		IPv6 is not supported, though this is totally possible, just not initially in v1.00.

		The server side of the connection is *not* supported. This is a client-only implementation.


	BUILDING ON APPLE DEVICES

		To build on Apple devices you must compile this file as an Objective-C file (.m). This is
		kind of icky and super lame, but blame Apple for continually deprecating their C APIs and
		releasing newer Obj-C replacements.

		Link against Network.framework, and Security.framework. Make sure automatic reference
		counting (ARC) is off if you're using XCode.

		Example command line:

			clang -framework Network main.cpp -o my_executable


	BUILDING ON *NIX (INCLUDING APPLE DEVICES)

		A wrapping implementation of s2n is implemented, see: https://github.com/aws/s2n-tls
		s2n is a pretty good library for implementing TLS servers, but can also be used for
		clients. It's quite a good library for Apple/Linux, but has no Windows support. Since
		Linux has no out-of-the-box TLS handshake implementation at the OS-level, unlike Apple/Windows,
		we have to choose some third-party tool to get things done. Another decent choice could
		have been mbedtls, but s2n is implemented with much less code, and was thus chosen instead.

		If on MacOS for development make sure to install s2n on your machine. I suggest using brew.

			brew install s2n

		Regardless of how you install s2n make sure your compiler can find the static library and
		headers in order to properly link against s2n. Here's an example implementation for devloping
		on MacOS: https://github.com/RandyGaul/cute_headers/tree/master/examples_cute_tls/macos/s2n

		For Linux it's basically he same story. If you're using Linux I assume you know what you're
		doing, so follow along on the s2n docs as you see fit: https://github.com/aws/s2n-tls


	OTHER OPERATING SYSTEMS (e.g. ANDROID/EMSCRIPTEN)

		Android is a bit of another story. I don't have too much Android dev experience, but the best
		option may be to call from C into Java and use Android's on SSL socket, or their HTTPS APi
		directly from Java: https://developer.android.com/training/articles/security-ssl

		For Emscripten it looks like they have a C++ websocket wrapper that might work, though I
		have yet to try it: https://emscripten.org/docs/porting/networking.html#emscripten-websockets-api


	CUSTOMIZATION

		A variety of macros can be overriden by merely defining them before including the implementation
		section of this file. Define any one of them to use your own custom functions.

		TLS_MALLOC
		TLS_MEMCPY
		TLS_MEMSET
		TLS_MEMMOVE
		TLS_ASSERT
		TLS_STRCMP
		TLS_PACKET_QUEUE_MAX_ENTRIES (default at 64)


	SPECIAL THANKS

		A special thanks goes to Martins Mozeiko for their sample code on setting up a TLS
		connection via SChannel: https://gist.github.com/mmozeiko/c0dfcc8fec527a90a02145d2cc0bfb6d

		A special thanks goes to Mattias Gustavsson for their API design in https.h:
		https://github.com/mattiasgustavsson/libs/blob/main/http.h


	Revision history:
		1.00 (06/17/2023) initial release
		1.01 (06/19/2023) s2n implementation for apple/linux
*/

/*
	DOCUMENTATION

		1. Call tls_connect
			TLS_Connection connection = tls_connect(hostname, 443);
		
		2. Call tls_process
			while (1) {
				TLS_State state = tls_process(connection);
				...
			}
		
		3. Call tls_read or tls_send
			tls_send(connection, buffer, size);
			or
			tls_read(connection, buffer, size);
		
		4. Call tls_disconnect
		
		5. For errors call tls_state_string on the return value of tls_process.


	FULL DEMO PROGRAM
	
		Here's a full program that connects to a given website and sends an HTTP GET request.
		The entire HTTP response is saved to a file called "response.txt". You may also try using
		badssl.com to try out a variety of failure cases (the commented strings at the top of the
		demo function).

			#define CUTE_TLS_IMPLEMENTATION
			#include "cute_tls.h"
			
			int main()
			{
				const char* hostname = "www.google.com";
				//const char* hostname = "badssl.com";
				//const char* hostname = "expired.badssl.com";
				//const char* hostname = "wrong.host.badssl.com";
				//const char* hostname = "self-signed.badssl.com";
				//const char* hostname = "untrusted-root.badssl.com";

				TLS_Connection connection = tls_connect(hostname, 443);

				while (1) {
					TLS_State state = tls_process(connection);
					if (state == TLS_STATE_CONNECTED) {
						break;
					} else if (state < 0) {
						printf("Error connecting to to %s with code %s.\n", hostname, tls_state_string(state));
						return -1;
					}
				}

				printf("Connected!\n");

				// Send GET request.
				char req[1024];
				int len = sprintf(req, "GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", hostname);
				if (tls_send(connection, req, len) < 0) {
					tls_disconnect(connection);
					printf("Failed to send data.\n");
					return -1;
				}

				// Write the full HTTP response to file.
				FILE* fp = fopen("response.txt", "wb");
				int received = 0;
				char buf[TLS_MAX_PACKET_SIZE];
				while (1) {
					TLS_State state = tls_process(connection);
					if (state == TLS_STATE_DISCONNECTED) {
						break;
					}

					int bytes = tls_read(connection, buf, sizeof(buf));
					if (bytes < 0) {
						tls_disconnect(connection);
						printf("Failed reading bytes.\n");
						return -1;
					}
					if (bytes) {
						fwrite(buf, 1, bytes, fp);
						fflush(fp);
						received += bytes;
					}
				}
				fclose(fp);

				printf("Received %d bytes\n", received);

				tls_disconnect(connection);
			}
*/

#ifndef CUTE_TLS_H
#define CUTE_TLS_H

typedef struct TLS_Connection { unsigned long long id; } TLS_Connection;

// Initiates a new TLS 1.2 connection.
TLS_Connection tls_connect(const char* hostname, int port);

// Frees up all resources associated with the connection.
void tls_disconnect(TLS_Connection connection);

typedef enum TLS_State
{
	TLS_STATE_BAD_CERTIFICATE                       = -8, // Bad or unsupported cert format.
	TLS_STATE_SERVER_ASKED_FOR_CLIENT_CERTS         = -7, // Not supported.
	TLS_STATE_CERTIFICATE_EXPIRED                   = -6,
	TLS_STATE_BAD_HOSTNAME                          = -5,
	TLS_STATE_CANNOT_VERIFY_CA_CHAIN                = -4,
	TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS     = -3,
	TLS_STATE_INVALID_SOCKET                        = -2,
	TLS_STATE_UNKNOWN_ERROR                         = -1,
	TLS_STATE_DISCONNECTED                          = 0,
	TLS_STATE_DISCONNECTED_BUT_PACKETS_STILL_REMAIN = 1,  // The TCP socket closed, but you should keep calling `tls_read`.
	TLS_STATE_PENDING                               = 2,  // Handshake in progress.
	TLS_STATE_CONNECTED                             = 3,
	TLS_STATE_PACKET_QUEUE_FILLED                   = 4,  // Not calling `tls_read` enough. Did you forget to call this in a loop after `tls_process`?
} TLS_State;

// Call this in a loop to update the connection.
// This will perform the initial connect sequence, and also fetch data off the wire once connected.
TLS_State tls_process(TLS_Connection connection);

// Returns `TLS_State` as a string.
const char* tls_state_string(TLS_State state);

// Returns number of bytes read on success, -1 on failure.
int tls_read(TLS_Connection connection, void* data, int size);

// Reads up to size bytes, returns amount of bytes received on success (<= size).
// Returns 0 on disconnect or -1 on error.
int tls_send(TLS_Connection connection, const void* data, int size);

#define TLS_1_KB 1024
#define TLS_MAX_RECORD_SIZE (16 * TLS_1_KB)                  // TLS defines records to be up to 16kb.
#define TLS_MAX_PACKET_SIZE (TLS_MAX_RECORD_SIZE + TLS_1_KB) // Some extra rooms for records split over two packets.

#endif // CUTE_TLS_H

#ifdef CUTE_TLS_IMPLEMENTATION
#ifndef CUTE_TLS_IMPLEMENTATION_ONCE
#define CUTE_TLS_IMPLEMENTATION_ONCE

#ifndef _CRT_SECURE_NO_WARNINGS
#	define _CRT_SECURE_NO_WARNINGS
#endif
#ifndef _CRT_NONSTDC_NO_DEPRECATE
#	define _CRT_NONSTDC_NO_DEPRECATE
#endif
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#	define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#ifdef CUTE_TLS_S2N
#	define TLS_S2N
#endif

#ifndef TLS_S2N
#	ifdef _WIN32
#		define TLS_WINDOWS
#	elif defined(__APPLE__)
#		define TLS_APPLE
#	elif defined(__linux__) || defined(__unix__) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
#		define TLS_S2N
#	else
#		error Platform not yet supported.
#	endif
#endif

#ifdef TLS_WINDOWS
#	ifndef WIN32_LEAN_AND_MEAN
#		define WIN32_LEAN_AND_MEAN
#	endif
#	ifndef NOMINMAX
#		define NOMINMAX
#	endif
#	include <winsock2.h>
#	include <ws2tcpip.h>
#	include <windows.h>
#	ifndef SECURITY_WIN32
#		define SECURITY_WIN32
#	endif
#	include <security.h>
#	include <schannel.h>
#	include <shlwapi.h>
#	include <assert.h>
#	include <stdio.h>

#	pragma comment (lib, "ws2_32.lib")
#	pragma comment (lib, "secur32.lib")
#	pragma comment (lib, "shlwapi.lib")
#elif defined(TLS_APPLE)
#	include <Network/Network.h>
#	include <pthread.h>
#elif defined(TLS_S2N)
#	include <assert.h>
#	include <stdio.h>
#	include <stdlib.h>
#	include <string.h>
#	include <sys/socket.h>
#	include <fcntl.h>
#	include <unistd.h>
#	include <netdb.h>
#	include <errno.h>
#	include <s2n.h>
#else
#	error No supported backend implementation found.
#endif

#define TLS_MIN(x, y) ((x) < (y) ? (x) : (y))
#define TLS_ARRAYSIZE(A) (sizeof(A) / sizeof(*A))

#ifndef TLS_MALLOC
#	define TLS_MALLOC malloc
#	define TLS_FREE free
#endif

#ifndef TLS_MEMCPY
#	define TLS_MEMCPY memcpy
#endif

#ifndef TLS_MEMSET
#	define TLS_MEMSET memset
#endif

#ifndef TLS_MEMMOVE
#	define TLS_MEMMOVE memmove
#endif

#ifndef TLS_ASSERT
#	define TLS_ASSERT assert
#endif

#ifndef TLS_STRCMP
#	define TLS_STRCMP strcmp
#endif

#ifndef TLS_STRNCMP
#	define TLS_STRNCMP strncmp
#endif

#ifndef TLS_PACKET_QUEUE_MAX_ENTRIES
#	define TLS_PACKET_QUEUE_MAX_ENTRIES (64)
#endif

typedef struct TLS_PacketQueue
{
#ifdef TLS_APPLE
	pthread_mutex_t lock;
#endif
	int count;
	int index0;
	int index1;
	int sizes[TLS_PACKET_QUEUE_MAX_ENTRIES];
	void* packets[TLS_PACKET_QUEUE_MAX_ENTRIES];
} TLS_PacketQueue;

static void tls_packet_queue_push(TLS_PacketQueue* q, void* packet, int size)
{
	#ifdef TLS_APPLE
		pthread_mutex_lock(&q->lock);
	#endif
	if (q->count < TLS_PACKET_QUEUE_MAX_ENTRIES) {
		q->count++;
		q->sizes[q->index1] = size;
		q->packets[q->index1] = packet;
		q->index1 = (q->index1 + 1) % TLS_PACKET_QUEUE_MAX_ENTRIES;
	}
	#ifdef TLS_APPLE
		pthread_mutex_unlock(&q->lock);
	#endif
}

void tls_packet_queue_pop(TLS_PacketQueue* q, void** packet, int* size)
{
	#ifdef TLS_APPLE
		pthread_mutex_lock(&q->lock);
	#endif
	if (q->count > 0) {
		q->count--;
		*size = q->sizes[q->index0];
		*packet = q->packets[q->index0];
		q->index0 = (q->index0 + 1) % TLS_PACKET_QUEUE_MAX_ENTRIES;
	}
	#ifdef TLS_APPLE
		pthread_mutex_unlock(&q->lock);
	#endif
}

typedef struct TLS_Context
{
	TLS_PacketQueue q;    // For receiving packets.
	TLS_State state;      // Current state of the connection. Negative values are errors.
	const char* hostname; // Website or address to connect to.
	void* packet;
	int packet_size;

#ifdef TLS_WINDOWS
	SOCKET sock;
	CredHandle handle;
	CtxtHandle context;
	SecPkgContext_StreamSizes sizes;
	bool tcp_connect_pending;
	bool first_call;
	int received;    // Byte count in incoming buffer (ciphertext).
	int used;        // Byte count used from incoming buffer to decrypt current packet.
	int available;   // Byte count available for decrypted bytes.
	char* decrypted; // Points to incoming buffer where data is decrypted in-place.
	char incoming[TLS_MAX_PACKET_SIZE];
#elif defined(TLS_S2N)
	int sock;
	struct s2n_connection* connection;
	bool tcp_connect_pending;
	int received;
	char incoming[TLS_MAX_PACKET_SIZE];
#elif defined(TLS_APPLE)
	dispatch_queue_t dispatch;
	nw_connection_t connection;
#endif
} TLS_Context;

#ifdef TLS_S2N
int tls_s2n_init = 0;
#endif

// Called in a poll-style manner on Windows.
// For Apple we call this once on init to setup a tail-end recursive callback loop.
static void tls_recv(TLS_Context* ctx)
{
	#ifdef TLS_WINDOWS
		fd_set sockets_to_check;
		FD_ZERO(&sockets_to_check);
		FD_SET(ctx->sock, &sockets_to_check);
		struct timeval timeout; timeout.tv_sec = 0; timeout.tv_usec = 0;
		while (select((int)(ctx->sock + 1), &sockets_to_check, NULL, NULL, &timeout) == 1) {
			int r = recv(ctx->sock, ctx->incoming + ctx->received, sizeof(ctx->incoming) - ctx->received, 0);
			if (r == 0) {
				// Server disconnected the socket.
				ctx->state = TLS_STATE_DISCONNECTED;
				break;
			} else if (r == SOCKET_ERROR) {
				// Socket related error.
				ctx->state = TLS_STATE_INVALID_SOCKET;
				break;
			} else {
				ctx->received += r;
			}
		}
	#endif // TLS_WINDOWS

	#ifdef TLS_APPLE
		// Queue up an asynchronous receive block loop.
		nw_connection_receive(ctx->connection, 1, TLS_MAX_PACKET_SIZE, ^(dispatch_data_t content, nw_content_context_t context, bool is_complete, 	nw_error_t receive_error) {
			if (content != NULL) {
				// What a horrid API design... So over-engineered to simply memcpy a buffer.
				int size = (int)dispatch_data_get_size(content);
				void* packet = TLS_MALLOC(size);
				dispatch_data_apply(content, ^bool(dispatch_data_t content, size_t offset, const void* buffer, size_t size) {
					TLS_MEMCPY((char*)packet + offset, buffer, size);
					return true;
				});
				tls_packet_queue_push(&ctx->q, packet, size);
			}
			if (is_complete && !receive_error) {
				ctx->state = TLS_STATE_DISCONNECTED_BUT_PACKETS_STILL_REMAIN;
			} else if (receive_error) {
				ctx->state = TLS_STATE_UNKNOWN_ERROR;
			} else {
				// Queue up another call to this function to receive the next packet.
				tls_recv(ctx);
			}
		});
	#endif // TLS_APPLE

	#ifdef TLS_S2N
		s2n_blocked_status blocked = S2N_NOT_BLOCKED;
		int bytes_read = 0;
		while (bytes_read < sizeof(ctx->incoming)) {
			int r = s2n_recv(ctx->connection, ctx->incoming + bytes_read, sizeof(ctx->incoming) - bytes_read, &blocked);
			s2n_error_type etype = (s2n_error_type)s2n_error_get_type(s2n_errno);
			if (r == 0) {
				break;
			} else if (r > 0) {
				bytes_read += r;
			} else if (etype == S2N_ERR_T_CLOSED) {
				ctx->state = TLS_STATE_DISCONNECTED;
				break;
			} else if (etype == S2N_ERR_T_IO) {
				ctx->state = TLS_STATE_INVALID_SOCKET;
				break;
			} else if (etype != S2N_ERR_T_BLOCKED) {
				ctx->state = TLS_STATE_UNKNOWN_ERROR;
				break;
			}
		}
		ctx->received = bytes_read;
	#endif // TLS_S2N
}

TLS_Connection tls_connect(const char* hostname, int port)
{
	TLS_Connection result = { 0 };

	TLS_Context* ctx = (TLS_Context*)TLS_MALLOC(sizeof(TLS_Context));
	TLS_MEMSET(ctx, 0, sizeof(*ctx));
	ctx->hostname = hostname;

	char sport[64];
	snprintf(sport, sizeof(sport), "%u", port);

	#if defined(TLS_WINDOWS) || defined(TLS_S2N)
		ctx->tcp_connect_pending = 1;

		#ifdef TLS_WINDOWS
			ctx->first_call = 1;

			// Initialize winsock.
			WSADATA wsadata;
			if (WSAStartup(MAKEWORD(2, 2), &wsadata) != 0) {
				TLS_FREE(ctx);
				return result;
			}
		#elif defined(TLS_S2N)
			if (!tls_s2n_init) {
				tls_s2n_init = 1;
				s2n_init();
			}
		#endif
	#endif // defined(TLS_WINDOWS) || defined(TLS_S2N)

	#if defined(TLS_WINDOWS) || defined(TLS_S2N)
		// Perform DNS lookup.
		struct addrinfo hints;
		TLS_MEMSET(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_PASSIVE;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;
		struct addrinfo* addri = NULL;
		if (getaddrinfo(hostname, sport, &hints, &addri) != 0) {
			freeaddrinfo(addri);
			TLS_FREE(ctx);
			return result;
		}

		// Create a TCP IPv4 socket.
		ctx->sock = socket(AF_INET, SOCK_STREAM, 0);
		if (ctx->sock == -1) {
			freeaddrinfo(addri);
			TLS_FREE(ctx);
			return result;
		}

		// Set non-blocking IO.
		{
			#ifdef TLS_WINDOWS
				DWORD non_blocking = 1;
				int res = ioctlsocket(ctx->sock, FIONBIO, &non_blocking);
			#else
				int flags = fcntl(ctx->sock, F_GETFL, 0);
				int res = fcntl(ctx->sock, F_SETFL, flags | O_NONBLOCK);
			#endif
			if (res != 0) {
				#ifdef TLS_WINDOWS
					closesocket(ctx->sock);
				#else
					close(ctx->sock);
				#endif
				freeaddrinfo(addri);
				TLS_FREE(ctx);
				return result;
			}
		}

		// Startup the TCP connection.
		if (connect(ctx->sock, addri->ai_addr, (int)addri->ai_addrlen) == -1) {
			#ifdef TLS_WINDOWS
				int error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) {
					freeaddrinfo(addri);
					closesocket(ctx->sock);
					TLS_FREE(ctx);
					return result;
				}
			#else
				if (errno != EWOULDBLOCK && errno != EINPROGRESS && errno != EAGAIN) {
					freeaddrinfo(addri);
					close(ctx->sock);
					TLS_FREE(ctx);
					return result;
				}
			#endif
		} else {
			freeaddrinfo(addri);
			addri = NULL;
		}

		ctx->state = TLS_STATE_PENDING;

		#ifdef TLS_WINDOWS
		// Initialize a credentials handle for Secure Channel.
		// This is needed for InitializeSecurityContextA.
		{
			SCHANNEL_CRED cred = { 0 };
			cred.dwVersion = SCHANNEL_CRED_VERSION;
			cred.dwFlags = SCH_USE_STRONG_CRYPTO          // Disable deprecated or otherwise weak algorithms (on as default).
						 | SCH_CRED_AUTO_CRED_VALIDATION  // Automatically validate server cert (on as default), as opposed to manual verify.
						 | SCH_CRED_NO_DEFAULT_CREDS;     // Client certs are not supported.
			cred.grbitEnabledProtocols = SP_PROT_TLS1_2;  // Specifically pick only TLS 1.2.

			if (AcquireCredentialsHandleA(NULL, (char*)UNISP_NAME_A, SECPKG_CRED_OUTBOUND, NULL, &cred, NULL, NULL, &ctx->handle, NULL) != SEC_E_OK)
			{
				closesocket(ctx->sock);
				TLS_FREE(ctx);
				return result;
			}
		}
		#elif defined(TLS_S2N)
			// Create connection and set our socket onto it. s2n wraps our socket -- actually
			// a pretty cool API design, which actually simplifies send/recv in our implementation.
			struct s2n_connection* connection = s2n_connection_new(S2N_CLIENT);
			if (!connection) {
				close(ctx->sock);
				TLS_FREE(ctx);
				return result;
			}
			if (s2n_connection_set_fd(connection, ctx->sock) < 0) {
				close(ctx->sock);
				TLS_FREE(ctx);
				return result;
			}
			ctx->connection = connection;

			// Disable client certs (not supported).
			s2n_connection_set_client_auth_type(connection, S2N_CERT_AUTH_NONE);

			// Make sure the cert hostname matches our expectation. If this isn't set here
			// the connection will fail to validate. We could also use `s2n_connection_set_verify_host_callback`
			// but this makes use of a built-in default callback (as per s2n docs). This was *not* at all
			// clear in the docs, and was figured out through painful trial + error.
			s2n_set_server_name(connection, hostname);

			// Turn off randomized side-channel blinding. This feature is useful for highly
			// secure servers, but for our simple client it's just annoying.
			s2n_connection_set_blinding(connection, S2N_SELF_SERVICE_BLINDING);
		#endif
	#endif // defined(TLS_WINDOWS) || defined(TLS_S2N)

	#ifdef TLS_APPLE
		// Used for syncing the packet queue.
		pthread_mutex_init(&ctx->q.lock, NULL);

		// Turn on TLS (default config).
		nw_endpoint_t endpoint = nw_endpoint_create_host(hostname, sport);
		nw_parameters_configure_protocol_block_t configure_tls = NW_PARAMETERS_DEFAULT_CONFIGURATION;
		nw_parameters_t parameters = nw_parameters_create_secure_tcp(configure_tls, NW_PARAMETERS_DEFAULT_CONFIGURATION);

		// Set ipv4.
		nw_protocol_stack_t protocol_stack = nw_parameters_copy_default_protocol_stack(parameters);
		nw_protocol_options_t ip_options = nw_protocol_stack_copy_internet_protocol(protocol_stack);
		nw_ip_options_set_version(ip_options, nw_ip_version_4);

		// Create actual connection object.
		ctx->connection = nw_connection_create(endpoint, parameters);
		nw_retain(ctx->connection);

		// Create an async queue for dispatching all of our connection's callbacks/blocks.
		ctx->dispatch = dispatch_queue_create("com.tls.internal_queue", DISPATCH_QUEUE_SERIAL);
		dispatch_retain(ctx->dispatch);

		// Various calls into Network.framework are asynchronous and use a queue to dispatch callbacks (blocks).
		nw_connection_set_queue(ctx->connection, ctx->dispatch);

		// Get notification of when the connection is ready.
		nw_connection_set_state_changed_handler(ctx->connection, ^(nw_connection_state_t state, nw_error_t error) {
			if (error) {
				int code = nw_error_get_error_code(error);
				nw_error_domain_t domain = nw_error_get_error_domain(error);
				if (domain == nw_error_domain_tls) {
					if (code == errSSLCertExpired) {
						ctx->state = TLS_STATE_CERTIFICATE_EXPIRED;
					} else if (code == errSSLNegotiation) {
						ctx->state = TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS;
					} else if (code == errSSLClientCertRequested) {
						ctx->state = TLS_STATE_SERVER_ASKED_FOR_CLIENT_CERTS;
					} else if (code == errSSLHostNameMismatch) {
						ctx->state = TLS_STATE_BAD_HOSTNAME;
					} else if (code == errSSLXCertChainInvalid || code == errSSLPeerUnknownCA) {
						ctx->state = TLS_STATE_CANNOT_VERIFY_CA_CHAIN;
					} else if (code == errSSLBadCert) {
						ctx->state = TLS_STATE_BAD_CERTIFICATE;
					} else {
						ctx->state = TLS_STATE_UNKNOWN_ERROR;
					}
					// More codes found at:
					// https://opensource.apple.com/source/Security/Security-55471/libsecurity_ssl/lib/SecureTransport.h.auto.html
				} else if (domain == nw_error_domain_dns) {
					if (code == -65554) { // Could not find what fucking header this code is defined within... *Shakes head at Tim Cook*
						ctx->state = TLS_STATE_BAD_HOSTNAME;
					} else {
						ctx->state = TLS_STATE_UNKNOWN_ERROR;
					}
				} else if (domain == TLS_STATE_INVALID_SOCKET) {
					ctx->state = TLS_STATE_INVALID_SOCKET;
				} else {
					ctx->state = TLS_STATE_UNKNOWN_ERROR;
				}
			} else {
				if (state == nw_connection_state_ready) {
					ctx->state = TLS_STATE_CONNECTED;
				} else if (state > nw_connection_state_ready) {
					ctx->state = TLS_STATE_DISCONNECTED;
				}
			}
		});

		// Asynchronously start the connection.
		nw_connection_start(ctx->connection);
		ctx->state = TLS_STATE_PENDING;

		// Accept incoming packets.
		tls_recv(ctx);

		nw_release(ip_options);
		nw_release(protocol_stack);
		nw_release(parameters);
		nw_release(endpoint);
	#endif

	result.id = (unsigned long long)ctx;
	return result;
}

TLS_State tls_process(TLS_Connection connection)
{
	TLS_Context* ctx = (TLS_Context*)connection.id;
	if (ctx->state < 0) {
		return ctx->state;
	} else if (ctx->state == TLS_STATE_PENDING) {
		#if defined(TLS_WINDOWS) || defined(TLS_S2N)
			// Wait for TCP to connect.
			if (ctx->tcp_connect_pending) {
				fd_set sockets_to_check;
				FD_ZERO(&sockets_to_check);
				FD_SET(ctx->sock, &sockets_to_check);
				struct timeval timeout; timeout.tv_sec = 0; timeout.tv_usec = 0;
				if (select((int)(ctx->sock + 1), NULL, &sockets_to_check, NULL, &timeout) == 1) {
					int opt = -1;
					socklen_t len = sizeof(opt);
					if (getsockopt(ctx->sock, SOL_SOCKET, SO_ERROR, (char*)(&opt), &len) >= 0 && opt == 0) {
						ctx->tcp_connect_pending = 0;
					}
				}
				if (ctx->tcp_connect_pending) {
					return ctx->state;
				}
			}
		#endif // defined(TLS_WINDOWS) || defined(TLS_S2N)

		#ifdef TLS_WINDOWS
			// TLS handshake algorithm.
			// 1. Call InitializeSecurityContext.
			//    The first call creates a security context.
			//    Subsequent calls update the security context.
			// 2. Check InitializeSecurityContext's return value.
			//    SEC_E_OK                     - Handshake completed, TLS tunnel ready to go.
			//    SEC_I_INCOMPLETE_CREDENTIALS - The server asked for client certs (not supported).
			//    SEC_I_CONTINUE_NEEDED        - Success, keep calling InitializeSecurityContext (and send).
			//    SEC_E_INCOMPLETE_MESSAGE     - Success, continue reading data from the server (recv).
			// 3. Otherwise an error may have been encountered. Set an error state and return.
			// 4. Read data from the server (recv).
	
			// 1. Call InitializeSecurityContext.
			if (ctx->first_call || ctx->received) {
				SecBuffer inbuffers[2] = { 0 };
				inbuffers[0].BufferType = SECBUFFER_TOKEN;
				inbuffers[0].pvBuffer = ctx->incoming;
				inbuffers[0].cbBuffer = ctx->received;
				inbuffers[1].BufferType = SECBUFFER_EMPTY;
	
				SecBuffer outbuffers[1] = { 0 };
				outbuffers[0].BufferType = SECBUFFER_TOKEN;
	
				SecBufferDesc indesc = { SECBUFFER_VERSION, TLS_ARRAYSIZE(inbuffers), inbuffers };
				SecBufferDesc outdesc = { SECBUFFER_VERSION, TLS_ARRAYSIZE(outbuffers), outbuffers };
	
				DWORD flags = ISC_REQ_USE_SUPPLIED_CREDS | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | 	ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM;
				SECURITY_STATUS sec = InitializeSecurityContextA(
					&ctx->handle,
					ctx->first_call ? NULL : &ctx->context,
					ctx->first_call ? (SEC_CHAR*)ctx->hostname : NULL,
					flags,
					0,
					0,
					ctx->first_call ? NULL : &indesc,
					0,
					ctx->first_call ? &ctx->context : NULL,
					&outdesc,
					&flags,
					NULL
				);
	
				// After the first call we are supposed to re-use the same context.
				ctx->first_call = 0;
	
				// Fetch incoming data.
				if (inbuffers[1].BufferType == SECBUFFER_EXTRA) {
					TLS_MEMMOVE(ctx->incoming, ctx->incoming + (ctx->received - inbuffers[1].cbBuffer), inbuffers[1].cbBuffer);
					ctx->received = inbuffers[1].cbBuffer;
				} else if (inbuffers[1].BufferType != SECBUFFER_MISSING) {
					ctx->received = 0;
				}
	
				// 2. Check InitializeSecurityContext's return value.
				if (sec == SEC_E_OK) {
					// Successfully completed handshake. TLS tunnel is now operational.
					QueryContextAttributes(&ctx->context, SECPKG_ATTR_STREAM_SIZES, &ctx->sizes);
					ctx->state = TLS_STATE_CONNECTED;
					return ctx->state;
				} else if (sec == SEC_I_INCOMPLETE_CREDENTIALS) {
					// Client certs are not supported.
					ctx->state = TLS_STATE_SERVER_ASKED_FOR_CLIENT_CERTS;
					return ctx->state;
				} else if (sec == SEC_I_CONTINUE_NEEDED) {
					// Continue sending data to the server.
					char* buffer = (char*)outbuffers[0].pvBuffer;
					int size = outbuffers[0].cbBuffer;
					while (size != 0) {
						int d = send(ctx->sock, buffer, size, 0);
						if (d <= 0) {
							break;
						}
						size -= d;
						buffer += d;
					}
					FreeContextBuffer(outbuffers[0].pvBuffer);
					if (size != 0) {
						// Somehow failed to send() data to server.
						ctx->state = TLS_STATE_UNKNOWN_ERROR;
						return ctx->state;
					}
				} else if (sec != SEC_E_INCOMPLETE_MESSAGE) {
					if (sec == SEC_E_CERT_EXPIRED) {
						ctx->state = TLS_STATE_CERTIFICATE_EXPIRED;
					} else if (sec == SEC_E_WRONG_PRINCIPAL) {
						ctx->state = TLS_STATE_BAD_HOSTNAME;
					} else if (sec == SEC_E_UNTRUSTED_ROOT) {
						ctx->state = TLS_STATE_CANNOT_VERIFY_CA_CHAIN;
					} else if (sec == SEC_E_ILLEGAL_MESSAGE || sec == SEC_E_ALGORITHM_MISMATCH) {
						ctx->state = TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS;
					} else {
						ctx->state = TLS_STATE_UNKNOWN_ERROR;
					}
					return ctx->state;
				} else {
					TLS_ASSERT(sec == SEC_E_INCOMPLETE_MESSAGE);
					// Need to read more bytes.
				}
			}
	
			if (ctx->received == sizeof(ctx->incoming)) {
				// Server is sending too much data instead of proper handshake?
				ctx->state = TLS_STATE_UNKNOWN_ERROR;
				return ctx->state;
			}
	
			// 4. Read data from the server (recv).
			tls_recv(ctx);
		#endif // TLS_WINDOWS

		#ifdef TLS_APPLE
			// Nothing needed here.
		#endif // TLS_APPLE

		#ifdef TLS_S2N
			// s2n wraps a file descriptor (our socket) and performs the entire
			// handshake for us. Pretty nice!
			s2n_blocked_status blocked = S2N_NOT_BLOCKED;
			s2n_errno = S2N_ERR_T_OK;
			if (s2n_negotiate(ctx->connection, &blocked) != S2N_SUCCESS) {
				s2n_error_type etype = (s2n_error_type)s2n_error_get_type(s2n_errno);
				if (etype == S2N_ERR_T_PROTO) {
					// For some unknown reason s2n doesn't expose their error constants, like at all.
					// So to avoid finding the correct header and including it, we can at least us
					// string comparisons, as that's the only way s2n has exposed error codes that aren't
					// a nightmare to hookup, and likely won't break as they add new error types.
					#define TLS_S2N_ERROR_MATCHES(X) (!TLS_STRCMP(s2n_strerror_name(s2n_errno), #X))
					if (TLS_S2N_ERROR_MATCHES(S2N_ERR_CERT_UNTRUSTED) ||
					    TLS_S2N_ERROR_MATCHES(S2N_ERR_CERT_REVOKED) ||
					    TLS_S2N_ERROR_MATCHES(S2N_ERR_CERT_TYPE_UNSUPPORTED) ||
					    TLS_S2N_ERROR_MATCHES(S2N_ERR_CERT_INVALID)) {
						ctx->state = TLS_STATE_BAD_CERTIFICATE;
					} else if (TLS_S2N_ERROR_MATCHES(S2N_ERR_CERT_EXPIRED)) {
						ctx->state = TLS_STATE_CERTIFICATE_EXPIRED;
					} else if (TLS_S2N_ERROR_MATCHES(S2N_ERR_NO_APPLICATION_PROTOCOL)) {
						ctx->state = TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS;
					}
				} else if (etype == S2N_ERR_T_IO) {
					ctx->state = TLS_STATE_INVALID_SOCKET;
				} else if (etype != S2N_ERR_T_BLOCKED) {
					ctx->state = TLS_STATE_UNKNOWN_ERROR;
				} else {
					// Continue calling s2n_negotiate...
				}
			} else {
				ctx->state = TLS_STATE_CONNECTED;
			}
		#endif // TLS_S2N
	} else if (ctx->state >= 0) {
		// Stall if the packet queue is full.
		if (ctx->q.count == TLS_PACKET_QUEUE_MAX_ENTRIES) {
			// User needs to call tls_read.
			return TLS_STATE_PACKET_QUEUE_FILLED;
		}

		// If any ciphertext data available then try to decrypt it.
		#ifdef TLS_WINDOWS
			// Read data from the TCP socket.
			tls_recv(ctx);
		
			if (ctx->received != 0) {
				SecBuffer buffers[4];
				TLS_ASSERT(ctx->sizes.cBuffers == TLS_ARRAYSIZE(buffers));

				buffers[0].BufferType = SECBUFFER_DATA;
				buffers[0].pvBuffer = ctx->incoming;
				buffers[0].cbBuffer = ctx->received;
				buffers[1].BufferType = SECBUFFER_EMPTY;
				buffers[2].BufferType = SECBUFFER_EMPTY;
				buffers[3].BufferType = SECBUFFER_EMPTY;

				SecBufferDesc desc = { SECBUFFER_VERSION, TLS_ARRAYSIZE(buffers), buffers };

				SECURITY_STATUS sec = DecryptMessage(&ctx->context, &desc, 0, NULL);
				if (sec == SEC_E_OK) {
					// Successfully decrypted some data.
					TLS_ASSERT(buffers[0].BufferType == SECBUFFER_STREAM_HEADER);
					TLS_ASSERT(buffers[1].BufferType == SECBUFFER_DATA);
					TLS_ASSERT(buffers[2].BufferType == SECBUFFER_STREAM_TRAILER);

					ctx->decrypted = (char*)buffers[1].pvBuffer;
					ctx->available = buffers[1].cbBuffer;
					ctx->used = ctx->received - (buffers[3].BufferType == SECBUFFER_EXTRA ? buffers[3].cbBuffer : 0);
				} else if (sec == SEC_I_CONTEXT_EXPIRED) {
					// Server closed TLS connection (but socket is still open).
					ctx->state = TLS_STATE_DISCONNECTED;
					return ctx->state;
				} else if (sec == SEC_I_RENEGOTIATE) {
					// Server wants to renegotiate TLS connection, not implemented here.
					ctx->state = TLS_STATE_UNKNOWN_ERROR;
					return ctx->state;
				} else if (sec != SEC_E_INCOMPLETE_MESSAGE) {
					ctx->state = TLS_STATE_UNKNOWN_ERROR;
					return ctx->state;
				} else {
					TLS_ASSERT(sec == SEC_E_INCOMPLETE_MESSAGE);
					// More data needs to be read.
				}
			}

			// Copy out a decrypted buffer into an output packet.
			if (ctx->decrypted) {
				TLS_ASSERT(ctx->decrypted);
				int size = ctx->available;
				void* data = TLS_MALLOC(size);
				TLS_MEMCPY(data, ctx->decrypted, size);

				// All decrypted data is used, remove ciphertext from incoming buffer so next time it starts from beginning.
				TLS_MEMMOVE(ctx->incoming, ctx->incoming + ctx->used, ctx->received - ctx->used);
				ctx->received -= ctx->used;
				ctx->used = 0;
				ctx->available = 0;
				ctx->decrypted = NULL;

				tls_packet_queue_push(&ctx->q, data, size);

				if (ctx->state == TLS_STATE_DISCONNECTED) {
					ctx->state = TLS_STATE_DISCONNECTED_BUT_PACKETS_STILL_REMAIN;
				}
			}
		#endif // TLS_WINDOWS

		#ifdef TLS_APPLE
			// Nothing on Apple.
			// Reads are setup via chained callbacks upon connection starting.
		#endif

		#ifdef TLS_S2N
			tls_recv(ctx);

			// We don't need to do anything special for encryption (unlike SChannel)
			// since s2n wraps our file descriptor (socket) for us.

			// Push our incoming buffer into packet queue.
			if (ctx->received) {
				int size = ctx->received;
				void* data = TLS_MALLOC(size);
				TLS_MEMCPY(data, ctx->incoming, size);
				ctx->received = 0;

				tls_packet_queue_push(&ctx->q, data, size);

				if (ctx->state == TLS_STATE_DISCONNECTED) {
					ctx->state = TLS_STATE_DISCONNECTED_BUT_PACKETS_STILL_REMAIN;
				}
			}
		#endif // TLS_S2N
	}

	return ctx->state;
}

const char* tls_state_string(TLS_State state)
{
	switch (state) {
	case TLS_STATE_BAD_CERTIFICATE                       : return "TLS_STATE_BAD_CERTIFICATE";
	case TLS_STATE_SERVER_ASKED_FOR_CLIENT_CERTS         : return "TLS_STATE_SERVER_ASKED_FOR_CLIENT_CERTS";
	case TLS_STATE_CERTIFICATE_EXPIRED                   : return "TLS_STATE_CERTIFICATE_EXPIRED";
	case TLS_STATE_BAD_HOSTNAME                          : return "TLS_STATE_BAD_HOSTNAME";
	case TLS_STATE_CANNOT_VERIFY_CA_CHAIN                : return "TLS_STATE_CANNOT_VERIFY_CA_CHAIN";
	case TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS     : return "TLS_STATE_NO_MATCHING_ENCRYPTION_ALGORITHMS";
	case TLS_STATE_INVALID_SOCKET                        : return "TLS_STATE_INVALID_SOCKET";
	case TLS_STATE_UNKNOWN_ERROR                         : return "TLS_STATE_UNKNOWN_ERROR";
	case TLS_STATE_DISCONNECTED                          : return "TLS_STATE_DISCONNECTED";
	case TLS_STATE_DISCONNECTED_BUT_PACKETS_STILL_REMAIN : return "TLS_STATE_DISCONNECTED_BUT_PACKETS_STILL_REMAIN";
	case TLS_STATE_PENDING                               : return "TLS_STATE_PENDING";
	case TLS_STATE_CONNECTED                             : return "TLS_STATE_CONNECTED";
	case TLS_STATE_PACKET_QUEUE_FILLED                   : return "TLS_STATE_PACKET_QUEUE_FILLED";
	}
	return NULL;
}

void tls_disconnect(TLS_Connection connection)
{
	TLS_Context* ctx = (TLS_Context*)connection.id;

	#ifdef _WIN32
		if (ctx->state >= 0) {
			DWORD type = SCHANNEL_SHUTDOWN;

			SecBuffer inbuffers[1];
			inbuffers[0].BufferType = SECBUFFER_TOKEN;
			inbuffers[0].pvBuffer = &type;
			inbuffers[0].cbBuffer = sizeof(type);

			SecBufferDesc indesc = { SECBUFFER_VERSION, TLS_ARRAYSIZE(inbuffers), inbuffers };
			ApplyControlToken(&ctx->context, &indesc);

			SecBuffer outbuffers[1];
			outbuffers[0].BufferType = SECBUFFER_TOKEN;

			SecBufferDesc outdesc = { SECBUFFER_VERSION, TLS_ARRAYSIZE(outbuffers), outbuffers };
			DWORD flags = ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_CONFIDENTIALITY | ISC_REQ_REPLAY_DETECT | ISC_REQ_SEQUENCE_DETECT | ISC_REQ_STREAM;
			if (InitializeSecurityContextA(&ctx->handle, &ctx->context, NULL, flags, 0, 0, &outdesc, 0, NULL, &outdesc, &flags, NULL) == SEC_E_OK) {
				char* buffer = (char*)outbuffers[0].pvBuffer;
				int size = outbuffers[0].cbBuffer;
				while (size != 0) {
					int d = send(ctx->sock, buffer, size, 0);
					buffer += d;
					size -= d;
				}
				FreeContextBuffer(outbuffers[0].pvBuffer);
			}
			shutdown(ctx->sock, SD_BOTH);
		}
		DeleteSecurityContext(&ctx->context);
		FreeCredentialsHandle(&ctx->handle);
		closesocket(ctx->sock);
	#endif

	#ifdef TLS_APPLE
		dispatch_release(ctx->dispatch);
		nw_release(ctx->connection);
		pthread_mutex_destroy(&ctx->q.lock);
	#endif

	#ifdef TLS_S2N
		// This is supposed to be in a loop, but, we're not going to do that as
		// it's too much effort and doesn't play nicely with the API design here.
		s2n_blocked_status blocked = S2N_NOT_BLOCKED;
		s2n_shutdown(ctx->connection, &blocked);

		s2n_connection_free(ctx->connection);
	#endif

	while (ctx->q.count) {
		void* packet;
		int size;
		tls_packet_queue_pop(&ctx->q, &packet, &size);
		TLS_FREE(packet);
	}
	TLS_FREE(ctx);
}

int tls_read(TLS_Connection connection, void* data, int size)
{
	TLS_Context* ctx = (TLS_Context*)connection.id;
	if (ctx->state < 0) {
		return -1;
	}

	if (!ctx->packet) {
		tls_packet_queue_pop(&ctx->q, &ctx->packet, &ctx->packet_size);
	}

	if (ctx->state == TLS_STATE_DISCONNECTED_BUT_PACKETS_STILL_REMAIN && ctx->q.count == 0) {
		ctx->state = TLS_STATE_DISCONNECTED;
	}

	if (ctx->packet) {
		if (size >= ctx->packet_size) {
			// Copy out the entire packet.
			TLS_MEMCPY(data, ctx->packet, ctx->packet_size);
			int result = ctx->packet_size;
			TLS_FREE(ctx->packet);
			ctx->packet = NULL;
			ctx->packet_size = 0;
			return result;
		} else {
			// Copy only a portion of the packet out, since the user buffer is small.
			// Shift unread bytes all the way to the front of the packet buffer.
			TLS_MEMCPY(data, ctx->packet, size);
			TLS_MEMMOVE(ctx->packet, (void*)((uintptr_t)ctx->packet + size), ctx->packet_size - size);
			ctx->packet_size -= size;
			TLS_ASSERT(ctx->packet_size > 0);
			return size;
		}
	} else {
		return 0;
	}
}

int tls_send(TLS_Connection connection, const void* data, int size)
{
	TLS_Context* ctx = (TLS_Context*)connection.id;
	if (ctx->state <= 0) return -1;

	#ifdef TLS_WINDOWS
		while (size != 0) {
			int use = TLS_MIN(size, (int)ctx->sizes.cbMaximumMessage);

			char wbuffer[TLS_MAX_PACKET_SIZE];
			TLS_ASSERT(ctx->sizes.cbHeader + ctx->sizes.cbMaximumMessage + ctx->sizes.cbTrailer <= sizeof(wbuffer));

			SecBuffer buffers[3];
			buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
			buffers[0].pvBuffer = wbuffer;
			buffers[0].cbBuffer = ctx->sizes.cbHeader;
			buffers[1].BufferType = SECBUFFER_DATA;
			buffers[1].pvBuffer = wbuffer + ctx->sizes.cbHeader;
			buffers[1].cbBuffer = use;
			buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
			buffers[2].pvBuffer = wbuffer + ctx->sizes.cbHeader + use;
			buffers[2].cbBuffer = ctx->sizes.cbTrailer;

			TLS_MEMCPY(buffers[1].pvBuffer, data, use);

			SecBufferDesc desc = { SECBUFFER_VERSION, TLS_ARRAYSIZE(buffers), buffers };
			SECURITY_STATUS sec = EncryptMessage(&ctx->context, 0, &desc, 0);
			if (sec != SEC_E_OK) {
				// This should not happen, but just in case check it.
				ctx->state = TLS_STATE_UNKNOWN_ERROR;
				return -1;
			}

			int total = buffers[0].cbBuffer + buffers[1].cbBuffer + buffers[2].cbBuffer;
			int sent = 0;
			while (sent != total) {
				int d = send(ctx->sock, wbuffer + sent, total - sent, 0);
				if (d <= 0) {
					#ifdef _WIN32
						int error = WSAGetLastError();
						if (error != WSAEWOULDBLOCK && error != WSAEINPROGRESS) {
							// Error sending data to socket, or server disconnected.
							ctx->state = TLS_STATE_UNKNOWN_ERROR;
							return -1;
						}
					#else
						if (errno != EAGAIN) {
							// Error sending data to socket, or server disconnected.
							ctx->state = TLS_STATE_UNKNOWN_ERROR;
							return -1;
						}
					#endif
				}
				sent += d;
			}

			data = (const void*)((uintptr_t)data + use);
			size -= use;
		}
	#endif // TLS_WINDOWS

	#ifdef TLS_APPLE
		// Again, so over-engineered. We just need to send a blob of bytes...
		void* copy = TLS_MALLOC(size);
		TLS_MEMCPY(copy, data, size);
		dispatch_data_t dispatch_data = dispatch_data_create(copy, size, ctx->dispatch, DISPATCH_DATA_DESTRUCTOR_FREE);
		nw_connection_send(ctx->connection, dispatch_data, NW_CONNECTION_DEFAULT_MESSAGE_CONTEXT, true, ^(nw_error_t error) {
			if (error) {
				int code = nw_error_get_error_code(error);
				(void)code;
				ctx->state = TLS_STATE_UNKNOWN_ERROR;
			}
		});
	#endif // TLS_APPLE

	#ifdef TLS_S2N
		s2n_blocked_status blocked = S2N_NOT_BLOCKED;
		int bytes_written = 0;
		while (bytes_written < size) {
			int w = s2n_send(ctx->connection, (const void*)((uintptr_t)data + bytes_written), size - bytes_written, &blocked);
			if (w >= 0) {
				bytes_written += w;
			} else if (s2n_error_get_type(s2n_errno) != S2N_ERR_T_BLOCKED) {
				// Error sending data to socket, or server disconnected.
				ctx->state = TLS_STATE_UNKNOWN_ERROR;
				return -1;
			}
		}
	#endif // TLS_S2N

	return 0;
}

#endif // CUTE_TLS_IMPLEMENTATION_ONCE
#endif // CUTE_TLS_IMPLEMENTATION

/*
	------------------------------------------------------------------------------
	This software is available under 2 licenses - you may choose the one you like.
	------------------------------------------------------------------------------
	ALTERNATIVE A - zlib license
	Copyright (c) 2023 Randy Gaul https://randygaul.github.io/
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
