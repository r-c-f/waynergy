#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <poll.h>
#include <sys/mman.h>
#include "os.h"
#include "xmem.h"
#include "config.h"
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "wlr-virtual-pointer-unstable-v1.prot.h"
#include "virtual-keyboard-unstable-v1.prot.h"
#include "xdg-output-unstable-v1.prot.h"
#include "wlr-data-control-unstable-v1.prot.h"
#include "uSynergy.h"

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

enum wlSelectionId {
	WL_SELECTION_CLIPBOARD = 0,
	WL_SELECTION_PRIMARY,
	WL_SELECTION_MAX
}; 
enum wlSelectionFormat {
	WL_SELECTION_FORMAT_TEXT = 0,
//	WL_SELECTION_FORMAT_BITMAP, TODO
//	WL_SELECTION_FORMAT_HTML, TODO
	WL_SELECTION_FORMAT_MAX
};
#define WL_SELECTION_FORMAT_MIMES { "text/plain", NULL };
extern  char **wlSelectionFormatMimes;


struct wlSelectionBuffer {
	unsigned char *data;
	size_t pos;
	size_t alloc;
	int offer_fd;
	bool complete;
	/* FIXME: need for hack purposes....
	 * stores the last time any activity occured for this buffer*/
	time_t last_active;
	/* FIXME: this should not be fucking needed, but for some reason
	 * we can't close the write end in our process */
	int offer_fd_write;
}

struct wlContext {
	struct wl_registry *registry;
	struct wl_display *display;
	struct wl_seat *seat;
	struct zwlr_data_control_manager *data_manager;
	struct zwlr_data_control_device_v1 *data_control;
	struct zwlr_data_control_offer_v1 *data_offer;
	bool data_offer_formats[WL_SELECTION_FORMAT_MAX];
	struct wlSelection data_buffer[WL_SELECTION_MAX][WL_SELECTION_FORMAT_MAX];
	struct zwp_virtual_keyboard_manager_v1 *keyboard_manager;
	struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
	struct zxdg_output_manager_v1 *output_manager;
	struct zwlr_virtual_pointer_v1 *pointer;
	struct zwp_virtual_keyboard_v1 *keyboard;
	struct wlOutput *outputs;
	//state
	int width;
	int height;
	time_t epoch;
	//callbacks
	void (*on_output_update)(struct wlContext *ctx);
};

extern int wlLoadConfLayout(struct wlContext *ctx);
extern int wlSetup(struct wlContext *context, int width, int height);
extern uint32_t wlTS(struct wlContext *context);
extern void wlResUpdate(struct wlContext *context, int width, int height);
extern void wlClose(struct wlContext *context);
extern int wlPrepareFd(struct wlContext *context);
extern void wlPollProc(struct wlContext *context, short revents);

extern void wlMouseRelativeMotion(struct wlContext *context, int dx, int dy);
extern void wlMouseMotion(struct wlContext *context, int x, int y);
extern void wlMouseButtonDown(struct wlContext *context, int button);
extern void wlMouseButtonUp(struct wlContext *context, int button);
extern void wlMouseWheel(struct wlContext *context, signed short dx, signed short dy);
extern void wlKey(struct wlContext *context, int key, int state, uint32_t mask);
extern void wlIdleInhibit(struct wlContext *context, bool on);
