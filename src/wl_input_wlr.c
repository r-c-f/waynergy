#include "wayland.h"
#include <stdbool.h>
#include "log.h"
#include "fdio_full.h"
#include "config.h"
#include <xkbcommon/xkbcommon.h>
#include <spawn.h>
#include <ctype.h>

extern char **environ;

struct state_wlr {
	struct zwlr_virtual_pointer_v1 *pointer;
	int wheel_mult;
	struct zwp_virtual_keyboard_v1 *keyboard;
};

/* create a layout file descriptor */
static bool key_map(struct wlInput *input, char *keymap_str)
{
	logDbg("Setting virtual keymap");
	struct state_wlr *wlr = input->state;
	int fd;
	if ((fd = osGetAnonFd()) == -1) {
		return false;
	}
	size_t keymap_size = strlen(keymap_str) + 1;
	if (!write_full(fd, keymap_str, keymap_size, 0)) {
		return false;
	}
	zwp_virtual_keyboard_v1_keymap(wlr->keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, keymap_size);
	return true;
}

static void key(struct wlInput *input, int key, int state)
{
	struct state_wlr *wlr = input->state;
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(input->xkb_state, XKB_STATE_MODS_DEPRESSED);
	xkb_mod_mask_t latched = xkb_state_serialize_mods(input->xkb_state, XKB_STATE_MODS_LATCHED);
	xkb_mod_mask_t locked = xkb_state_serialize_mods(input->xkb_state, XKB_STATE_MODS_LOCKED);
	xkb_layout_index_t group = xkb_state_serialize_layout(input->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	zwp_virtual_keyboard_v1_key(wlr->keyboard, wlTS(input->wl_ctx), key - 8, state);
	zwp_virtual_keyboard_v1_modifiers(wlr->keyboard, depressed, latched, locked, group);
	wlDisplayFlush(input->wl_ctx);
}

static void mouse_rel_motion(struct wlInput *input, int dx, int dy)
{
	struct state_wlr *wlr = input->state;
	zwlr_virtual_pointer_v1_motion(wlr->pointer, wlTS(input->wl_ctx), wl_fixed_from_int(dx), wl_fixed_from_int(dy));
	zwlr_virtual_pointer_v1_frame(wlr->pointer);
	wlDisplayFlush(input->wl_ctx);
}
static void mouse_motion(struct wlInput *input, int x, int y)
{
	struct state_wlr *wlr = input->state;
	zwlr_virtual_pointer_v1_motion_absolute(wlr->pointer, wlTS(input->wl_ctx), x, y, input->wl_ctx->width, input->wl_ctx->height);
	zwlr_virtual_pointer_v1_frame(wlr->pointer);
	wlDisplayFlush(input->wl_ctx);
}
static void mouse_button(struct wlInput *input, int button, int state)
{
	struct state_wlr *wlr = input->state;
	zwlr_virtual_pointer_v1_button(wlr->pointer, wlTS(input->wl_ctx), button, state);
	zwlr_virtual_pointer_v1_frame(wlr->pointer);
	wlDisplayFlush(input->wl_ctx);
}
static void mouse_wheel(struct wlInput *input, signed short dx, signed short dy)
{
	struct state_wlr *wlr = input->state;
	//we are a wheel, after all
	zwlr_virtual_pointer_v1_axis_source(wlr->pointer, 0);
	if (dx < 0) {
		zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 1, wl_fixed_from_int(15), wlr->wheel_mult);
	}else if (dx > 0) {
		zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 1, wl_fixed_from_int(-15), -1 * wlr->wheel_mult);
	}
	if (dy < 0) {
		zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 0, wl_fixed_from_int(15), wlr->wheel_mult);
	} else if (dy > 0) {
		zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 0, wl_fixed_from_int(-15), -1 * wlr->wheel_mult);
	}
	zwlr_virtual_pointer_v1_frame(wlr->pointer);
	wlDisplayFlush(input->wl_ctx);
}

bool wlInputInitWlr(struct wlContext *ctx)
{
	int wheel_mult_default;
	struct state_wlr *wlr;
	if (!(ctx->pointer_manager && ctx->keyboard_manager)) {
		return false;
	}
	wlr = xmalloc(sizeof(*wlr));
	wlr->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(ctx->pointer_manager, ctx->seat);

	wlr->keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(ctx->keyboard_manager, ctx->seat);
	/* some wlroots versions behaved weirdly with discrete inputs,
	 * accumulating them and only issuing a client event when they've reached
	 * 120. This seems to have been fixed in v0.16.0, so the detection
	 * logic shouldn't be needed anymore, but we'll leave it configurable
	 * just in case */
	wheel_mult_default = 1;
	wlr->wheel_mult = configTryLong("wlr/wheel_mult", wheel_mult_default);
	logDbg("Using wheel_mult value of %d", wlr->wheel_mult);
	ctx->input = (struct wlInput) {
		.state = wlr,
		.wl_ctx = ctx,
		.mouse_rel_motion = mouse_rel_motion,
		.mouse_motion = mouse_motion,
		.mouse_button = mouse_button,
		.mouse_wheel = mouse_wheel,
		.key = key,
		.key_map = key_map,
	};
	wlLoadButtonMap(ctx);
	logInfo("Using wlroots virtual input protocols");
	return true;
}

