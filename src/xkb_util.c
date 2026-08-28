/* See include/xkb_util.h for why this is split out of wayland.h/
 * wl_input.c: no Wayland-protocol dependency, so it (and its declarations)
 * can be included and linked directly by test/keymap.c, using the same
 * plain `cc` invocation as the rest of this project's minimal test
 * harness (test/run.sh) -- unlike wayland.h, which pulls in generated
 * protocol headers only available inside the meson build tree.
 */
#include "xkb_util.h"

const char *waynergyDefaultKeymap =
	"xkb_keymap {\n"
	"	xkb_keycodes  { include \"evdev+aliases(qwerty)\" };\n"
	"	xkb_types     { include \"complete\" };\n"
	"	xkb_compat    { include \"complete\" };\n"
	"	xkb_symbols   { include \"pc+us+inet(evdev)\" };\n"
	"};\n";

bool waynergyModInit(struct xkb_context **out_ctx, struct xkb_keymap **out_map,
		struct xkb_state **out_state, char *keymap_str)
{
	/* Set all three up front so every failure path below honors the
	 * "left NULL on failure" contract documented in xkb_util.h, even
	 * the first one, which returns before out_map/out_state are
	 * otherwise touched. A caller that reuses these across a retry
	 * without re-zeroing them itself could otherwise misread leftover
	 * non-NULL values as still valid, or double-free them. */
	*out_ctx = NULL;
	*out_map = NULL;
	*out_state = NULL;

	*out_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!*out_ctx) {
		return false;
	}
	*out_map = xkb_keymap_new_from_string(*out_ctx, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!*out_map) {
		xkb_context_unref(*out_ctx);
		*out_ctx = NULL;
		return false;
	}
	*out_state = xkb_state_new(*out_map);
	if (!*out_state) {
		xkb_map_unref(*out_map);
		*out_map = NULL;
		xkb_context_unref(*out_ctx);
		*out_ctx = NULL;
		return false;
	}
	return true;
}
