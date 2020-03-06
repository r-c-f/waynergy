#include "wayland.h"
#include <stdbool.h>
#include "log.h"
#include <xkbcommon/xkbcommon.h>

#define XMOD_SHIFT              0x0001
#define XMOD_CONTROL            0x0004
#define XMOD_ALT                0x0008
#define XMOD_META               0x0008
#define XMOD_SUPER              0x0040

static uint32_t smod_to_xmod[][2] = {
        {USYNERGY_MODIFIER_SHIFT, XMOD_SHIFT},
        {USYNERGY_MODIFIER_CTRL, XMOD_CONTROL},
        {USYNERGY_MODIFIER_ALT, XMOD_ALT},
        {USYNERGY_MODIFIER_META, XMOD_META},
        {USYNERGY_MODIFIER_WIN, XMOD_SUPER},
        {0,0}
};
static inline uint32_t wlModConvert(uint32_t smod)
{
        uint32_t xmod = 0;
        int i;
        for (i = 0; smod_to_xmod[i][0] && smod_to_xmod[i][1]; ++i) {
                if (smod & smod_to_xmod[i][0])
                        xmod |= smod_to_xmod[i][1];
        }
        return xmod;
}

/* Code to handle intrinsic masks
 * because the synergy protocol is less than ideal at sending us modifiers
*/
static long *local_mod = NULL;
static size_t local_mod_len = 0;

static bool local_mod_init(char *keymap_str) {
#ifdef USE_INTRINSIC_MASK
        char **lines;
        size_t i, l, k;
        long key;
        char *conf[] = {
                "intrinsic_mask/shift",
                "intrinsic_mask/control",
                "intrinsic_mask/alt",
                "intrinsic_mask/meta",
                "intrinsic_mask/super",
                NULL
        };
        uint32_t masks[] = {
                XMOD_SHIFT,
                XMOD_CONTROL,
                XMOD_ALT,
                XMOD_META,
                XMOD_SUPER,
                0
        };
	struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!ctx) {
		return false;
	}
	struct xkb_keymap *map = xkb_keymap_new_from_string(ctx, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!map) {
		xkb_context_unref(ctx);
		return false;
	}
        //start off with something sensible -- 255 seems good
        local_mod_len = 256;
        local_mod = xcalloc(sizeof(*local_mod), local_mod_len);
        for (i = 0; conf[i] && masks[i]; ++i) {
                if (!(lines = configReadLines(conf[i])))
                        continue;
                for (l = 0; lines[l]; ++l) {
			/* strip newlines */
			for (k = 0; lines[l][k]; ++k) {
				if (lines[l][k] == '\n') {
					lines[l][k] = '\0';
					break;
				}
			}
                        key = xkb_keymap_key_by_name(map, lines[l]);
                        if (key == XKB_KEYCODE_INVALID)
                                continue;
                        /* resize array if needed */
                        if (key >= local_mod_len) {
                                local_mod = xrealloc(local_mod, sizeof(*local_mod) * (key + 1));
                                /* be sure to zero all that shit */
                                memset(local_mod + local_mod_len, 0, (key + 1) - local_mod_len);
                                local_mod_len = key + 1;
                        }
                        /* now we process as normal */
                        local_mod[key] |= masks[i];
			logDbg("Got intrinsic mask for key %s (%ld): %lx", lines[l], key, local_mod[key]);
                }
                strfreev(lines);
        }
	xkb_map_unref(map);
	xkb_context_unref(ctx);
#endif
        return true;
}



static inline long intrinsic_mask(int key)
{
#ifdef USE_INTRINSIC_MASK
        if (key < local_mod_len) {
                return local_mod[key];
        }
#endif
        return 0;
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
	/* intrinsic mask  hack initialize */
	local_mod_init(keymap_str);
done:
        free(keymap_str);
        return ret;
}

void wlKey(struct wlContext *ctx, int key, int state, uint32_t mask)
{
        key -= 8;
        int xkb_sym;
        uint32_t xmodmask = wlModConvert(mask);
        if ((key & 0xE000) == 0xE000) {
                xkb_sym = key + 0x1000;
        } else {
                xkb_sym = key;
        }
        logDbg("Got modifier mask: %" PRIx32, xmodmask);
        xmodmask |= intrinsic_mask(key + 8);
	logDbg("With intrinsic mask: %" PRIx32, xmodmask);
        zwp_virtual_keyboard_v1_modifiers(ctx->keyboard, xmodmask, 0, 0, 0);
        zwp_virtual_keyboard_v1_key(ctx->keyboard, wlTS(ctx), xkb_sym, state);
        if (!state) {
                zwp_virtual_keyboard_v1_modifiers(ctx->keyboard, 0, 0, 0, 0);
        }
        wl_display_flush(ctx->display);
}
