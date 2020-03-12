#include "wayland.h"
#include <stdbool.h>
#include "log.h"
#include <xkbcommon/xkbcommon.h>


/* Code to track keyboard state for modifier masks
 * because the synergy protocol is less than ideal at sending us modifiers
*/
static struct xkb_context *xkb_ctx;
static struct xkb_keymap *xkb_map;
static struct xkb_state *xkb_state;


static bool local_mod_init(char *keymap_str) {
	xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!xkb_ctx) {
		return false;
	}
	xkb_map = xkb_keymap_new_from_string(xkb_ctx, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!xkb_map) {
		xkb_context_unref(xkb_ctx);
		return false;
	}
	xkb_state = xkb_state_new(xkb_map);
	if (!xkb_state) {
		xkb_map_unref(xkb_map);
		xkb_context_unref(xkb_ctx);
		return false;
	}

        return true;
}



/* create a layout file descriptor */
int wlLoadConfLayout(struct wlContext *ctx)
{
        int ret = 0;
        int fd;
        char nul = 0;
        char *keymap_str = configTryString("xkb_keymap", "xkb_keymap { \
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
	local_mod_init(keymap_str);
done:
        free(keymap_str);
        return ret;
}

void wlKey(struct wlContext *ctx, int key, int state)
{
	xkb_state_update_key(xkb_state, key, state);
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(xkb_state, XKB_STATE_MODS_DEPRESSED);
	xkb_mod_mask_t latched = xkb_state_serialize_mods(xkb_state, XKB_STATE_MODS_LATCHED);
        xkb_mod_mask_t locked = xkb_state_serialize_mods(xkb_state, XKB_STATE_MODS_LOCKED);
	xkb_layout_index_t group = xkb_state_serialize_layout(xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	logDbg("depressed: %x latched: %x locked: %x group: %x", depressed, latched, locked, group);
        zwp_virtual_keyboard_v1_modifiers(ctx->keyboard, depressed, latched, locked, group);
	zwp_virtual_keyboard_v1_key(ctx->keyboard, wlTS(ctx), key - 8, state);
        wl_display_flush(ctx->display);
}
