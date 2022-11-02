#include "wayland.h"


struct ext_state {
	struct ext_idle_notification_v1_listener listener;
	struct ext_idle_notification_v1 *notification;
	xkb_keycode_t key;
	int key_raw;
	long idle_time;
};

static void on_idle_mouse(void *data, struct ext_idle_notification_v1 *notification)
{
	struct wlIdle *idle = data;
	logDbg("Got idle event, responding with zero mouse move");
	wlMouseRelativeMotion(idle->wl_ctx, 0, 0);
}
static void on_idle_key(void *data, struct ext_idle_notification_v1 *notification)
{
	struct wlIdle *idle = data;
	struct ext_state *kde = idle->state;
	//Second try at this -- press a key we do not care about
	logDbg("Got idle event, responding with keypress");
	if (kde->key_raw != -1) {
		wlKeyRaw(idle->wl_ctx, kde->key_raw, true);
		wlKeyRaw(idle->wl_ctx, kde->key_raw, false);
	} else {
		wlKey(idle->wl_ctx, kde->key, 0, true);
		wlKey(idle->wl_ctx, kde->key, 0, false);
	}
}
static void on_resumed(void *data, struct ext_idle_notification_v1 *notification)
{
	logDbg("Got resume event");
}

static void inhibit_start(struct wlIdle *idle)
{
	struct ext_state *ext = idle->state;

	ext->notification = ext_idle_notifier_v1_get_idle_notification(idle->wl_ctx->idle_notifier, ext->idle_time * 1000, idle->wl_ctx->seat);
	if (!ext->notification) {
		logErr("Could not get idle notification");
		return;
	}
	ext_idle_notification_v1_add_listener(ext->notification, &ext->listener, idle);
	wl_display_flush(idle->wl_ctx->display);
}

static void inhibit_stop(struct wlIdle *idle)
{
	struct ext_state *ext = idle->state;

	if (!ext->notification) {
		logDbg("Idle already not inhibited");
		return;
	}
	ext_idle_notification_v1_destroy(ext->notification);
	wl_display_flush(idle->wl_ctx->display);
	ext->notification = NULL;
}

bool wlIdleInitExt(struct wlContext *ctx)
{
	char *idle_method;
	char *idle_keyname;

	if (!ctx->idle_notifier) {
		logWarn("ext idle inhibit selected, but no idle notifier support");
		return false;
	}
	struct ext_state *ext = xcalloc(1, sizeof(*ext));
	ext->listener.resumed = on_resumed;

	ext->idle_time = configTryLong("idle-inhibit/interval", 30);
	idle_method = configTryString("idle-inhibit/method", "mouse");

	if (!strcmp(idle_method, "mouse")) {
		ext->listener.idled = on_idle_mouse;
	} else if (!strcmp(idle_method, "key")) {
		ext->listener.idled = on_idle_key;
		/* first try a raw keycode for idle, because in case
		 * of uinput xkb map might be rather useless */
		ext->key_raw = configTryLong("idle-inhibit/keycode", -1);
		idle_keyname = configTryString("idle-inhibit/keyname", "HYPR");
		ext->key = xkb_keymap_key_by_name(ctx->input.xkb_map, idle_keyname);
		free(idle_keyname);
	} else {
		logErr("Unknown idle inhibition method %s, initialization failed", idle_method);
		free(idle_method);
		free(ext);
		return false;
	}
	free(idle_method);
	ctx->idle.wl_ctx = ctx;
	ctx->idle.state = ext;
	ctx->idle.inhibit_start = inhibit_start;
	ctx->idle.inhibit_stop = inhibit_stop;
	return true;
}
