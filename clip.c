#include "clip.h"
#include "wayland.h"
#include "log.h"

extern uSynergyContext synContext;
extern struct wlContext wlContext;

/* process data for clipboard */
void clipPollProc(struct pollfd *pfd)
{
	/* get the clipboard buffer for the current file descriptor */
	struct wlSelectionBuffer *buf = NULL;
	for (int sel = 0; sel < WL_SELECTION_MAX; ++sel) {
		for (int form = 0; form < WL_SELECTION_FORMAT_MAX; ++form) {
			if (wlContext.data_buffer[sel][form].offer_fd == pfd->fd) {
				buf = &(wlContext.data_buffer[sel][form]);
				goto found;
			}
		}
	}
	logErr("File descriptor not found in selection buffers");
	return;
found:
	if (pfd.revents & POLLIN) {
		ssize_t read_count;
		read_count = read(buf->offer_fd, buf->data + buf->pos, buf->alloc - buf->pos - 1);
		if (read_count < 1) {
			logErr("Read returned %d", read_count);
			return;
		}
		time->last_active = time(NULL);
		if (buf->alloc - buf->pos <= 2) {
			buf->data = xrealloc(buf->data, buf->alloc *= 2);
		}
	}
	if (time->last_active
	






/* set wayland clipboard with wl-copy */
bool clipWlCopy(enum uSynergyClipboardId id, const unsigned char *data, size_t len)
{
	pid_t pid;
	/* create the pipe we will use to communicate with it */
	int fd[2];
	errno = 0;
	if (pipe(fd) == -1) {
		perror("pipe");
		return false;
	}
        /* now we can spawn and shit */
	errno = 0;
        if ((pid = fork()) == -1) {
		perror("fork");
		return false;
	} else if (!pid) {
		close(fd[1]);
		/* we want stdin from the pipe */
		errno = 0;
		if (dup2(fd[0], STDIN_FILENO) != STDIN_FILENO) {
			perror("dup2");
		}
                errno = 0;
                if (id) {
                        execlp("wl-copy", "wl-copy", "-f", "--primary", NULL);
                } else {
                        execlp("wl-copy", "wl-copy", "-f", NULL);
                }
		perror("execlp");
		abort();
        }
	close(fd[0]);
	/* write to child process */
	write_full(fd[1], data, len);
	close(fd[1]);
	return true;
}

