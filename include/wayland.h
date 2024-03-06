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
#include "ext-idle-notify-v1-client-protocol.h"
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

struct wlIdle
{
	/*module specific state */
	void *state;
	/*wayland context */
	struct wlContext *wl_ctx;
	/* actual functions */
	void (*inhibit_start)(struct wlIdle *);
	void (*inhibit_stop)(struct wlIdle *);
};

extern bool wlIdleInitExt(struct wlContext *ctx);
extern bool wlIdleInitKde(struct wlContext *ctx);
extern bool wlIdleInitGnome(struct wlContext *ctx);

#define WL_INPUT_BUTTON_COUNT 8

struct wlInput {
	/* module-specific state */
	void *state;
	/* key state information*/
	int *key_press_state;
	size_t key_press_state_len;
	// keyboard layout handling
	struct xkb_context *xkb_ctx;
	struct xkb_keymap *xkb_map;
	struct xkb_state *xkb_state;
	/* raw keymap -- distinct from xkb */
	size_t key_count;
	int *raw_keymap;
	/* id-based keymap -- uses synergy abstract keycodes */
	size_t id_count;
	int *id_keymap;
	/* whether or not a given id entry should be used */
	bool *id_keymap_valid;
	/* mouse button map */
	int button_map[WL_INPUT_BUTTON_COUNT];
	/* wayland context */
	struct wlContext *wl_ctx;
	/* actual functions */
	void (*mouse_rel_motion)(struct wlInput *, int, int);
	void (*mouse_motion)(struct wlInput *, int, int);
	void (*mouse_button)(struct wlInput *, int, int);
	void (*mouse_wheel)(struct wlInput *, signed short dx, signed short dy);
	void (*key)(struct wlInput *, int, int);
	bool (*key_map)(struct wlInput *, char *);
	void (*update_geom)(struct wlInput *);
};

/* uinput must open device fds before privileges are dropped, so this is
 * necessary */

extern bool wlInputInitWlr(struct wlContext *ctx);
extern bool wlInputInitKde(struct wlContext *ctx);
extern bool wlInputInitUinput(struct wlContext *ctx);

struct wlContext {
	char *comp_name;
	struct wl_registry *registry;
	struct wl_display *display;
	struct wl_seat *seat;
	uint32_t seat_caps;
	struct wl_keyboard *kb;
	char *kb_map;
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
	struct org_kde_kwin_idle *idle_manager; /* old KDE */
	struct ext_idle_notifier_v1 *idle_notifier; /* new standard */
	struct wlIdle idle;
	//state
	int width;
	int height;
	time_t epoch;
	long timeout;
	//callbacks
	void (*on_output_update)(struct wlContext *ctx);
};

/* flush the display with proper error checking */
extern void wlDisplayFlush(struct wlContext *ctx);

/* (re)set the keyboard layout according to the configuration
 * probably not useful outside wlSetup*/
extern int wlKeySetConfigLayout(struct wlContext *ctx);
/* load button map */
extern void wlLoadButtonMap(struct wlContext *ctx);
/* set up the wayland context */
extern bool wlSetup(struct wlContext *context, int width, int height, char *backend);

/* obtain a monotonic timestamp */
extern uint32_t wlTS(struct wlContext *context);
/* update screen resolution */
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
extern void wlMouseButton(struct wlContext *context, int button, int state);
extern void wlMouseWheel(struct wlContext *context, signed short dx, signed short dy);

/* keyboard-related functions */
/* send a raw keycode, no mapping is performed */
extern void wlKeyRaw(struct wlContext *context, int key, int state);
/* send a keycode or id, mapping as needed. Prefers the id value. */
extern void wlKey(struct wlContext *context, int key, int id, int state);
/* release all currently-pressed keys, usually on exiting the screen */
extern void wlKeyReleaseAll(struct wlContext *context);

/* enable or disable idle inhibition */
extern void wlIdleInhibit(struct wlContext *context, bool on);
