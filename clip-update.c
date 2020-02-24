#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include "xmem.h"
#include "clip.h"

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
int clipWriteToFifo(char *path)
{
	char *buf;
	FILE *fifo;
	size_t len = 4000;
	size_t pos = 0;
	buf = xmalloc(len);
	if (!(fifo = fopen(path, "w")))
		return EXIT_FAILURE;
	if (!buf_append_file(&buf, &len, &pos, stdin))
		return EXIT_FAILURE;
	/* write out the clipboard sequence -- size_t for size, then data */
	lockf(fileno(fifo), F_LOCK, 0);
	/* NOTE: if we can't write consistent data, never releasing the lock is
	 * actually what we want, to avoid crashing the main process */ 
	if (fwrite(&pos, sizeof(pos), 1, fifo) != 1)
		return EXIT_FAILURE;
	if (fwrite(buf, pos, 1, fifo) != 1)
		return EXIT_FAILURE;
	lockf(fileno(fifo), F_ULOCK, 0);
	fclose(fifo);
	return EXIT_SUCCESS;
}

