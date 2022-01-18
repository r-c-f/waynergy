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
	/* key state information*/
	int *key_press_state;
	size_t key_count;
	// keyboard layout handling
	int xkb_key_offset;
	struct xkb_context *xkb_ctx;
	struct xkb_keymap *xkb_map;
	struct xkb_state *xkb_state;
	/* an array of size key_count -- all calls to key() will be translated
	 * to raw_keymap[code] */
	int *raw_keymap;
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

/* uinput must open device fds before privileges are dropped, so this is
 * necessary */

extern bool wlInputInitWlr(struct wlContext *ctx);
extern bool wlInputInitKde(struct wlContext *ctx);
extern bool wlInputInitUinput(struct wlContext *ctx);

struct wlContext {
	struct wl_registry *registry;
	struct wl_display *display;
	struct wl_seat *seat;
	struct wlInput input;
	/* /dev/uinput file descriptors, for mouse or keyboard
	 * or -1 to disable */
	int uinput_fd[2];
	/* objects offered by the compositor that backends might use */
	struct zwp_virtual_keyboard_manager_v1 *keyboard_manager;
	struct zwlr_virtual_pointer_manager_v1 *pointer_manager;
	struct org_kde_kwin_fake_input *fake_input;
	/* output stuff */
	struct zxdg_output_manager_v1 *output_manager;
	struct wlOutput *outputs;
	/* idle stuff */
	struct org_kde_kwin_idle *idle_manager;
	struct org_kde_kwin_idle_timeout *idle_timeout;
	//state
	int width;
	int height;
	time_t epoch;
	//callbacks
	void (*on_output_update)(struct wlContext *ctx);
};

/* (re)set the keyboard layout according to the configuration
 * probably not usful outside wlSetup*/
extern int wlKeySetConfigLayout(struct wlContext *ctx);
/* set up the wayland contet */
extern bool wlSetup(struct wlContext *context, int width, int height, char *backend);

/* obtain a monotonic timestamp */
extern uint32_t wlTS(struct wlContext *context);
/* update sreen resolution */
extern void wlResUpdate(struct wlContext *context, int width, int height);
/* close wayland connection */
extern void wlClose(struct wlContext *context);
/* retrieve the wayland connection file descriptor, for polling purposes */
extern int wlPrepareFd(struct wlContext *context);
/* process IO indicated by poll() */
extern void wlPollProc(struct wlContext *context, short revents);

/* mouse-related functions */
extern void wlMouseRelativeMotion(struct wlContext *context, int dx, int dy);
extern void wlMouseMotion(struct wlContext *context, int x, int y);
extern void wlMouseButtonDown(struct wlContext *context, int button);
extern void wlMouseButtonUp(struct wlContext *context, int button);
extern void wlMouseWheel(struct wlContext *context, signed short dx, signed short dy);

/* keyboard-related functions */
extern void wlKey(struct wlContext *context, int key, int state);
/* release all currently-pressed keys, usually on exiting the screen */
extern void wlKeyReleaseAll(struct wlContext *context);

/* enable or disable idle inhibition */
extern void wlIdleInhibit(struct wlContext *context, bool on);
