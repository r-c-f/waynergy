#include "../include/xkb_util.h"
#include "../include/log.h"

/* Covers two things wlKeySetConfigLayout() (src/wl_input.c) depends on:
 * that waynergyDefaultKeymap is valid xkb, and that waynergyModInit()
 * correctly reports success/failure (its caller relies on that return
 * value to fail cleanly on an unparseable keymap rather than continuing
 * with a NULL xkb_state, which wlKeyRaw() would later crash on).
 *
 * Doesn't cover wlKeySetConfigLayout() itself, or the
 * wl_display_roundtrip() call it makes to give a compositor keymap event
 * a chance to arrive -- that's entangled with a live Wayland connection,
 * outside what this project's existing test harness attempts (see
 * test/os.c, test/config.c: pure logic only, no compositor).
 */

static bool default_keymap_compiles(void)
{
	struct xkb_context *ctx;
	struct xkb_keymap *km;
	bool ok;

	if (!(ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS))) {
		logErr("Could not create xkb context");
		return false;
	}
	km = xkb_keymap_new_from_string(ctx, waynergyDefaultKeymap,
			XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	ok = (km != NULL);
	if (!ok) {
		logErr("waynergyDefaultKeymap failed to compile");
	}
	if (km) {
		xkb_keymap_unref(km);
	}
	xkb_context_unref(ctx);
	return ok;
}

static bool malformed_keymap_rejected(void)
{
	struct xkb_context *ctx;
	struct xkb_keymap *km;
	bool ok;
	const char *bad = "this is not a valid xkb keymap {{{";

	if (!(ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS))) {
		logErr("Could not create xkb context");
		return false;
	}
	km = xkb_keymap_new_from_string(ctx, bad,
			XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	ok = (km == NULL);
	if (!ok) {
		logErr("malformed keymap string was incorrectly accepted");
		xkb_keymap_unref(km);
	}
	xkb_context_unref(ctx);
	return ok;
}

static bool mod_init_succeeds_on_valid_keymap(void)
{
	struct xkb_context *xkb_ctx = NULL;
	struct xkb_keymap *xkb_map = NULL;
	struct xkb_state *xkb_state = NULL;
	bool ok;

	ok = waynergyModInit(&xkb_ctx, &xkb_map, &xkb_state, (char *)waynergyDefaultKeymap);
	if (!ok) {
		logErr("waynergyModInit() failed on a known-valid keymap");
		return false;
	}
	if (!xkb_state) {
		logErr("waynergyModInit() returned true but left xkb_state NULL");
		return false;
	}
	xkb_state_unref(xkb_state);
	xkb_keymap_unref(xkb_map);
	xkb_context_unref(xkb_ctx);
	return true;
}

static bool mod_init_fails_on_malformed_keymap(void)
{
	struct xkb_context *xkb_ctx = NULL;
	struct xkb_keymap *xkb_map = NULL;
	struct xkb_state *xkb_state = NULL;
	bool ok;

	ok = waynergyModInit(&xkb_ctx, &xkb_map, &xkb_state, "this is not a valid xkb keymap {{{");
	if (ok) {
		logErr("waynergyModInit() incorrectly succeeded on a malformed keymap");
		return false;
	}
	if (xkb_state) {
		logErr("waynergyModInit() returned false but xkb_state is non-NULL anyway");
		return false;
	}
	return true;
}

int main(int argc, char **argv)
{
	logInit(LOG_DBG, NULL);

	bool stat = true;
	stat = stat && default_keymap_compiles();
	stat = stat && malformed_keymap_rejected();
	stat = stat && mod_init_succeeds_on_valid_keymap();
	stat = stat && mod_init_fails_on_malformed_keymap();
	return !stat;
}
