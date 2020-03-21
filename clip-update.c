#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "xmem.h"
#include "clip.h"
#include "net.h"
#include "fdio_full.h"

/* read file into a buffer, resizing as needed */
static bool buf_append_file(char **buf, size_t *len, size_t *pos, FILE *f)
{
        size_t read_count;
        while ((read_count = fread(*buf + *pos, 1, *len - *pos - 1, f))) {
                *pos += read_count;
                if (*len - *pos <= 2) {
                        *buf = xrealloc(*buf, *len *= 2);
                }
        }
        return true;
}
int clipWriteToSocket(char *path, char cid)
{
	char *buf;
	size_t len = 4000;
	size_t pos = 0;
	struct sockaddr_un sa = {0};
	int sock;
	buf = xmalloc(len);
	if (!buf_append_file(&buf, &len, &pos, stdin))
		return EXIT_FAILURE;
	/* write out the clipboard sequence -- char for ID, size_t for size, then data */
	strncpy(sa.sun_path, path, sizeof(sa.sun_path));
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
	if (!write_full(sock, &pos, sizeof(pos), 0)) {
		return EXIT_FAILURE;
	}
	if (!write_full(sock, buf, pos, 0)) {
		return EXIT_FAILURE;
	}
	shutdown(sock, SHUT_RDWR);
	close(sock);
	return EXIT_SUCCESS;
}

