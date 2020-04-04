#include "clip.h"
#include "wayland.h"
#include "net.h"
#include "img.h"

extern uSynergyContext synContext;
extern char **environ;

int clipMonitorFd;
struct sockaddr_un clipMonitorAddr;
size_t clipMonitorCount;
struct clipMonitor *clipMonitor;


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
	strncpy(clipMonitorAddr.sun_path, osGetRuntimePath("swaynergy-clip-sock"), sizeof(clipMonitorAddr.sun_path));
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



/*spawn wl-paste watchers
 *
 * */
bool clipSpawnMonitors(char *mime, enum uSynergyClipboardFormat fmt, ...)
{
	char *fmt_str[] = {"0","1","2"};
	va_list ap;
	va_start(ap, fmt);
	for (int f = 0;;++f) {
		/* only b other with va_list if we're past the first args */
		if (f) {
			mime = va_arg(ap, char*);
			if (!mime)
				break;
			fmt = va_arg(ap, enum uSynergyClipboardFormat);
		}
		char *clip_mid = NULL;
		char *sel_mid = NULL;
		xasprintf(&clip_mid, "%d", f * 2);
		xasprintf(&sel_mid, "%d", (f * 2) + 1);
		char *argv_clip[] = {
				"wl-paste",
				"-n",
				"-t",
				mime,
				"-w",
				"swaynergy-clip-update",
				clipMonitorAddr.sun_path,
				clip_mid,
				NULL
		};
		char *argv_sel[] = {
				"wl-paste",
				"-n",
				"-t",
				mime,
				"--primary",
				"-w",
				"swaynergy-clip-update",
				clipMonitorAddr.sun_path,
				sel_mid,
				NULL
		};
		char **argv[] = {argv_clip, argv_sel};
		clipMonitor = xrealloc(clipMonitor, (clipMonitorCount += 2) * sizeof(*clipMonitor));
		for (int i = 0; i < 2; ++i) {
			struct clipMonitor *cm = clipMonitor + i + (2 * f);
			if (posix_spawnp(&cm->pid,"wl-paste",NULL,NULL,argv[i],environ)) {
				logPErr("spawn");
				free(clip_mid);
				free(sel_mid);
				return false;
			}
			cm->buf_len = 0;
			cm->buf = NULL;
			cm->fmt = fmt;
			cm->id = i;
		}
		free(clip_mid);
		free(sel_mid);
	}
	return true;
}
/* Converts in-place, so *buf *MUST* be free-able. */
static bool convert_bmp(char **buf, size_t *len)
{
	unsigned char *out;
	size_t out_len = 0;
	if (!(out = imgMemConvert(*buf, *len, &out_len, IMG_FMT_BMP))) {
		return false;
	}
	free(*buf);
	*buf = out;
	*len = out_len;
	/* synergy expects us to trim off 14-byte header it seems */
	memmove(*buf, *buf + 14, *len -= 14);
	*buf = xrealloc(*buf, *len);
	logDbg("Wrote %zd bytes of bitmap data", *len);
	return true;
}
/* process poll data */
void clipMonitorPollProc(struct pollfd *pfd)
{
	size_t len, mid;
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
			if (!read_full(pfd->fd, &mid, sizeof(mid), 0)) {
				logPErr("Could not read monitor ID");
				goto done;
			}
			if (mid >= clipMonitorCount) {
				logErr("Monitor ID exceeds count");
				goto done;
			}
			if (!read_full(pfd->fd, &len, sizeof(len), 0)) {
				logPErr("Could not read clipboard data length");
				goto done;
			}
			char *buf = xmalloc(len);
			if (!read_full(pfd->fd, buf, len, 0)) {
				logPErr("Could not read clipboard data");
				goto done;
			}
			struct clipMonitor *cm = clipMonitor + mid;
			logDbg("Raw clipboard data: CMID: %zd, ID: %d, FMT: %d, Size: %zd", mid, cm->id, cm->fmt, len);
			/* we need to convert images to please synergy */
			if (cm->fmt == USYNERGY_CLIPBOARD_FORMAT_BITMAP) {
				logDbg("Converting image to bitmap format...");
				if (!convert_bmp(&buf, &len)) {
					logErr("Could not convert to bitmap, ignoring image data");
					goto done;
				}
			}
			/* manager our buffer -- check for distinct data */
			if (cm->buf && cm->buf_len) {
				if (cm->buf_len == len) {
					if (!memcmp(cm->buf, buf, len)) {
						logDbg("Identical data, not updating synergy");
						goto done;
					}
				}
			}
			free(cm->buf);
			cm->buf = buf;
			cm->buf_len = len;
			uSynergyUpdateClipBuf(&synContext, cm->id , cm->fmt, cm->buf_len, cm->buf);
done:
			shutdown(pfd->fd, SHUT_RDWR);
			close(pfd->fd);
			pfd->fd = -1;
		}
	}
	return;
}


/* set wayland clipboard with wl-copy */
bool clipWlCopy(enum uSynergyClipboardId id, char *mime, const unsigned char *data, size_t len)
{
	pid_t pid;
	posix_spawn_file_actions_t fa;
	char *argv_0[] = {
			"wl-copy",
			"-t",
			mime,
			"-f",
			NULL};

	char *argv_1[] = {
			"wl-copy",
			"-t",
			mime,
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

