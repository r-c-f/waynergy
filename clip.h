#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <poll.h>
#include <stdint.h>
#include <stdbool.h>
#include "uSynergy.h"
#include "os.h"
#include "xmem.h"
#include "fdio_full.h"
#include <spawn.h>
#include "log.h"
#include "net.h"


#define CLIP_UPDATER_FD_COUNT 8

struct clipMonitor {
	pid_t pid;
	enum uSynergyClipboardFormat fmt;
	enum uSynergyClipboardId id;
	unsigned char *buf;
	size_t buf_len;
};


extern int clipMonitorFd;
extern struct sockaddr_un clipMonitorAddr;
extern size_t clipMonitorCount;
extern struct clipMonitor *clipMonitor;

/* check if wl-clipboard is even present */
bool clipHaveWlClipboard(void);
/* spawn wl-paste monitor processes */
bool clipSetupSockets(void);
bool clipSpawnMonitors(char *mime, enum uSynergyClipboardFormat fmt,...);
/* convert a file descriptor to a clipboard ID */
enum uSynergyClipboardId clipIdFromFd(int fd);
/* process poll data */
void clipMonitorPollProc(struct pollfd *pfd);
/* run wl-copy, with given data */
bool clipWlCopy(enum uSynergyClipboardId id, char *mime, const unsigned char *data, size_t len);
/* write all of stdin to the clipboard monitor FIFO */
int clipWriteToSocket(char *path, size_t mid);
