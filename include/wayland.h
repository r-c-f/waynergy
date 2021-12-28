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
#include "fake-input-client-protocol.h"
#include "wlr-virtual-pointer-unstable-v1-client-protocol.h"
#include "virtual-keyboard-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"
#include "idle-client-protocol.h"
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

struct wlInput {
	/* module-specific state */
	void *state;
	/* key state information */
	int *key_press_state;
	size_t key_count;
	// keyboard layout handling
	int xkb_key_offset;
	struct xkb_context *xkb_ctx;
	struct xkb_keymap *xkb_map;
	struct xkb_state *xkb_state;
	/* wayland context */
	struct wlContext *wl_ctx;
	/* actual functions */
	void (*mouse_rel_motion)(struct wlInput *, int, int);
	void (*mouse_motion)(struct wlInput *, int, int);
	void (*mouse_button)(struct wlInput *, int, int);
	void (*mouse_wheel)(struct wlInput *, signed short dx, signed short dy);
	void (*key)(struct wlInput *, int, int);
	bool (*key_map)(struct wlInput *, char *);
};

extern bool wlInputInitWlr(struct wlContext *ctx);
extern bool wlInputInitKde(struct wlContext *ctx);

struct wlContext {
	struct wl_registry *registry;
	struct wl_display *display;
	struct wl_seat *seat;
	struct wlInput *input;
	struct zwp_virtual_keyboard_manager_v1 *keyboard_manager;
	struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
	struct org_kde_kwin_fake_input *fake_input;
	struct zxdg_output_manager_v1 *output_manager;
	struct wlOutput *outputs;
	struct org_kde_kwin_idle *idle_manager;
	struct org_kde_kwin_idle_timeout *idle_timeout;
	//state
	int width;
	int height;
	time_t epoch;
	//callbacks
	void (*on_output_update)(struct wlContext *ctx);
};

extern int wlKeySetConfigLayout(struct wlContext *ctx);
extern bool wlSetup(struct wlContext *context, int width, int height);
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
