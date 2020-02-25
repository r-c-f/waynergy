#include "clip.h"

int clipMonitorFd[2];
char *clipMonitorPath[2];
pid_t clipMonitorPid[2];

extern uSynergyContext synContext;

/* spawn wl-paste watchers */
bool clipSpawnMonitors(void)
{
	pid_t pid;
	clipMonitorPath[0] = osGetRuntimePath("swaynergy-cb-fifo");
	clipMonitorPath[1] = osGetRuntimePath("swaynergy-p-fifo");
	char *argv_0[] = {
			"wl-paste",
			"-n",
			"-w",
			"swaynergy-clip-update",
			clipMonitorPath[0],
			NULL
	};
	char *argv_1[] = {
			"wl-paste",
			"-n",
			"--primary", 
			"-w",
			"swaynergy-clip-update",
			clipMonitorPath[1],
			NULL
	};
	char **argv[] = { argv_0, argv_1 };
	for (int i = 0; i < 2; ++i) {
		/*first set up FIFO*/
		unlink(clipMonitorPath[i]);
		errno = 0;
		if (mkfifo(clipMonitorPath[i], 0600))
			return false;
		errno = 0;
		if ((clipMonitorFd[i] = open(clipMonitorPath[i], O_RDWR | O_CLOEXEC)) == -1)
			return false;
		/* now we spawn */
		errno = 0;
		if ((pid = fork()) == -1) {
			perror("fork");
			return false;
		} else if (!pid) {
			errno = 0;
			execvp("wl-paste", argv[i]);
			perror("execvp");
			/* we should never arrive here */
			abort();
		}
		clipMonitorPid[i] = pid;
	}
	return true;
}

/* get clipboard ID from fd */
enum uSynergyClipboardId clipIdFromFd(int fd)
{
	for (int i = 0; i < 2; ++i) {
		if (clipMonitorFd[i] == fd)
			return i;
	}
	return -1;
}

/* process poll data */
void clipMonitorPollProc(struct pollfd *pfd)
{
	size_t len;
	int id = clipIdFromFd(pfd->fd);
	if (id < 0)
		return;
	if (pfd->revents & POLLIN) {
		if (!read_full(pfd->fd, &len, sizeof(len))) {
			abort();
			return;
		}
		char buf[len];
		if (!read_full(pfd->fd, buf, len)) {
			abort();
		}
		uSynergyUpdateClipBuf(&synContext, id, len, buf);
	}
}


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

