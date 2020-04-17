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
	bool ret = true;
	pid_t pid;
	int status;
	sigset_t sigset, sigsetold;
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

	sigWaitSIGCHLD(true);

	for (int i = 0; i < 2; ++i) {
		if (posix_spawnp(&pid, argv[i][0], NULL, NULL, argv[i], environ)) {
			ret = false;
			goto done;
		}
		if (waitpid(pid, &status, 0) != pid) {
			ret = false;
			goto done;
		}
		if (status) {
			ret = false;
			goto done;
		}
		logDbg("Found %s", argv[i][0]);
	}
done:
	sigWaitSIGCHLD(false);
	return ret;
}

/* set up sockets */
bool clipSetupSockets()
{
	strncpy(clipMonitorAddr.sun_path, osGetRuntimePath("waynergy-clip-sock"), sizeof(clipMonitorAddr.sun_path));
	unlink(clipMonitorAddr.sun_path);
	clipMonitorAddr.sun_family = AF_UNIX;
	if ((clipMonitorFd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
		logPErr("socket");
		return false;
	}
	if (bind(clipMonitorFd, (struct sockaddr *)&clipMonitorAddr, sizeof(clipMonitorAddr)) == -1) {
		logPErr("bind");
		return false;
	}
	if (listen(clipMonitorFd, 8) == -1) {
		logPErr("listen");
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
	char *argv[] = {
			"waynergy-clipmon",
			clipMonitorAddr.sun_path,
			NULL
	};
	/* kill the other crap on our socket */
	char *killcmd;
	xasprintf(&killcmd, "pkill -f 'waynergy-clipmon.*%s'", clipMonitorAddr.sun_path);
	system(killcmd);
	free(killcmd);
	xasprintf(&killcmd, "pkill -f 'waynergy-clip-update.%s'", clipMonitorAddr.sun_path);
	system(killcmd);
	free(killcmd);
	if (posix_spawnp(clipMonitorPid,argv[0],NULL,NULL,argv,environ)) {
		logPErr("spawn");
		return false;
	}
	return true;
}

/* process poll data */
void clipMonitorPollProc(struct pollfd *pfd)
{
	size_t len;
	char seq;
	char c_id;
	size_t mime_len;
	enum uSynergyClipboardId id;
	if (pfd->revents & POLLIN) {
		if (pfd->fd == clipMonitorFd) {
			for (int i = POLLFD_CLIP_UPDATER; i < POLLFD_COUNT; ++i) {
				if (netPollFd[i].fd == -1) {
					logDbg("Accepting");
					netPollFd[i].fd = accept(clipMonitorFd, NULL, NULL);
					if (netPollFd[i].fd == -1) {
						logPErr("accept");
					}
					return;
				}
			}
			logErr("No free updater file descriptors -- doing nothing");
		} else {
			if (!read_full(pfd->fd, &seq, 1, 0)) {
				logPErr("Could not read clipboard offer sequence");
				goto done;
			}	
			if (!read_full(pfd->fd, &c_id, 1, 0)) {
				logPErr("Could not read clipboard ID");
				goto done;
			}
			if (!read_full(pfd->fd, &mime_len, sizeof(mime_len), 0)) {
				logPErr("Could not read MIME type length");
				goto done;
			}
			char *mime = xmalloc(mime_len + 1);
			if (!read_full(pfd->fd, mime, mime_len, 0)) {
				logPErr("could not read mime type");
				goto done;
			}
			mime[mime_len] = 0;
			if (!read_full(pfd->fd, &len, sizeof(len), 0)) {
				logPErr("Could not read clipboard data length");
				goto done;
			}
			char *buf = xmalloc(len);
			if (!read_full(pfd->fd, buf, len, 0)) {
				logPErr("Could not read clipboard data");
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
			logDbg("Clipboard data read for %c, sequence %c, mime %s: %zd bytes", c_id, seq, mime, len);
			if (!strcmp(mime, "text/plain")) {
				uSynergyUpdateClipBuf(&synContext, id , len, buf);
			}else {
				logDbg("Ignoring unknown mimetype %s", mime);
			}
done:
			shutdown(pfd->fd, SHUT_RDWR);
			close(pfd->fd);
			pfd->fd = -1;
			free(buf);
		}
	}
	return;
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
	posix_spawn_file_actions_adddup2(&fa, STDIN_FILENO, fd[0]);
	posix_spawn_file_actions_addclose(&fa, fd[1]);
   	/* now we can spawn */
	errno = 0;
        if (posix_spawnp(&pid, "wl-copy", &fa, NULL, argv[id], environ)) {
		logPErr("wl-copy spawn");
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

