#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <sys/mman.h>
#include "config.h"
#include "os.h"
#include "xmem.h"
#include "wayland.h"
#include <stdbool.h>
#include "log.h"
#include "sig.h"

static void wl_log_handler(const char *fmt, va_list ap)
{
	logOutV(LOG_ERR, fmt, ap);
}

static char *display_strerror(int error)
{
	switch (error) {
		case WL_DISPLAY_ERROR_INVALID_OBJECT:
			return "server couldn't find object";
		case WL_DISPLAY_ERROR_INVALID_METHOD:
			return "method doesn't exist on specified interface or malformed request";
		case WL_DISPLAY_ERROR_NO_MEMORY:
			return "server is out of memory";
		case WL_DISPLAY_ERROR_IMPLEMENTATION:
			return "compositor implementation error";
	}
	return "unkown error";
}

void wlDisplayFlush(struct wlContext *ctx)
{
	int error;

	if ((error = wl_display_get_error(ctx->display))) {
		logErr("Wayland display error %d: %s", error, display_strerror(error));
		ExitOrRestart(2);
	}
	wl_display_flush(ctx->display);
}

void wlOutputAppend(struct wlOutput **outputs, struct wl_output *output, struct zxdg_output_v1 *xdg_output, uint32_t wl_name)
{
	struct wlOutput *l;
	struct wlOutput *n = xmalloc(sizeof(*n));
	memset(n, 0, sizeof(*n));
	n->wl_output = output;
	n->xdg_output = xdg_output;
	n->wl_name = wl_name;
	if (!*outputs) {
		*outputs = n;
	} else {
		for (l = *outputs; l->next; l = l->next);
		l->next = n;
	}
}
struct wlOutput *wlOutputGet(struct wlOutput *outputs, struct wl_output *wl_output)
{
	struct wlOutput *l = NULL;
	for (l = outputs; l; l = l->next) {
		if (l->wl_output == wl_output) {
			break;
		}
	}
	return l;
}
struct wlOutput *wlOutputGetXdg(struct wlOutput *outputs, struct zxdg_output_v1 *xdg_output)
{
	struct wlOutput *l = NULL;
	for (l = outputs; l; l = l->next) {
		if (l->xdg_output == xdg_output)
			break;
	}
	return l;
}
struct wlOutput *wlOutputGetWlName(struct wlOutput *outputs, uint32_t wl_name)
{
	struct wlOutput *l = NULL;
	for (l = outputs; l; l = l->next) {
		if (l->wl_name == wl_name)
			break;
	}
	return l;
}
void wlOutputRemove(struct wlOutput **outputs, struct wlOutput *output)
{
	struct wlOutput *prev = NULL;
	if (!outputs)
		return;
	if (*outputs != output) {
		for (prev = *outputs; prev; prev = prev->next) {
			if (prev->next == output)
				break;
		}
		if (!prev) {
			logErr("Tried to remove unknown output");
			return;
		}
		prev->next = prev->next->next;
	} else {
		*outputs = NULL;
	}
	free(output->name);
	free(output->desc);
	if (output->xdg_output) {
		zxdg_output_v1_destroy(output->xdg_output);
	}
	if (output->wl_output) {
		wl_output_destroy(output->wl_output);
	}
	free(output);
}



