#include "wayland.h"
#include <stdbool.h>
#include "log.h"
#include "fdio_full.h"
#include "config.h"
#include <xkbcommon/xkbcommon.h>
#include <spawn.h>
#include <ctype.h>

extern char **environ;

struct state_wlr {
	struct zwlr_virtual_pointer_v1 *pointer;
	int wheel_mult;
	struct zwp_virtual_keyboard_v1 *keyboard;
};

/* create a layout file descriptor */
static bool key_map(struct wlInput *input, char *keymap_str)
{
	struct state_wlr *wlr = input->state;
	int fd;
	if ((fd = osGetAnonFd()) == -1) {
		return false;
	}
	size_t keymap_size = strlen(keymap_str) + 1;
	if (!write_full(fd, keymap_str, keymap_size, 0)) {
		return false;
	}
	zwp_virtual_keyboard_v1_keymap(wlr->keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, keymap_size);
	return true;
}

static void key(struct wlInput *input, int key, int state)
{
	struct state_wlr *wlr = input->state;
	xkb_mod_mask_t depressed = xkb_state_serialize_mods(input->xkb_state, XKB_STATE_MODS_DEPRESSED);
	xkb_mod_mask_t latched = xkb_state_serialize_mods(input->xkb_state, XKB_STATE_MODS_LATCHED);
	xkb_mod_mask_t locked = xkb_state_serialize_mods(input->xkb_state, XKB_STATE_MODS_LOCKED);
	xkb_layout_index_t group = xkb_state_serialize_layout(input->xkb_state, XKB_STATE_LAYOUT_EFFECTIVE);
	zwp_virtual_keyboard_v1_key(wlr->keyboard, wlTS(input->wl_ctx), key - 8, state);
	zwp_virtual_keyboard_v1_modifiers(wlr->keyboard, depressed, latched, locked, group);
	wl_display_flush(input->wl_ctx->display);
}

static void mouse_rel_motion(struct wlInput *input, int dx, int dy)
{
	struct state_wlr *wlr = input->state;
	zwlr_virtual_pointer_v1_motion(wlr->pointer, wlTS(input->wl_ctx), wl_fixed_from_int(dx), wl_fixed_from_int(dy));
	zwlr_virtual_pointer_v1_frame(wlr->pointer);
	wl_display_flush(input->wl_ctx->display);
}
static void mouse_motion(struct wlInput *input, int x, int y)
{
	struct state_wlr *wlr = input->state;
	zwlr_virtual_pointer_v1_motion_absolute(wlr->pointer, wlTS(input->wl_ctx), x, y, input->wl_ctx->width, input->wl_ctx->height);
	zwlr_virtual_pointer_v1_frame(wlr->pointer);
	wl_display_flush(input->wl_ctx->display);
}
static void mouse_button(struct wlInput *input, int button, int state)
{
	struct state_wlr *wlr = input->state;
	zwlr_virtual_pointer_v1_button(wlr->pointer, wlTS(input->wl_ctx), button, state);
	zwlr_virtual_pointer_v1_frame(wlr->pointer);
	wl_display_flush(input->wl_ctx->display);
}
static void mouse_wheel(struct wlInput *input, signed short dx, signed short dy)
{
	struct state_wlr *wlr = input->state;
	//we are a wheel, after all
	zwlr_virtual_pointer_v1_axis_source(wlr->pointer, 0);
	if (dx < 0) {
		zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 1, wl_fixed_from_int(15), wlr->wheel_mult);
	}else if (dx > 0) {
		zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 1, wl_fixed_from_int(-15), -1 * wlr->wheel_mult);
	}
	if (dy < 0) {
		zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 0, wl_fixed_from_int(15), wlr->wheel_mult);
	} else if (dy > 0) {
		zwlr_virtual_pointer_v1_axis_discrete(wlr->pointer, wlTS(input->wl_ctx), 0, wl_fixed_from_int(-15), -1 * wlr->wheel_mult);
	}
	zwlr_virtual_pointer_v1_frame(wlr->pointer);
	wl_display_flush(input->wl_ctx->display);
}

size_t get_nums(size_t count, long *dst, char *buf)
{
	size_t i;
	char *next;

	for (i = 0; i < count; ++i) {
		for (; *buf && !isdigit(*buf); ++buf);
		if (!isdigit(*buf)) {
			break;
		}
		errno = 0;
		dst[i] = strtol(buf, &next, 0);
		if (errno) {
			break;
		}
		buf = next;
	}
	return i;
}

