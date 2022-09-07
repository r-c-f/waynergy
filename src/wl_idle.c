#include "wayland.h"
#include <signal.h>
#include <spawn.h>

pid_t gnome_session_inhibit = -1;

static void gnome_inhibit_stop(void)
{
	if (gnome_session_inhibit == -1) {
		logDbg("gnome-session-inhibit not running");
		return;
	}
	logDbg("Stopping gnome-session-inhibit");
	kill(gnome_session_inhibit, SIGTERM);
}
static bool gnome_inhibit_start(void)
{
	char *argv[] = {
		"gnome-session-inhibit",
		"--inhibit",
		"idle",
		"--inhibit-only",
		NULL,
	};

	if (gnome_session_inhibit != -1) {
		gnome_inhibit_stop();
	}

	logDbg("Starting gnome-session-inhibit");
	if (posix_spawnp(&gnome_session_inhibit, argv[0], NULL, NULL, argv, environ)) {
		gnome_session_inhibit = -1;
		return false;
	}

	return true;
}

static xkb_keycode_t idle_key;
int idle_key_raw = -1;
static void on_idle_mouse(void *data, struct org_kde_kwin_idle_timeout *timeout)
{
	logDbg("Got idle event, responding with zero mouse move");
	struct wlContext *ctx = data;
	wlMouseRelativeMotion(ctx, 0, 0);
}
static void on_idle_key(void *data, struct org_kde_kwin_idle_timeout *timeout)
{
	logDbg("Got idle event, responding with keypress");
	struct wlContext *ctx = data;
	//Second try at this -- press a key we do not care about
	if (idle_key_raw != -1) {
		wlKeyRaw(ctx, idle_key_raw, true);
		wlKeyRaw(ctx, idle_key_raw, false);
	} else {
		wlKey(ctx, idle_key, 0, true);
		wlKey(ctx, idle_key, 0, false);
	}
}
static void on_resumed(void *data, struct org_kde_kwin_idle_timeout *timeout)
{
	logDbg("Got resume event");
}

static struct org_kde_kwin_idle_timeout_listener idle_timeout_listener = {
	.idle = NULL,
	.resumed = on_resumed
};

void wlIdleInhibit(struct wlContext *ctx, bool on)
{
	long idle_time;
	char *idle_method;
	char *idle_keyname;
	char *method_default;

	logDbg("got idle inhibit request, state: %s", on ? "on" : "off");
	if (!configTryBool("idle-inhibit/enable", true)) {
		logDbg("idle inhibition disabled, ignoring request");
		return;
	}

	/* set default based on compositor in use */
	if (!strcmp(ctx->comp_name, "gnome-shell")) {
		method_default = "gnome";
	} else { /* most things should support the idle manager */
		method_default = "mouse";
	}
	logDbg("Defaulting to idle-inhibit/method %s for compositor %s",
		method_default,
		ctx->comp_name
	);

	if (on) {
		if (ctx->idle_timeout) {
			logDbg("Idle already inhibited");
			return;
		}
		idle_time = configTryLong("idle-inhibit/interval", 30);
		idle_method = configTryString("idle-inhibit/method", method_default);

		/* use GNOME's session inhibitor */
		if (!strcmp(idle_method, "gnome")) {
			gnome_inhibit_start();
			free(idle_method);
			return;
		}
		/* use the idle manager hack */
		if (!ctx->idle_manager) {
			logWarn("Idle inhibit request, but no idle manager support");
			free(idle_method);
			return;
		}
		if (!strcmp(idle_method, "mouse")) {
			idle_timeout_listener.idle = on_idle_mouse;
		} else if (!strcmp(idle_method, "key")) {
			idle_timeout_listener.idle = on_idle_key;
			/* first try a raw keycode for idle, because in case
			 * of uinput xkb map might be rather useless */
			idle_key_raw = configTryLong("idle-inhibit/keycode", -1);
			idle_keyname = configTryString("idle-inhibit/keyname", "HYPR");
			idle_key = xkb_keymap_key_by_name(ctx->input.xkb_map, idle_keyname);
			free(idle_keyname);
		} else {
			logErr("Unknown idle inhibition method %s, ignoring inhibit request", idle_method);
			free(idle_method);
			return;
		}
		free(idle_method);

		ctx->idle_timeout = org_kde_kwin_idle_get_idle_timeout(ctx->idle_manager, ctx->seat, idle_time * 1000);
		if (!ctx->idle_timeout) {
			logErr("Could not get idle timeout");
			return;
		}
		org_kde_kwin_idle_timeout_add_listener(ctx->idle_timeout, &idle_timeout_listener, ctx);
		wl_display_flush(ctx->display);
	} else {
		if (gnome_session_inhibit != -1) {
			gnome_inhibit_stop();
			return;
		}
		if (!ctx->idle_timeout) {
			logDbg("Idle already not inhibited");
			return;
		}
		org_kde_kwin_idle_timeout_release(ctx->idle_timeout);
		wl_display_flush(ctx->display);
		ctx->idle_timeout = NULL;
	}
}
