#include "clip.h"
#include "wayland.h"


extern uSynergyContext synContext;
extern struct wlContext wlContext;

/* process data



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