static char *get_version_string(char **argv)
{
	size_t buf_len = 0;
	char *buf = NULL;
	pid_t pid;
	int fd[2] = {-1, -1};
	FILE *out = NULL;
	posix_spawn_file_actions_t fa;
	bool ret = true;

	errno = 0;

	if (pipe(fd) == -1) {
		logPDbg("Could not create pipe for version");
		return NULL;
	}

	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, fd[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&fa, fd[0]);
	errno = 0;
	if (posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ)) {
		logPErr("version spawn");
		ret = false;
		goto done;
	}
	close(fd[1]);
	fd[1] = -1;

	if (!(out = fdopen(fd[0], "r"))) {
		logPErr("fdopen for stdout");
		ret = false;
		goto done;
	}
	fd[0] = -1;
	if (getline(&buf, &buf_len, out) == -1) {
		logPErr("getline");
		ret = false;
		goto done;
	}
	fclose(out);
	out = NULL;

	logDbg("Got version string %s", buf);
done:
	if (fd[0] != -1)
		close(fd[0]);
	if (fd[1] != -1)
		close(fd[1]);
	if (out)
		fclose(out);
	if (!ret) {
		free(buf);
		buf = NULL;
	}
	return buf;
}

static bool wayfire_version(long ver[static 3])
{
	char *argv[] = {
		"wayfire",
		"--version",
		NULL
	};
	char *buf;
	size_t valid;
	if (!(buf = get_version_string(argv))) {
		return false;
	}
	valid = get_nums(3, ver, buf);
	free(buf);
	if (valid != 3)
		return false;
	logInfo("Wayfire version is %ld.%ld.%ld", ver[0], ver[1], ver[2]);
	return true;
}
static bool sway_version(long ver[static 2])
{
	char *argv[] = {
		"sway",
		"--version",
		NULL
	};
	char *buf;
	size_t valid;
	if (!(buf = get_version_string(argv))) {
		return false;
	}
	valid = get_nums(2, ver, buf);
	free(buf);
	if (valid != 2)
		return false;
	logInfo("Sway version is %ld.%ld", ver[0], ver[1]);
	return true;
}

static bool wheel_mult_detect(struct wlContext *ctx, int *wheel_mult)
{
	long wayfire_ver[3];
	long sway_ver[2];
	if (!strcmp(ctx->comp_name, "sway")) {
		if (sway_version(sway_ver)) {
			if ((sway_ver[0] <= 1) && (sway_ver[1] <= 7)) {
				*wheel_mult = 1;
				return true;
			}
		}
	} else if (!strcmp(ctx->comp_name, "wayfire")) {
		if (wayfire_version(wayfire_ver)) {
			if ((wayfire_ver[0] == 0) &&
			    (wayfire_ver[1] <= 7) &&
			    (wayfire_ver[2] <= 4)) {
				*wheel_mult = 1;
				return true;
			}
		}
	}

	return false;
}

bool wlInputInitWlr(struct wlContext *ctx)
{
	int wheel_mult_default;
	struct state_wlr *wlr;
	if (!(ctx->pointer_manager && ctx->keyboard_manager)) {
		return false;
	}
	wlr = xmalloc(sizeof(*wlr));
	wlr->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(ctx->pointer_manager, ctx->seat);

	wlr->keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(ctx->keyboard_manager, ctx->seat);
	/* recent wlroots versions behave weirdly with discrete inputs,
	 * accumulating them and only issuing a client event when they've reached
	 * 120. Try to detect sway versions that will be susceptible this, with
	 * a user configuration to override */
	wheel_mult_default = 120;
	if (configTryBool("wlr/auto_wheel_mult", true)) {
		if (wheel_mult_detect(ctx, &wheel_mult_default)) {
			logInfo("Set wheel mult based on compositor version");
		}
	}
	wlr->wheel_mult = configTryLong("wlr/wheel_mult", wheel_mult_default);
	logDbg("Using wheel_mult value of %d", wlr->wheel_mult);
	ctx->input = (struct wlInput) {
		.state = wlr,
		.wl_ctx = ctx,
		.mouse_rel_motion = mouse_rel_motion,
		.mouse_motion = mouse_motion,
		.mouse_button = mouse_button,
		.mouse_wheel = mouse_wheel,
		.key = key,
		.key_map = key_map,
	};
	logInfo("Using wlroots virtual input protocols");
	return true;
}

