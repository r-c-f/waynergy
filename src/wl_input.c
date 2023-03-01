#include "wayland.h"
#include <assert.h>
#include <stdbool.h>
#include "log.h"
#include "fdio_full.h"
#include <xkbcommon/xkbcommon.h>


/* handle button maps */

void wlLoadButtonMap(struct wlContext *ctx)
{
	int i;
	char *key;
	int default_map[] = {
		0,
		0x110, /*BTN_LEFT*/
		0x112, /*BTN_MIDDLE*/
		0x111, /*BTN_RIGHT*/
		0x113, /*BTN_SIDE*/
		0x114, /*BTN_EXTRA*/
	};
	static_assert(sizeof(default_map)/sizeof(*default_map) == WL_INPUT_BUTTON_COUNT, "button map size mismatch");
	for (i = 0; i < WL_INPUT_BUTTON_COUNT; ++i) {
		xasprintf(&key, "button-map/%d", i);
		ctx->input.button_map[i] = configTryLong(key, default_map[i]);
		logDbg("Set button mapping: %d -> %d", i, ctx->input.button_map[i]);
		free(key);
	}
};


/* Code to track keyboard state for modifier masks
 * because the synergy protocol is less than ideal at sending us modifiers
*/


static bool local_mod_init(struct wlContext *wl_ctx, char *keymap_str) {
	wl_ctx->input.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	if (!wl_ctx->input.xkb_ctx) {
		return false;
	}
	wl_ctx->input.xkb_map = xkb_keymap_new_from_string(wl_ctx->input.xkb_ctx, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	if (!wl_ctx->input.xkb_map) {
		xkb_context_unref(wl_ctx->input.xkb_ctx);
		return false;
	}
	wl_ctx->input.xkb_state = xkb_state_new(wl_ctx->input.xkb_map);
	if (!wl_ctx->input.xkb_state) {
		xkb_map_unref(wl_ctx->input.xkb_map);
		xkb_context_unref(wl_ctx->input.xkb_ctx);
		return false;
	}
	return true;
}

/* and code to handle raw mapping of keys */

static void load_raw_keymap(struct wlContext *ctx)
{
	bool offset_on_explicit;
	char **key, **val, *endstr;
	int i, count, offset, lkey, rkey;
	key = NULL;
	val = NULL;
	if (ctx->input.raw_keymap) {
		free(ctx->input.raw_keymap);
	}
	/* start with the xkb maximum */
	ctx->input.key_count = xkb_keymap_max_keycode(ctx->input.xkb_map) + 1;
	logDbg("max key: %zu", ctx->input.key_count);
	if ((count = configReadFullSection("raw-keymap", &key, &val)) != -1) {
		/* slightly inefficient approach, but it will actually work
		 * First pass -- just find the *real* maximum raw keycode */
		for (i = 0; i < count; ++i) {
			errno = 0;
			lkey = strtol(key[i], &endstr, 0);
			if (errno || endstr == key[i])
				continue;
			if (lkey >= ctx->input.key_count) {
				ctx->input.key_count = lkey + 1;
				logDbg("max key update: %zu", ctx->input.key_count);

			}
		}
	}
	/* initialize everything */
	ctx->input.raw_keymap = xcalloc(ctx->input.key_count, sizeof(*ctx->input.raw_keymap));
	offset = configTryLong("raw-keymap/offset", 0);
	offset += configTryLong("xkb_key_offset", 0);
	logDbg("Initial raw key offset: %d", offset);
	for (i = 0; i < ctx->input.key_count; ++i) {
		ctx->input.raw_keymap[i] = i + offset;
	}
	/* and second pass -- store any actually mappings, apply offset */
	offset_on_explicit = configTryBool("raw-keymap/offset_on_explicit", true);
	for (i = 0; i < count; ++i) {
		errno = 0;
		lkey = strtol(key[i], &endstr, 0);
		if (errno || endstr == key[i])
			continue;
		errno = 0;
		rkey = strtol(val[i], &endstr, 0);
		if (errno || endstr == val[i])
			continue;
		ctx->input.raw_keymap[lkey] = rkey + (offset_on_explicit ? offset : 0);
		logDbg("set raw key map: %d = %d", lkey, ctx->input.raw_keymap[lkey]);
		if (rkey >= ctx->input.key_press_state_len) {
			ctx->input.key_press_state_len = rkey + 1;
			logDbg("Set maximum raw keycode to %d", rkey + 1);
		}
	}

	strfreev(key);
	strfreev(val);
}

static void load_id_keymap(struct wlContext *ctx)
{
	char **key, **val, *endstr;
	int i, count,lkey, rkey;
	key = NULL;
	val = NULL;
	if (ctx->input.id_keymap) {
		free(ctx->input.id_keymap);
	}
	/* start with the known synergy maximum */
	ctx->input.id_count = 0xF000;
	logDbg("max key: %zu", ctx->input.id_count);
	if ((count = configReadFullSection("id-keymap", &key, &val)) != -1) {
		/* slightly inefficient approach, but it will actually work
		 * First pass -- just find the *real* maximum id keycode */
		for (i = 0; i < count; ++i) {
			errno = 0;
			lkey = strtol(key[i], &endstr, 0);
			if (errno || endstr == key[i])
				continue;
			if (lkey >= ctx->input.id_count) {
				ctx->input.id_count = lkey + 1;
				logDbg("max id update: %zu", ctx->input.id_count);

			}
		}
	}
	/* initialize everything */
	ctx->input.id_keymap = xcalloc(ctx->input.id_count, sizeof(*ctx->input.id_keymap));
	/* and set everything as invalid initially, to trigger raw key map */
	if (ctx->input.id_keymap_valid) {
		free(ctx->input.id_keymap_valid);
	}
	ctx->input.id_keymap_valid = xcalloc(ctx->input.id_count, sizeof(*ctx->input.id_keymap_valid));
	/* and second pass -- store any actually mappings, set valid */
	for (i = 0; i < count; ++i) {
		errno = 0;
		lkey = strtol(key[i], &endstr, 0);
		if (errno || endstr == key[i])
			continue;
		errno = 0;
		rkey = strtol(val[i], &endstr, 0);
		if (errno || endstr == val[i])
			continue;
		ctx->input.id_keymap[lkey] = rkey;
		ctx->input.id_keymap_valid[lkey] = true;
		logDbg("set id key map: %d = %d", lkey, ctx->input.id_keymap[lkey]);
		if (rkey >= ctx->input.key_press_state_len) {
			ctx->input.key_press_state_len = rkey + 1;
			logDbg("Set maximum raw keycode to %d", rkey + 1);
		}
	}

	strfreev(key);
	strfreev(val);
}

int wlKeySetConfigLayout(struct wlContext *ctx)
{
	int ret = 0;

	/* ensure that we've given everything a chance to give us a proper
	   default */
	if (!ctx->kb_map) {
		wl_display_dispatch(ctx->display);
		wl_display_roundtrip(ctx->display);
	}

	char *default_map = ctx->kb_map;
	logDbg("Will default to map %s", default_map);
	char *keymap_str = configTryStringFull("xkb_keymap", default_map);
	local_mod_init(ctx, keymap_str);
	ret = !ctx->input.key_map(&ctx->input, keymap_str);
	ctx->input.key_press_state_len = 0;
	load_raw_keymap(ctx);
	load_id_keymap(ctx);
	ctx->input.key_press_state = xcalloc(ctx->input.key_press_state_len, sizeof(*ctx->input.key_press_state));
	free(keymap_str);
	return ret;
}

void wlKeyRaw(struct wlContext *ctx, int key, int state)
{
	size_t i;

	/* keep track of raw keystate size */
	if (key >= ctx->input.key_press_state_len) {
		logDbg("Resizing key press state array from %zu to %zu", ctx->input.key_press_state_len, key + 1);
		ctx->input.key_press_state = xreallocarray (ctx->input.key_press_state, key + 1, sizeof(*ctx->input.key_press_state));
		for (i = ctx->input.key_press_state_len; i < (key + 1); ++i) {
			ctx->input.key_press_state[i] = 0;
		}
		ctx->input.key_press_state_len = key + 1;
	}

	if (!ctx->input.key_press_state[key] && !state) {
		logDbg("Superfluous release of raw key %d", key);
		return;
	}

	if (key > xkb_keymap_max_keycode(ctx->input.xkb_map)) {
		logDbg("keycode greater than xkb maximum, mod not tracked");
	} else {
		xkb_state_update_key(ctx->input.xkb_state, key, state);
		xkb_mod_mask_t depressed = xkb_state_serialize_mods(ctx->input.xkb_state, XKB_STATE_MODS_DEPRESSED);
		xkb_mod_mask_t latched = xkb_state_serialize_mods(ctx->input.xkb_state, XKB_STATE_MODS_LATCHED);
		xkb_mod_mask_t locked = xkb_state_serialize_mods(ctx->input.xkb_state, XKB_STATE_MODS_LOCKED);
		xkb_layout_index_t group = xkb_state_serialize_layout(ctx->input.xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
		logDbg("Modifiers: depressed: %x latched: %x locked: %x group: %x", depressed, latched, locked, group);
	}

	logDbg("Keycode: %d, state %d", key, state);
	ctx->input.key_press_state[key] += state ? 1 : -1;
	ctx->input.key(&ctx->input, key, state);
}


void wlKey(struct wlContext *ctx, int key, int id, int state)
{
	int oldkey = key;

	if ((id < ctx->input.id_count) && ctx->input.id_keymap_valid[id]) {
		key = ctx->input.id_keymap[id];
		logDbg("Key %d remapped to %d by id %d", oldkey, key, id);
	} else {
		if (key >= ctx->input.key_count) {
			logWarn("Key %d outside configured keymap, dropping", key);
			return;
		}
		key = ctx->input.raw_keymap[key];
		if (key != oldkey) {
			logDbg("Key %d remapped to %d", oldkey, key);
		}
	}
	if (key == -1) {
		logDbg("Dropping key mapped to -1");
		return;
	}
	wlKeyRaw(ctx, key, state);
}

void wlKeyReleaseAll(struct wlContext *ctx)
{
	size_t i;
	for (i = 0; i < ctx->input.key_press_state_len; ++i) {
		while (ctx->input.key_press_state[i]) {
			logDbg("Release all: key %zd, pressed %d times", i, ctx->input.key_press_state[i]);
			wlKeyRaw(ctx, i, 0);
		}
	}
}


void wlMouseRelativeMotion(struct wlContext *ctx, int dx, int dy)
{
	ctx->input.mouse_rel_motion(&ctx->input, dx, dy);
}
void wlMouseMotion(struct wlContext *ctx, int x, int y)
{
	ctx->input.mouse_motion(&ctx->input, x, y);
}
void wlMouseButton(struct wlContext *ctx, int button, int state)
{
	logDbg("Mouse button: %d (mapped to %d), state: %d", button, ctx->input.button_map[button], state);
	ctx->input.mouse_button(&ctx->input, ctx->input.button_map[button], state);
}
void wlMouseWheel(struct wlContext *ctx, signed short dx, signed short dy)
{
	ctx->input.mouse_wheel(&ctx->input, dx, dy);
}
