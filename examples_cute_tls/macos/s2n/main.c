#define CUTE_TLS_IMPLEMENTATION
#include "../../../cute_tls.h"

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
			printf("Failed reading bytes, %s.\n", tls_state_string(state));
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
