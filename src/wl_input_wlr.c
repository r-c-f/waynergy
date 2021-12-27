#include "wayland.h"
#include <stdbool.h>
#include "log.h"
#include "fdio_full.h"
#include <xkbcommon/xkbcommon.h>

struct state_wlr {
	struct zwlr_virtual_pointer_v1 *pointer;
        struct zwp_virtual_keyboard_v1 *keyboard;
};

/* create a layout file descriptor */
static bool key_map(struct wlInput *input, char *keymap_str)
{
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
        wl_display_flush(input->wl_ctx->display);
}

static int button_map[] = {
        0,
        0x110,
        0x112,
        0x111,
        0x150,
        0x151,
        -1
};

static void mouse_rel_motion(struct wlInput *input, int dx, int dy)
{
	struct state_wlr *wlr = input->state;
        zwlr_virtual_pointer_v1_motion(wlr->pointer, wlTS(input->wl_ctx), wl_fixed_from_int(dx), wl_fixed_from_int(dy));
        zwlr_virtual_pointer_v1_frame(wlr->pointer);
        wl_display_flush(input->wl_ctx->display);
}
static void mouse_motion(struct wlInput *input, int x, int y)
{
	struct state_wlr *wlr = input->state;
        zwlr_virtual_pointer_v1_motion_absolute(wlr->pointer, wlTS(input->wl_ctx), x, y, input->wl_ctx->width, input->wl_ctx->height);
        zwlr_virtual_pointer_v1_frame(wlr->pointer);
        wl_display_flush(input->wl_ctx->display);
}
static void mouse_button(struct wlInput *input, int button, int state)
{
	struct state_wlr *wlr = input->state;
	logDbg("mouse: button %d (mapped to %x) %s", button, button_map[button], state ? "down": "up");
        zwlr_virtual_pointer_v1_button(wlr->pointer, wlTS(input->wl_ctx), button_map[button], state);
        zwlr_virtual_pointer_v1_frame(wlr->pointer);
        wl_display_flush(input->wl_ctx->display);
}
static void mouse_wheel(struct wlInput *input, signed short dx, signed short dy)
{
	struct state_wlr *wlr = input->state;
        //we are a wheel, after all
        zwlr_virtual_pointer_v1_axis_source(wlr->pointer, 0);
        if (dx < 0) {
                zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 1, wl_fixed_from_int(15), 1);
        }else if (dx > 0) {
                zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 1, wl_fixed_from_int(-15), -1);
        }
        if (dy < 0) {
                zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 0, wl_fixed_from_int(15),1);
        } else if (dy > 0) {
                zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 0, wl_fixed_from_int(-15), -1);
        }
        zwlr_virtual_pointer_v1_frame(wlr->pointer);
        wl_display_flush(input->wl_ctx->display);
}


bool wlInputInitWlr(struct wlContext *ctx)
{
	struct wlInput *input;
	struct state_wlr *wlr;
	if (!(ctx->pointer_manager && ctx->keyboard_manager)) {
		return false;
	}
	logInfo("Using wlroots protocols for fake input");
	wlr = xmalloc(sizeof(*wlr));
	wlr->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(ctx->pointer_manager, ctx->seat);
        wlr->keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(ctx->keyboard_manager, ctx->seat);
	input = xcalloc(1, sizeof(*input));
	input->state = wlr;
       	input->wl_ctx = ctx;
	input->mouse_rel_motion = mouse_rel_motion;
	input->mouse_motion = mouse_motion;
	input->mouse_button = mouse_button;
	input->mouse_wheel = mouse_wheel;
	input->key = key;
	input->key_map = key_map;
	ctx->input = input;
	return true;
}

