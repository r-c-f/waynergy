#include "wayland.h"


/* Idle-handling stuff */

static void on_idle(void *data, struct org_kde_kwin_idle_timeout *timeout)
{
        logDbg("Got idle event");
        struct wlContext *ctx = data;
        //Second try at this -- press a key we do not care about
        wlKey(ctx, ctx->idle_inhibit_key, true);
}
static void on_resumed(void *data, struct org_kde_kwin_idle_timeout *timeout)
{
        logDbg("Got resume event");
        struct wlContext *ctx = data;
        wlKey(ctx, ctx->idle_inhibit_key, false);
}

static struct org_kde_kwin_idle_timeout_listener idle_timeout_listener = {
        .idle= on_idle,
        .resumed = on_resumed
};

void wlIdleInhibit(struct wlContext *ctx, bool on)
{
        long idle_time;
        logDbg("got idle inhibit request");
        if (!ctx->idle_manager) {
                logWarn("Idle inhibit request, but no idle manager support");
                return;
        }
        if (on) {
                if (ctx->idle_timeout) {
                        logDbg("Idle already inhibited");
                        return;
                }
                idle_time = configTryLong("idle-inhibit/interval", 30);
                ctx->idle_timeout = org_kde_kwin_idle_get_idle_timeout(ctx->idle_manager, ctx->seat, idle_time * 1000);
                if (!ctx->idle_timeout) {
                        logErr("Could not get idle timeout");
                        return;
                }
                org_kde_kwin_idle_timeout_add_listener(ctx->idle_timeout, &idle_timeout_listener, ctx);
        } else {
                if (!ctx->idle_timeout) {
                        logDbg("Idle already not inhibited");
                        return;
                }
                org_kde_kwin_idle_timeout_release(ctx->idle_timeout);
                ctx->idle_timeout = NULL;
        }
}
