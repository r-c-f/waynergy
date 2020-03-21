#include "clip.h"
#include "wayland.h"
#include "net.h"

extern uSynergyContext synContext;
extern char **environ;

int clipMonitorFd;
struct sockaddr_un clipMonitorAddr;
pid_t clipMonitorPid[2];

/* check for wl-clipboard's presence */
bool clipHaveWlClipboard(void)
{
	pid_t pid;
	int ret;
	char *argv_0[] = {
		"wl-paste",
		"-v",
		NULL
	};
	char *argv_1[] = {
		"wl-copy",
		"-v",
		NULL
	};
	char **argv[] = {argv_0, argv_1};
	for (int i = 0; i < 2; ++i) {
		if (posix_spawnp(&pid, argv[i][0], NULL, NULL, argv[i], environ))
			return false;
		if (waitpid(pid, &ret, 0) != pid)
			return false;
		if (ret)
			return false;
	}
	return true;
}

/* set up sockets */
bool clipSetupSockets()
{
	strncpy(clipMonitorAddr.sun_path, osGetRuntimePath("swaynergy-clip-sock"), sizeof(clipMonitorAddr.sun_path));
	unlink(clipMonitorAddr.sun_path);
	clipMonitorAddr.sun_family = AF_UNIX;
	if ((clipMonitorFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		logErr("Socket call failed: %s", strerror(errno));
		return false;
	}
	if (bind(clipMonitorFd, (struct sockaddr *)&clipMonitorAddr, sizeof(clipMonitorAddr)) == -1) {
		logErr("Bind call failed: %s", strerror(errno));
		return false;
	}
	if (listen(clipMonitorFd, 8) == -1) {
		logErr("Could not listen: %s", strerror(errno));
		return false;
	}
	for (int i = POLLFD_CLIP_UPDATER; i < POLLFD_COUNT; ++i) {
		netPollFd[i].fd = -1;
	}
	return true;
}

/* spawn wl-paste watchers */
bool clipSpawnMonitors(void)
{
	char *argv_0[] = {
			"wl-paste",
			"-n",
			"-w",
			"swaynergy-clip-update",
			"c",
			clipMonitorAddr.sun_path,
			NULL
	};
	char *argv_1[] = {
			"wl-paste",
			"-n",
			"--primary",
			"-w",
			"swaynergy-clip-update",
			"p",
			clipMonitorAddr.sun_path,
			NULL
	};
	char **argv[] = { argv_0, argv_1 };
	for (int i = 0; i < 2; ++i) {
		if (posix_spawnp(clipMonitorPid + i,"wl-paste",NULL,NULL,argv[i],environ)) {
			logErr("Spawn failed: %s", strerror(errno));
			return false;
		}
	}
	return true;
}

/* process poll data */
void clipMonitorPollProc(struct pollfd *pfd)
{
	size_t len;
	char c_id;
	enum uSynergyClipboardId id;
	if (pfd->revents & POLLIN) {
		if (pfd->fd == clipMonitorFd) {
			for (int i = POLLFD_CLIP_UPDATER; i < POLLFD_COUNT; ++i) {
				if (netPollFd[i].fd == -1) {
					logDbg("Accepting");
					netPollFd[i].fd = accept(clipMonitorFd, NULL, NULL);
					if (netPollFd[i].fd == -1) {
						logErr("Could not accept connection: %s", strerror(errno));
					}
					return;
				}
			}
			logErr("No free updater file descriptors -- doing nothing");
		} else {
			if (!read_full(pfd->fd, &c_id, 1, 0)) {
				logErr("Could not read clipboard ID: %s", strerror(errno));
				goto done;
			}
			if (!read_full(pfd->fd, &len, sizeof(len), 0)) {
				logErr("Could not read clipboard data length: %s", strerror(errno));
				goto done;
			}
			char *buf = xmalloc(len);
			if (!read_full(pfd->fd, buf, len, 0)) {
				logErr("Could not read clipboard data: %s", strerror(errno));
				goto done;
			}
			if (c_id == 'p') {
				id = SYNERGY_CLIPBOARD_SELECTION;
			} else if (c_id == 'c') {
				id = SYNERGY_CLIPBOARD_CLIPBOARD;
			} else {
				logErr("Unknown clipboard ID %c", c_id);
				goto done;
			}
			logDbg("Clipboard data read for %c: %zd bytes", c_id, len);
			uSynergyUpdateClipBuf(&synContext, id , len, buf);
done:
			shutdown(pfd->fd, SHUT_RDWR);
			close(pfd->fd);
			pfd->fd = -1;
			free(buf);
		}
	}
	return;
error:
	/* instead of aborting, or cleaning up gracefully, just restart */
	kill(getpid(), SIGUSR1);
}


/* set wayland clipboard with wl-copy */
bool clipWlCopy(enum uSynergyClipboardId id, const unsigned char *data, size_t len)
{
	pid_t pid;
	posix_spawn_file_actions_t fa;
	char *argv_0[] = {
			"wl-copy",
			"-f",
			NULL};

	char *argv_1[] = {
			"wl-copy",
			"-f",
			"--primary",
			NULL
		};
	char **argv[] = {argv_0, argv_1};
	/* create the pipe we will use to communicate with it */
	int fd[2];
	errno = 0;
	if (pipe(fd) == -1) {
		perror("pipe");
		return false;
	}
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, fd[0], STDIN_FILENO);
	posix_spawn_file_actions_addclose(&fa, fd[1]);
   	/* now we can spawn */
	errno = 0;
        if (posix_spawnp(&pid, "wl-copy", &fa, NULL, argv[id], environ)) {
		logErr("wl-copy spawn failed: %s", strerror(errno));
		close(fd[0]);
		close(fd[1]);
		return false;
	}
	close(fd[0]);
	/* write to child process */
	write_full(fd[1], data, len, 0);
	close(fd[1]);
	return true;
}

