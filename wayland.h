#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <poll.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "wlr-virtual-pointer-unstable-v1.prot.h"
#include "virtual-keyboard-unstable-v1.prot.h"


extern int wlSetup(int width, int height);
extern void wlClose(void);
extern int wlPrepareFd(void);
extern void wlPollProc(short revents);

extern void wlMouseRelativeMotion(int dx, int dy);
extern void wlMouseMotion(int x, int y);
extern void wlMouseButtonDown(int button);
extern void wlMouseButtonUp(int button);
extern void wlMouseWheel(signed short dx, signed short dy);
extern void wlKey(int key, int state, uint32_t mask);
extern void wlIdleInhibit(bool on);
