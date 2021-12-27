#include "wayland.h"


static bool key_map(struct wlInput *input, char *keymap_str)
{
	/* XXX: this is blatantly inadequate */
	return true;
}

static void key(struct wlInput *input, int key, int state)
{
	struct org_kde_kwin_fake_input *fake = input->state;
	org_kde_kwin_fake_input_keyboard_key(fake, key, state);
	wl_display_flush(input->wl_ctx->display);
}
static void mouse_rel_motion(struct wlInput *input, int dx, int dy)
{
	struct org_kde_kwin_fake_input *fake = input->state;
	org_kde_kwin_fake_input_pointer_motion(fake, wl_fixed_from_int(dx), wl_fixed_from_int(dy));
	wl_display_flush(input->wl_ctx->display);
}

static void mouse_motion(struct wlInput *input, int x, int y)
{
	struct org_kde_kwin_fake_input *fake = input->state;
	org_kde_kwin_fake_input_pointer_motion_absolute(fake, wl_fixed_from_int(x), wl_fixed_from_int(y));
	wl_display_flush(input->wl_ctx->display);
}

static void mouse_button(struct wlInput *input, int button, int state)
{
	struct org_kde_kwin_fake_input *fake = input->state;
	org_kde_kwin_fake_input_button(fake, button, state);
	wl_display_flush(input->wl_ctx->display);
}

static void mouse_wheel(struct wlInput *input, signed short dx, signed short dy)
{
	struct org_kde_kwin_fake_input *fake = input->state;
        if (dx < 0) {
                org_kde_kwin_fake_input_axis(fake, 1, wl_fixed_from_int(15));
        }else if (dx > 0) {
                org_kde_kwin_fake_input_axis(fake, 1, wl_fixed_from_int(-15));
        }
        if (dy < 0) {
                org_kde_kwin_fake_input_axis(fake, 0, wl_fixed_from_int(15));
        } else if (dy > 0) {
                org_kde_kwin_fake_input_axis(fake, 0, wl_fixed_from_int(-15));
        }
        wl_display_flush(input->wl_ctx->display);
}

bool wlInputInitKde(struct wlContext *ctx)
{
	logDbg("Trying KDE fake input protocol for input");
	struct wlInput *input;
	if (!(ctx->fake_input)) {
		logDbg("Fake input not supported");
		return false;
	}
	org_kde_kwin_fake_input_authenticate(ctx->fake_input, "waynergy", "control keyboard and mouse with Synergy/Barrier server");
	input = xcalloc(1, sizeof(*input));
	input->state = ctx->fake_input;
	input->wl_ctx = ctx;
	input->mouse_motion = mouse_motion;
	input->mouse_rel_motion = mouse_rel_motion;
	input->mouse_button = mouse_button;
	input->mouse_wheel = mouse_wheel;
	input->key = key;
	input->key_map = key_map;
	ctx->input = input;
	return true;
}

