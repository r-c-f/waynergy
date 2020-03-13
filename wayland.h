#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <poll.h>
#include <sys/mman.h>
#include <xkbcommon/xkbcommon.h>
#include "os.h"
#include "xmem.h"
#include "config.h"
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include "wlr-virtual-pointer-unstable-v1.prot.h"
#include "virtual-keyboard-unstable-v1.prot.h"
#include "xdg-output-unstable-v1.prot.h"
#include "idle.prot.h"
#include "wlr-data-control-unstable-v1.prot.h"
#include "uSynergy.h"

struct wlOutput
{
	uint32_t wl_name;
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

#define CLIP_FORMAT_COUNT 1
#define CLIP_COUNT 2

/* mimetypes matching up with uSynergyClipboardFormat enums */
#define CLIP_FORMAT_MIMES_TEXT {"text/plain", "text/plain;charset=utf-8", "TEXT", "STRING", "UTF8_STRING", NULL}

struct wlContext {
	struct wl_registry *registry;
	struct wl_display *display;
	struct wl_seat *seat;
	struct zwp_virtual_keyboard_manager_v1 *keyboard_manager;
	struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
	struct zxdg_output_manager_v1 *output_manager;
	struct zwlr_virtual_pointer_v1 *pointer;
	struct zwp_virtual_keyboard_v1 *keyboard;
	struct wlOutput *outputs;
	struct org_kde_kwin_idle *idle_manager;
	struct org_kde_kwin_idle_timeout *idle_timeout;
	// keyboard layout handling
	struct xkb_context *xkb_ctx;
	struct xkb_keymap *xkb_map;
	struct xkb_state *xkb_state;
	struct zwlr_data_control_manager_v1 *data_manager;
	struct zwlr_data_control_device_v1 *data_device;
	struct zwlr_data_control_offer_v1 *data_offer;
	char *data_offer_mimes[CLIP_FORMAT_COUNT];
	struct zwlr_data_control_source_v1 *data_source[CLIP_COUNT];
	bool data_source_types[CLIP_COUNT][CLIP_FORMAT_COUNT];
	char *data_source_buf[CLIP_COUNT][CLIP_FORMAT_COUNT];
	size_t data_source_len[CLIP_COUNT][CLIP_FORMAT_COUNT];
	//listeners
	struct zxdg_output_v1_listener xdg_output_listener;
	struct wl_output_listener output_listener;
	struct wl_registry_listener registry_listener;
	//state
	int width;
	int height;
	time_t epoch;
	//callbacks
	void (*on_output_update)(struct wlContext *ctx);
};

extern int wlKeySetConfigLayout(struct wlContext *ctx);
extern bool wlClipAll(struct wlContext *context, enum uSynergyClipboardId id, char **data, size_t *len);
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
extern void wlKey(struct wlContext *context, int key, int state);
extern void wlKeyReleaseAll(struct wlContext *context);
extern void wlIdleInhibit(struct wlContext *context, bool on);
