#include "wayland.h"

void wlIdleInhibit(struct wlContext *ctx, bool on)
{
	logDbg("Got idle inhibit request: %s", on ? "on" : "off");
	if (on) {
		if (!ctx->idle.inhibit_start) {
			logDbg("No idle inhibition support, ignoring request");
			return;
		}
		ctx->idle.inhibit_start(&ctx->idle);
	} else {
		if (!ctx->idle.inhibit_stop) {
			logDbg("No idle inhibition support, ignoring request");
			return;
		}
		ctx->idle.inhibit_stop(&ctx->idle);
	}
}
