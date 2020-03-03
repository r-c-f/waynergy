#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <poll.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "wlr-virtual-pointer-unstable-v1.prot.h"
#include "virtual-keyboard-unstable-v1.prot.h"
#include "xdg-output-unstable-v1.prot.h"

struct wlOutput
{
	struct wl_output *wl_output;
	struct zxdg_output_v1 *xdg_output;
	int32_t x;
	int32_t y;
	int width;
	int height;
	int32_t scale;
	bool complete;
	bool have_log_size;
	bool have_log_pos;
	char *name;
	char *desc;
	struct wlOutput *next;
};

extern void (*wlOnOutputsUpdated)(struct wlOutput *output);

extern int wlSetup(int width, int height);
extern void wlResUpdate(int width, int height);
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
