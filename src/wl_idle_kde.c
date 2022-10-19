#include "wayland.h"


struct kde_state {
	struct org_kde_kwin_idle_timeout_listener listener;
	struct org_kde_kwin_idle_timeout *timeout;
	xkb_keycode_t key;
	int key_raw;
	long idle_time;
};

static void on_idle_mouse(void *data, struct org_kde_kwin_idle_timeout *timeout)
{
	struct wlIdle *idle = data;
	logDbg("Got idle event, responding with zero mouse move");
	wlMouseRelativeMotion(idle->wl_ctx, 0, 0);
}
static void on_idle_key(void *data, struct org_kde_kwin_idle_timeout *timeout)
{
	struct wlIdle *idle = data;
	struct kde_state *kde = idle->state;
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
static void on_resumed(void *data, struct org_kde_kwin_idle_timeout *timeout)
{
	logDbg("Got resume event");
}

static void inhibit_start(struct wlIdle *idle)
{
	struct kde_state *kde = idle->state;

	kde->timeout = org_kde_kwin_idle_get_idle_timeout(idle->wl_ctx->idle_manager, idle->wl_ctx->seat, kde->idle_time * 1000);
	if (!kde->timeout) {
		logErr("Could not get idle timeout");
		return;
	}
	org_kde_kwin_idle_timeout_add_listener(kde->timeout, &kde->listener, idle);
	wlDisplayFlush(idle->wl_ctx);
}

static void inhibit_stop(struct wlIdle *idle)
{
	struct kde_state *kde = idle->state;

	if (!kde->timeout) {
		logDbg("Idle already not inhibited");
		return;
	}
	org_kde_kwin_idle_timeout_release(kde->timeout);
	wlDisplayFlush(idle->wl_ctx);
	kde->timeout = NULL;
}

bool wlIdleInitKde(struct wlContext *ctx)
{
	char *idle_method;
	char *idle_keyname;

	if (!ctx->idle_manager) {
		logWarn("KDE idle inhibit selected, but no idle manager support");
		return false;
	}
	struct kde_state *kde = xcalloc(1, sizeof(*kde));
	kde->listener.resumed = on_resumed;

	kde->idle_time = configTryLong("idle-inhibit/interval", 30);
	idle_method = configTryString("idle-inhibit/method", "mouse");

	if (!strcmp(idle_method, "mouse")) {
		kde->listener.idle = on_idle_mouse;
	} else if (!strcmp(idle_method, "key")) {
		kde->listener.idle = on_idle_key;
		/* first try a raw keycode for idle, because in case
		 * of uinput xkb map might be rather useless */
		kde->key_raw = configTryLong("idle-inhibit/keycode", -1);
		idle_keyname = configTryString("idle-inhibit/keyname", "HYPR");
		kde->key = xkb_keymap_key_by_name(ctx->input.xkb_map, idle_keyname);
		free(idle_keyname);
	} else {
		logErr("Unknown idle inhibition method %s, initialization failed", idle_method);
		free(idle_method);
		free(kde);
		return false;
	}
	free(idle_method);
	ctx->idle.wl_ctx = ctx;
	ctx->idle.state = kde;
	ctx->idle.inhibit_start = inhibit_start;
	ctx->idle.inhibit_stop = inhibit_stop;
	return true;
}
