#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>
#include <stdint.h>
#include <stdbool.h>
#include "uSynergy.h"
#include "os.h"
#include "xmem.h"
#include "fdio_full.h"

/* wl-paste FIFO file descriptors -- -1, until spawned */
extern int clipMonitorFd[2];
/* wl-paste FIFO paths -- NULL, until spawned */
extern char *clipMonitorPath[2];
/* wl-paste pids -- so we can kill them off later */
extern pid_t clipMonitorPid[2];

/* convert a file descriptor to a clipboard ID */
enum uSynergyClipboardId clipIdFromFd(int fd);
/* process poll data */
void clipMonitorPollProc(struct pollfd *pfd);
/* run wl-copy, with given data */
bool clipWlCopy(enum uSynergyClipboardId id, const unsigned char *data, size_t len);
/* write all of stdin to the clipboard monitor FIFO */
int clipWriteToFifo(char *path);
