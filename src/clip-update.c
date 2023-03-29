#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ssb.h"
#include "xmem.h"
#include "fdio_full.h"

int main(int argc, char **argv)
{
	char *path = argv[2];
	char cid = argv[1][0];
	struct ssb s = {0};
	struct sockaddr_un sa = {0};
	int sock;
	if (!ssb_readfile(&s, stdin)) {
		return EXIT_FAILURE;
	}
	/* write out the clipboard sequence -- char for ID, size_t for size, then data */
	strncpy(sa.sun_path, path, sizeof(sa.sun_path) - 1);
	sa.sun_family = AF_UNIX;
	if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		return EXIT_FAILURE;
	}
	if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
		return EXIT_FAILURE;
	}
	if (!write_full(sock, &cid, 1, 0)) {
		return EXIT_FAILURE;
	}
	if (!write_full(sock, &(s.pos), sizeof(s.pos), 0)) {
		return EXIT_FAILURE;
	}
	if (!write_full(sock, s.buf, s.pos, 0)) {
		return EXIT_FAILURE;
	}
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return EXIT_SUCCESS;
}

