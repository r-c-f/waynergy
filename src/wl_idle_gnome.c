#include "wayland.h"
#include <signal.h>
#include <spawn.h>

static void inhibit_stop(struct wlIdle *idle)
{
	pid_t *inhibitor = idle->state;
        if (*inhibitor == -1) {
                logDbg("gnome-session-inhibit not running");
                return;
        }
        logDbg("Stopping gnome-session-inhibit");
        kill(*inhibitor, SIGTERM);
	*inhibitor = -1;
}
static void inhibit_start(struct wlIdle *idle)
{
	pid_t *inhibitor = idle->state;
        char *argv[] = {
                "gnome-session-inhibit",
                "--inhibit",
                "idle",
                "--inhibit-only",
                NULL,
        };

        if (*inhibitor != -1) {
                inhibit_stop(idle);
        }

        logDbg("Starting gnome-session-inhibit");
        if (posix_spawnp(inhibitor, argv[0], NULL, NULL, argv, environ)) {
                *inhibitor = -1;
                logPErr("Could not spawn gnome-session-inhibit");
        }
}

bool wlIdleInitGnome(struct wlContext *ctx)
{
	if (strcmp(ctx->comp_name, "gnome-shell")) {
		logDbg("gnome inhibitor only works with 'gnome-shell', we have '%s'", ctx->comp_name);
		return false;
	}
	pid_t *inhibitor = xmalloc(sizeof(*inhibitor));
	*inhibitor = -1;
	ctx->idle.wl_ctx = ctx;
	ctx->idle.state = inhibitor;
	ctx->idle.inhibit_start = inhibit_start;
	ctx->idle.inhibit_stop = inhibit_stop;
	return true;
}
