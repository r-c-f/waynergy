#include "clip.h"
#include "wayland.h"

extern uSynergyContext synContext;
extern char **environ;

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

/* spawn wl-paste watchers */
bool clipSpawnMonitors(void)
{
	clipMonitorPath[0] = osGetRuntimePath("swaynergy-cb-fifo");
	clipMonitorPath[1] = osGetRuntimePath("swaynergy-p-fifo");
	struct sockaddr_un sa[2] = {
		{
			.sun_family = AF_UNIX,
		},
		{
			.sun_family = AF_UNIX,
		}
	};
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
		unlink(clipMonitorPath[i]);
		errno = 0;
		if ((clipMonitorFd[i] = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
			logErr("Socket call failed: %s", strerror(errno));
			return false;
		}
		strncpy(sa.sun_path, clipMonitorPath[i], sizeof(sa.sun_path));
		if (bind(clipMonitorFd[i], (struct sockaddr *)(sa + i), sizeof(sa[i])) == 1) {
			logErr("Bind error: %s", strerror(errno));
			return false;
		}
		while (
		/* now we spawn */
		errno = 0;
		if (posix_spawnp(clipMonitorPid + i,"wl-paste",NULL,NULL,argv[i],environ)) {
			logErr("Spawn failed: %s", strerror(errno));
			clipMonitorPid[i] = -1;
			return false;
		}
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
		if (!read_full(pfd->fd, &len, sizeof(len), 0)) {
			logErr("Clip monitor poll proc failed");
			return;
		}
		char buf[len];
		if (!read_full(pfd->fd, buf, len, 0)) {
			logErr("Clipboard monitor could not read data: %s", strerror(errno));
			goto error;
		}
		uSynergyUpdateClipBuf(&synContext, id, len, buf);
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

