#pragma once
#include <stdbool.h>
#include <xkbcommon/xkbcommon.h>

/* src/xkb_util.c. Split out of wayland.h/wl_input.c specifically so
 * these have no Wayland-protocol dependency and can be included and
 * linked (see test/keymap.c) without any of the generated protocol
 * headers, which only exist inside the meson build tree and aren't
 * available to the plain `cc` invocations in test/run.sh. */

/* Baked-in fallback keymap, used when the compositor's own keymap could
 * not be obtained (either because wl_keyboard_map is set to false, or
 * because fetching/mmap'ing it from the compositor failed) and no
 * explicit xkb_keymap has been configured. Equivalent to what
 * `setxkbmap -print` emits for a stock US/evdev layout. This exists so
 * ctx->kb_map being NULL never reaches xkb_keymap_new_from_string() as a
 * NULL keymap string -- see wlKeySetConfigLayout() in wl_input.c.
 */
extern const char *waynergyDefaultKeymap;

/* Keyboard state tracking for modifier masks (the synergy protocol is
 * less than ideal at sending us modifiers). On success, *out_ctx,
 * *out_map, and *out_state are all populated and owned by the caller;
 * on failure they're left NULL and nothing needs cleaning up. */
extern bool waynergyModInit(struct xkb_context **out_ctx,
		struct xkb_keymap **out_map, struct xkb_state **out_state,
		char *keymap_str);
