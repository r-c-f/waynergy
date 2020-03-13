#include "clip.h"
#include "wayland.h"
#include "net.h"

extern uSynergyContext synContext;
extern char **environ;

int clipMonitorFd;
struct sockaddr_un clipMonitorAddr;
pid_t clipMonitorPid[2];

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

}
