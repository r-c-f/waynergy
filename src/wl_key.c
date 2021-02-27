#include "wayland.h"
#include <stdbool.h>
#include "log.h"
#include <xkbcommon/xkbcommon.h>



/* keep track of every key pressed, and how many times it has been pressed. 
 * This will allow us to avoid the problem of stuck keys should we exit the
 * screen with something still pressed for some reason, which you wouldn't
 * encounter with the mouse, but with the keyboard shortcut? Probably*/
static int *key_press_counts;
static size_t key_press_len;


/* Code to track keyboard state for modifier masks
 * because the synergy protocol is less than ideal at sending us modifiers
*/


static bool local_mod_init(struct wlContext *wl_ctx, char *keymap_str) {
	wl_ctx->xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!wl_ctx->xkb_ctx) {
		return false;
	}
	wl_ctx->xkb_map = xkb_keymap_new_from_string(wl_ctx->xkb_ctx, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!wl_ctx->xkb_map) {
		xkb_context_unref(wl_ctx->xkb_ctx);
		return false;
	}
	wl_ctx->xkb_state = xkb_state_new(wl_ctx->xkb_map);
	if (!wl_ctx->xkb_state) {
		xkb_map_unref(wl_ctx->xkb_map);
		xkb_context_unref(wl_ctx->xkb_ctx);
		return false;
	}
	/* initialize keystats */
	if (key_press_counts) {
		free(key_press_counts);
	}
	key_press_len = xkb_keymap_max_keycode(wl_ctx->xkb_map) + 1;
	key_press_counts = xcalloc(key_press_len, sizeof(*key_press_counts));
        return true;
}



/* create a layout file descriptor */
int wlKeySetConfigLayout(struct wlContext *ctx)
{
        int ret = 0;
        int fd;
        char nul = 0;
	char *keymap_str = configTryStringFull("xkb_keymap", "xkb_keymap { \
		xkb_keycodes  { include \"xfree86+aliases(qwerty)\"     }; \
		xkb_types     { include \"complete\"    }; \
		xkb_compat    { include \"complete\"    }; \
		xkb_symbols   { include \"pc+us+inet(evdev)\"   }; \
		xkb_geometry  { include \"pc(pc105)\"   }; \
};");
        if ((fd = osGetAnonFd()) == -1) {
                ret = 1;
                goto done;
        }
        size_t keymap_size = strlen(keymap_str) + 1;
        if (lseek(fd, keymap_size, SEEK_SET) != keymap_size) {
                ret = 2;
                goto done;
        }
        if (write(fd, &nul, 1) != 1) {
                ret = 3;
                goto done;
        }
        void *ptr = mmap(NULL, keymap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (ptr == MAP_FAILED) {
                ret = 4;
                goto done;
        }
        strcpy(ptr, keymap_str);
        zwp_virtual_keyboard_v1_keymap(ctx->keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, keymap_size);
	local_mod_init(ctx, keymap_str);
done:
        free(keymap_str);
        return ret;
}

void wlKey(struct wlContext *ctx, int key, int state)
{
	if (!key_press_counts[key] && !state) {
		return;
	}
	key_press_counts[key] += state ? 1 : -1;
	xkb_state_update_key(ctx->xkb_state, key, state);
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(ctx->xkb_state, XKB_STATE_MODS_DEPRESSED);
	xkb_mod_mask_t latched = xkb_state_serialize_mods(ctx->xkb_state, XKB_STATE_MODS_LATCHED);
        xkb_mod_mask_t locked = xkb_state_serialize_mods(ctx->xkb_state, XKB_STATE_MODS_LOCKED);
	xkb_layout_index_t group = xkb_state_serialize_layout(ctx->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	logDbg("depressed: %x latched: %x locked: %x group: %x", depressed, latched, locked, group);
	zwp_virtual_keyboard_v1_key(ctx->keyboard, wlTS(ctx), key - 8, state);
        zwp_virtual_keyboard_v1_modifiers(ctx->keyboard, depressed, latched, locked, group);
        wl_display_flush(ctx->display);
}
void wlKeyReleaseAll(struct wlContext *ctx)
{
	size_t i;
	for (i = 0; i < key_press_len; ++i) {
		while (key_press_counts[i]) {
			logDbg("Release all: key %zd, pressed %d times", i, key_press_counts[i]);
			wlKey(ctx, i, 0);
		}
	}
}
