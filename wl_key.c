#include "wayland.h"
#include <stdbool.h>
#include "log.h"

#define SMOD_SHIFT              0x0001
#define XMOD_SHIFT              0x0001

#define SMOD_CONTROL            0x0002
#define XMOD_CONTROL            0x0004

#define SMOD_ALT                0x0004
#define XMOD_ALT                0x0008

#define SMOD_META               0x0008
#define XMOD_META               0x0008

#define SMOD_SUPER              0x0010
#define XMOD_SUPER              0x0040

static uint32_t smod_to_xmod[][2] = {
        {SMOD_SHIFT, XMOD_SHIFT},
        {SMOD_CONTROL, XMOD_CONTROL},
        {SMOD_ALT, XMOD_ALT},
        {SMOD_META, XMOD_META},
        {SMOD_SUPER, XMOD_SUPER},
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

/* FIXME XXX XXX
 * jesus christ this is fucking hacky as shit we should automate this behind
 * xkb or something fuck me
*/
static long *local_mod = NULL;
static size_t local_mod_len = 0;

static bool local_mod_init(void) {
#ifdef USE_INTRINSIC_MASK
        char **lines;
        size_t i, l;
        long key;
        char *conf[] = {
                "intrinsic_mask/shift",
                "instrinsic_mask/control",
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
        //start off with something sensible -- 255 seems good
        local_mod_len = 256;
        local_mod = xcalloc(sizeof(*local_mod), local_mod_len);
        for (i = 0; conf[i] && masks[i]; ++i) {
                if (!(lines = configReadLines(conf[i])))
                        continue;
                for (l = 0; lines[l]; ++l) {
                        errno = 0;
                        key = strtol(lines[l], NULL, 0);
                        if (errno)
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
                }
                strfreev(lines);
        }
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
	local_mod_init();
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