static void output_geometry(void *data, struct wl_output *wl_output, int32_t x, int32_t y, int32_t physical_width, int32_t physical_height, int32_t subpixel, const char *make, const char *model, int32_t transform)
{
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGet(ctx->outputs, wl_output);
	if (!output) {
		logErr("Output not found");
		return;
	}
	logDbg("Mutating output...");
	if (output->have_log_pos) {
		logDbg("Except not really, because the logical position outweighs this");
		return;
	}
	output->complete = false;
	output->x = x;
	output->y = y;
	logDbg("Got output at position %d,%d", x, y);
}
static void output_mode(void *data, struct wl_output *wl_output, uint32_t flags, int32_t width, int32_t height, int32_t refresh)
{
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGet(ctx->outputs, wl_output);
	bool preferred = flags & WL_OUTPUT_MODE_PREFERRED;
	bool current = flags & WL_OUTPUT_MODE_CURRENT;
	logDbg("Got %smode: %dx%d@%d%s", current ? "current " : "", width, height, refresh, preferred ? "*" : "");
	if (!output) {
		logErr("Output not found in list");
		return;
	}
	if (current) {
		if (!preferred) {
			logInfo("Not using preferred mode on output -- check config");
		}
		logDbg("Mutating output...");
		if (output->have_log_size) {
			logDbg("Except not really, because logical size outweighs this");
			return;
		}
		output->complete = false;
		output->width = width;
		output->height = height;
	}
}
static void output_scale(void *data, struct wl_output *wl_output, int32_t factor)
{
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGet(ctx->outputs, wl_output);
	logDbg("Got scale factor for output: %d", factor);
	if (!output) {
		logErr("Output not found in list");
		return;
	}
	logDbg("Mutating output...");
	output->complete = false;
	output->scale = factor;
}
static void output_done(void *data, struct wl_output *wl_output)
{
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGet(ctx->outputs, wl_output);
	if (!output) {
		logErr("Output not found in list");
		return;
	}
	output->complete = true;
	if (output->name) {
		logInfo("Output name: %s", output->name);
	}
	if (output->desc) {
		logInfo("Output description: %s", output->desc);
	}
	logInfo("Output updated: %dx%d at %d, %d (scale: %d)",
			output->width,
			output->height,
			output->x,
			output->y,
			output->scale);

	/* fire event if all outputs are complete. */
	bool complete = true;
	for (output = ctx->outputs; output; output = output->next) {
		complete = complete && output->complete;
	}
	if (complete) {
		logDbg("All outputs updated, triggering event");
		if (ctx->on_output_update)
			ctx->on_output_update(ctx);
	}
}

static void xdg_output_pos(void *data, struct zxdg_output_v1 *xdg_output, int32_t x, int32_t y)
{
	logDbg("Got xdg output position: %d, %d", x, y);
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGetXdg(ctx->outputs, xdg_output);
	if (!output) {
		logErr("Could not find xdg output");
		return;
	}
	logDbg("Mutating output from xdg_output event");
	output->complete = false;
	output->have_log_pos = true;
	output->x = x;
	output->y = y;
}
static void xdg_output_size(void *data, struct zxdg_output_v1 *xdg_output, int32_t width, int32_t height)
{
	logDbg("Got xdg output size: %dx%d", width, height);
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGetXdg(ctx->outputs, xdg_output);
	if (!output) {
		logErr("Could not find xdg output");
		return;
	}
	logDbg("Mutating output from xdg_output event");
	output->complete = false;
	output->have_log_size = true;
	output->width = width;
	output->height = height;
}
static void xdg_output_name(void *data, struct zxdg_output_v1 *xdg_output, const char *name)
{
	logDbg("Got xdg output name: %s", name);
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGetXdg(ctx->outputs, xdg_output);
	if (!output) {
		logErr("Could not find xdg output");
		return;
	}
	logDbg("Mutating output from xdg_output event");
	output->complete = false;
	if (output->name) {
		free(output->name);
	}
	output->name = xstrdup(name);
}
static void xdg_output_desc(void *data, struct zxdg_output_v1 *xdg_output, const char *desc)
{
	logDbg("Got xdg output desc: %s", desc);
	struct wlContext *ctx = data;
	struct wlOutput *output = wlOutputGetXdg(ctx->outputs, xdg_output);
	if (!output) {
		logErr("Could not find xdg output");
		return;
	}
	logDbg("Mutating output from xdg_output event");
	output->complete = false;
	if (output->desc) {
		free(output->desc);
	}
	output->desc = xstrdup(desc);
}




static struct zxdg_output_v1_listener xdg_output_listener = {
	.logical_position = xdg_output_pos,
	.logical_size = xdg_output_size,
	.name = xdg_output_name,
	.description = xdg_output_desc,
};

static struct wl_output_listener output_listener = {
	.geometry = output_geometry,
	.mode = output_mode,
	.done = output_done,
	.scale = output_scale
};

