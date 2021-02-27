#include "wayland.h"


static int button_map[] = {
        0,
        0x110,
        0x112,
        0x111,
        0x150,
        0x151,
        -1
};

void wlMouseRelativeMotion(struct wlContext *ctx, int dx, int dy)
{
        zwlr_virtual_pointer_v1_motion(ctx->pointer, wlTS(ctx), wl_fixed_from_int(dx), wl_fixed_from_int(dy));
        zwlr_virtual_pointer_v1_frame(ctx->pointer);
        wl_display_flush(ctx->display);
}
void wlMouseMotion(struct wlContext *ctx, int x, int y)
{
        zwlr_virtual_pointer_v1_motion_absolute(ctx->pointer, wlTS(ctx), x, y, ctx->width, ctx->height);
        zwlr_virtual_pointer_v1_frame(ctx->pointer);
        wl_display_flush(ctx->display);
}
void wlMouseButtonDown(struct wlContext *ctx, int button)
{
        zwlr_virtual_pointer_v1_button(ctx->pointer, wlTS(ctx), button_map[button], 1);
        zwlr_virtual_pointer_v1_frame(ctx->pointer);
        wl_display_flush(ctx->display);
}
void wlMouseButtonUp(struct wlContext *ctx, int button)
{
        zwlr_virtual_pointer_v1_button(ctx->pointer, wlTS(ctx), button_map[button], 0);
        zwlr_virtual_pointer_v1_frame(ctx->pointer);
        wl_display_flush(ctx->display);
}
void wlMouseWheel(struct wlContext *ctx, signed short dx, signed short dy)
{
        //we are a wheel, after all
        zwlr_virtual_pointer_v1_axis_source(ctx->pointer, 0);
        if (dx < 0) {
                zwlr_virtual_pointer_v1_axis_discrete(ctx->pointer, wlTS(ctx), 1, wl_fixed_from_int(15), 1);
        }else if (dx > 0) {
                zwlr_virtual_pointer_v1_axis_discrete(ctx->pointer, wlTS(ctx), 1, wl_fixed_from_int(-15), -1);
        }
        if (dy < 0) {
                zwlr_virtual_pointer_v1_axis_discrete(ctx->pointer, wlTS(ctx), 0, wl_fixed_from_int(15),1);
        } else if (dy > 0) {
                zwlr_virtual_pointer_v1_axis_discrete(ctx->pointer, wlTS(ctx), 0, wl_fixed_from_int(-15), -1);
        }
        zwlr_virtual_pointer_v1_frame(ctx->pointer);
        wl_display_flush(ctx->display);
}