static void keyboard_keymap(void *data, struct wl_keyboard *wl_kb, uint32_t format, int32_t fd, uint32_t size)
{
	struct wlContext *ctx = data;
	char *buf = NULL;
	char *map = NULL;
	if (!configTryBool("wl_keyboard_map", true)) {
		goto cleanup;
	}

	if ((map = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		logPDbg("Could not map keymap from fd");
		goto cleanup;
	}

	buf = xcalloc(size + 1, 1);
	memcpy(buf, map, size);
	free(ctx->kb_map);
	ctx->kb_map = buf;
	buf = NULL;
	logDbg("Current keymap updated");
	free(buf);
	munmap(map, size);
cleanup:
	close(fd);
}

static void keyboard_enter(void *data, struct wl_keyboard *wl_kb, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
}
static void keyboard_leave(void *data, struct wl_keyboard *wl_kb, uint32_t serial, struct wl_surface *surface)
{
}
static void keyboard_key(void *data, struct wl_keyboard *wl_kb, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
}
static void keyboard_mod(void *data, struct wl_keyboard *wl_kb, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
}
static void keyboard_rep(void *data, struct wl_keyboard *wl_kb, int32_t rate, int32_t delay)
{
}

static struct wl_keyboard_listener keyboard_listener = {
	.keymap = keyboard_keymap,
	.enter = keyboard_enter,
	.leave = keyboard_leave,
	.key = keyboard_key,
	.modifiers = keyboard_mod,
	.repeat_info = keyboard_rep,
};

static void seat_capabilities(void *data, struct wl_seat *wl_seat, uint32_t caps)
{
	struct wlContext *ctx = data;
	ctx->seat_caps = caps;
	if (caps & WL_SEAT_CAPABILITY_POINTER) {
		logDbg("Seat has pointer");
	}
	if (caps & WL_SEAT_CAPABILITY_KEYBOARD) {
		logDbg("Seat has keyboard");
		ctx->kb = wl_seat_get_keyboard(wl_seat);
		wl_keyboard_add_listener(ctx->kb, &keyboard_listener, ctx);
		wl_display_dispatch(ctx->display);
		wl_display_roundtrip(ctx->display);
	}
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
	logDbg("Seat name is %s", name);
}

static struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};

static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	struct wlContext *ctx = data;
	struct wl_output *wl_output;
	struct zxdg_output_v1 *xdg_output;
	if (strcmp(interface, wl_seat_interface.name) == 0) {
		ctx->seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
		wl_seat_add_listener(ctx->seat, &seat_listener, ctx);
	} else if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
		ctx->pointer_manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
	} else if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
		ctx->keyboard_manager = wl_registry_bind(registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
	} else if (strcmp(interface, org_kde_kwin_fake_input_interface.name) == 0) {
		ctx->fake_input = wl_registry_bind(registry, name, &org_kde_kwin_fake_input_interface, 4);
	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) ==0) {
		if (version < 3) {
			logInfo("xdg-output too old (version %d)", version);
			return;
		}
		ctx->output_manager = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 3);
		if (ctx->outputs) {
			for (struct wlOutput *output = ctx->outputs; output; output = output->next) {
				if (!output->xdg_output) {
					output->xdg_output = zxdg_output_manager_v1_get_xdg_output(ctx->output_manager, output->wl_output);
					zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener, ctx);
				}
			}
		}
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		wl_output = wl_registry_bind(registry, name, &wl_output_interface, 2);
		wl_output_add_listener(wl_output, &output_listener, ctx);
		if (ctx->output_manager) {
			xdg_output = zxdg_output_manager_v1_get_xdg_output(ctx->output_manager, wl_output);
			zxdg_output_v1_add_listener(xdg_output, &xdg_output_listener, ctx);
		} else {
			xdg_output = NULL;
		}
		wlOutputAppend(&ctx->outputs, wl_output, xdg_output, name);
	} else if (strcmp(interface, org_kde_kwin_idle_interface.name) == 0) {
		logDbg("Got idle manager");
		ctx->idle_manager = wl_registry_bind(registry, name, &org_kde_kwin_idle_interface, version);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	struct wlContext *ctx = data;
	/* possible objects */
	struct wlOutput *output;
	/* for now we only handle the case of outputs going away */
	output = wlOutputGetWlName(ctx->outputs, name);
	if (output) {
		logInfo("Lost output %s", output->name ? output->name : "");
		wlOutputRemove(&ctx->outputs, output);
		ctx->on_output_update(ctx);
	}
}

static const struct wl_registry_listener registry_listener = {
	.global = handle_global,
	.global_remove = handle_global_remove,
};

uint32_t wlTS(struct wlContext *ctx)
{
	struct timespec ts;
	if (ctx->epoch == -1) {
		ctx->epoch = time(NULL);
	}
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec -= ctx->epoch;
	return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

void wlClose(struct wlContext *ctx)
{
	return;
}

bool wlSetup(struct wlContext *ctx, int width, int height, char *backend)
{
	int fd;
	bool input_init = false;

	wl_log_set_handler_client(&wl_log_handler);

	ctx->width = width;
	ctx->height = height;
	ctx->display = wl_display_connect(NULL);
	if (!ctx->display) {
		logErr("Could not connect to display");
		return false;
	}
	ctx->registry = wl_display_get_registry(ctx->display);
	wl_registry_add_listener(ctx->registry, &registry_listener, ctx);
	wl_display_dispatch(ctx->display);
	wl_display_roundtrip(ctx->display);

	/* figure out which compositor we are using */
	fd = wl_display_get_fd(ctx->display);
	ctx->comp_name = osGetPeerProcName(fd);
	logInfo("Compositor seems to be %s", ctx->comp_name);

	if (backend) {
		if (!strcmp(backend, "wlr")) {
			input_init = wlInputInitWlr(ctx);
		} else if (!strcmp(backend, "kde")) {
			input_init = wlInputInitKde(ctx);
		} else if (!strcmp(backend, "uinput")) {
			input_init = wlInputInitUinput(ctx);
		}
		if (!input_init) {
			logErr("Input backend %s not supported", backend);
			return false;
		}
	} else { /* try them all */
		if (wlInputInitWlr(ctx)) {
			logInfo("Using wlroots protocols for virtual input");
		} else if (wlInputInitKde(ctx)) {
			logInfo("Using kde protocols for virtual input");
		} else if (wlInputInitUinput(ctx)) {
			logInfo("Using uinput for virtual input");
		} else {
			logErr("Virtual input not supported by compositor");
			return false;
		}
	}
	/* if these have been used, they'll be -1 in the wlContext and saved
	 * in the input state structure; otherwise, they should be closed */
	if (ctx->uinput_fd[0] != -1)
		close(ctx->uinput_fd[0]);
	if (ctx->uinput_fd[1] != -1)
		close(ctx->uinput_fd[1]);

	if(wlKeySetConfigLayout(ctx)) {
		logErr("Could not configure virtual keyboard");
		return false;
	}

	/* initiailize idle inhibition */
	if (configTryBool("idle-inhibit/enable", true)) {
		if (wlIdleInitKde(ctx)) {
			logInfo("Using KDE idle inhibition protocol");
		} else if (wlIdleInitGnome(ctx)) {
			logInfo("Using GNOME idle inhibition through gnome-session-inhibit");
		} else {
			logInfo("No idle inhibition support");
		}
	} else {
		logInfo("Idle inhibition explicitly disabled");
	}

	/* set FD_CLOEXEC */
	int flags = fcntl(fd, F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(fd, F_SETFD, flags);
	return true;
}
void wlResUpdate(struct wlContext *ctx, int width, int height)
{
	ctx->width = width;
	ctx->height = height;
}

int wlPrepareFd(struct wlContext *ctx)
{
	int fd;

	fd = wl_display_get_fd(ctx->display);
//	while (wl_display_prepare_read(display) != 0) {
//		wl_display_dispatch(display);
//	}
//	wl_display_flush(display);
	return fd;
}

void wlPollProc(struct wlContext *ctx, short revents)
{
	if (revents & POLLIN) {
//		wl_display_cancel_read(display);
		wl_display_dispatch(ctx->display);
	}
	if (revents & POLLHUP) {
		logErr("Lost wayland connection");
		Exit(1);
	}
}
