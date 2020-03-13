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
#include <spawn.h>
#include "config.h"
#include "os.h"
#include "xmem.h"
#include "wayland.h"
#include <stdbool.h>
#include "log.h"
#include "clip.h"



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

static char *clip_format_mimes_text[] = CLIP_FORMAT_MIMES_TEXT;
static char **clip_format_mimes[] = {clip_format_mimes_text};
static enum uSynergyClipboardFormat clip_format_from_mime(const char *mime)
{
	for (int fmt = 0; fmt < CLIP_FORMAT_COUNT; ++fmt) {
		for (int i = 0; clip_format_mimes[fmt][i]; ++i) {
			if (!strcmp(mime, clip_format_mimes[fmt][i]))
				return fmt;
		}
	}
	return -1;
}
static char *get_static_mime_string(const char *mime_type)
{
	enum uSynergyClipboardFormat fmt = clip_format_from_mime(mime_type);
	if (fmt < 0)
		return NULL;
	for (int i = 0; clip_format_mimes[fmt][i]; ++i) {
		if (!strcmp(clip_format_mimes[fmt][i], mime_type)) {
			return clip_format_mimes[fmt][i];
		}
	}
	return NULL;
}

static void on_offer_mime(void *data, struct zwlr_data_control_offer_v1 *offer, const char *mime_type)
{
	struct wlContext *ctx = data;
	if (ctx->data_offer != offer) {
		logErr("Got MIME type for unknown offer");
		return;
	}
	enum uSynergyClipboardFormat fmt;
	fmt = clip_format_from_mime(mime_type);
	if (fmt == -1) {
		logWarn("Unsupported mime type %s", mime_type);
		return;
	}
	if (ctx->data_offer_mimes[fmt]) {
		logDbg("Already have format %s, ignoring %s", ctx->data_offer_mimes[fmt], mime_type);
		return;
	}
	ctx->data_offer_mimes[fmt] = get_static_mime_string(mime_type);
}
static struct zwlr_data_control_offer_v1_listener data_offer_listener = {
	on_offer_mime
};
static void on_data_offer(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
	logDbg("Got data offer");
	struct wlContext *ctx = data;
	if (ctx->data_offer) {
		for (int i = 0; i < CLIP_FORMAT_COUNT; ++i) {
			ctx->data_offer_mimes[i] = NULL;
		}
		zwlr_data_control_offer_v1_destroy(ctx->data_offer);
	}
	ctx->data_offer = offer;
	zwlr_data_control_offer_v1_add_listener(offer, &data_offer_listener, ctx);
	wl_display_roundtrip(ctx->display);
}
static void on_finished(void *data, struct zwlr_data_control_device_v1 *device)
{
	logWarn("Data control device invalid");
}
extern char **environ;
static void on_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer, enum uSynergyClipboardId id)
{
	pid_t pid;
	posix_spawn_file_actions_t fa;
	int fd[2];
	struct wlContext *ctx = data;
	if (offer != ctx->data_offer) {
		logErr("Unknown offer selected");
		return;
	}
	logDbg("Trying to receive");
	for (int i = 0; i < CLIP_FORMAT_COUNT; ++i) {
		if (!ctx->data_offer_mimes[i]) {
			continue;
		}
		char *argv[] = {
			"swaynergy-clip-update",
			clipMonitorPath[id],
			ctx->data_offer_mimes[i],
			NULL
		};
		pipe(fd);
		posix_spawn_file_actions_init(&fa);
		posix_spawn_file_actions_adddup2(&fa, fd[0], STDIN_FILENO);
		posix_spawn_file_actions_addclose(&fa, fd[1]);
		zwlr_data_control_offer_v1_receive(offer, ctx->data_offer_mimes[i], fd[1]);
		posix_spawnp(&pid, "swaynergy-clip-update", &fa, NULL, argv, environ);
		close(fd[0]);
		close(fd[1]);
		logDbg("Spawned updater for clipboard %d, type %s", id, ctx->data_offer_mimes[i]);
	}
}
static void on_clipboard(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
	on_selection(data, device, offer, SYNERGY_CLIPBOARD_CLIPBOARD);
}
static void on_primary(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
	on_selection(data, device, offer, SYNERGY_CLIPBOARD_SELECTION);
}
static enum uSynergyClipboardId clip_id_from_data_source(struct wlContext *ctx, struct zwlr_data_control_source_v1 *source)
{
	for (int i = 0; i < CLIP_COUNT; ++i) {
		if (ctx->data_source[i] == source)
			return i;
	}
	return -1;
}
static void on_send(void *data, struct zwlr_data_control_source_v1 *source, const char *mime_type, int32_t fd)
{
	struct zwlr_data_control_source_v1 *ctx_source = NULL;
	struct wlContext *ctx = data;
	enum uSynergyClipboardId id;
	enum uSynergyClipboardFormat fmt = clip_format_from_mime(mime_type);
	if (fmt == -1) {
		logErr("Got unknown mime type in source send: %s", mime_type);
		return;
	}
	if ((id = clip_id_from_data_source(ctx, source)) < 0) {
		logErr("Got unknown data source in send event");
		return;
	}
	if (!ctx->data_source_types[id][fmt]) {
		logErr("Got unsupported mime type in request for %d: %s", id, mime_type);
		return;
	}
	/* we do something dumb here*/
	logDbg("Forking to send, because... fuck me");
	if (!fork()) {

		bool ret;
		ret = write_full(fd, ctx->data_source_buf[id][fmt], ctx->data_source_len[id][fmt]);
		close(fd);
		if (!ret) {
			logErr("FORKING SEND FAILED");
			exit(EXIT_FAILURE);
		}
		logDbg("FORKING SEND SUCCEEDED");
		exit(EXIT_SUCCESS);
	}
	close(fd);
}
static void on_cancelled(void *data, struct zwlr_data_control_source_v1 *source)
{
	enum uSynergyClipboardId id;
	struct wlContext *ctx = data;
	logInfo("Got cancel request");
	if ((id = clip_id_from_data_source(ctx, source)) < 0) {
		logErr("Got unknown data source in cancel event");
		return;
	}
	if (id == SYNERGY_CLIPBOARD_CLIPBOARD) {
		zwlr_data_control_device_v1_set_selection(ctx->data_device, NULL);
	} else if (id == SYNERGY_CLIPBOARD_SELECTION) {
		zwlr_data_control_device_v1_set_primary_selection(ctx->data_device, NULL);
	}

	zwlr_data_control_source_v1_destroy(ctx->data_source[id]);
	for (int fmt = 0; fmt < CLIP_FORMAT_COUNT; ++fmt) {
		if (ctx->data_source_types[id][fmt]) {
			free(ctx->data_source_buf[id][fmt]);
			ctx->data_source_buf[id][fmt] = NULL;
			ctx->data_source_types[id][fmt] = false;
		}
	}
	ctx->data_source[id] = NULL;
	wl_display_roundtrip(ctx->display);
}
static struct zwlr_data_control_source_v1_listener data_source_listener = {
	.send = on_send,
	.cancelled = on_cancelled
};

static void source_offer_all_mimes(struct zwlr_data_control_source_v1 *src, enum uSynergyClipboardFormat fmt)
{
	for (int i = 0; clip_format_mimes[fmt][i]; ++i) {
		zwlr_data_control_source_v1_offer(src, clip_format_mimes[fmt][i]);
	}
}
static bool clip_data_changed(struct wlContext *ctx, enum uSynergyClipboardId id, char **data, size_t *len)
{
	for (int fmt = 0; fmt < CLIP_FORMAT_COUNT; ++fmt) {
		if (ctx->data_source_len[id][fmt] != len[fmt]) {
			logDbg("Size differs, data has changed: %d %d", ctx->data_source_len[id][fmt], len[fmt]);
			return true;
		}
		if (memcmp(ctx->data_source_buf[id][fmt], data[fmt], len[fmt])) {
			logDbg("Data differs, has changed");
			return true;
		}
	}
	logDbg("Data is identical");
	return false;
}
bool wlClipAll(struct wlContext *ctx, enum uSynergyClipboardId id, char **data, size_t *len)
{
	if (!clip_data_changed(ctx, id, data, len)) {
		logDbg("Duplicate data, not copying");
		return false;
	}
	if (!ctx->data_manager)
		return false;
	if (ctx->data_source[id]) {
		logInfo("Destroying old data source, id %d", id);
		on_cancelled(ctx, ctx->data_source[id]);
	}
	ctx->data_source[id] = zwlr_data_control_manager_v1_create_data_source(ctx->data_manager);
	zwlr_data_control_source_v1_add_listener(ctx->data_source[id], &data_source_listener, ctx);
	for (int fmt = 0; fmt < CLIP_FORMAT_COUNT; ++fmt) {
		if (data[fmt]) {
			ctx->data_source_types[id][fmt] = true;
			ctx->data_source_len[id][fmt] = len[fmt];
			source_offer_all_mimes(ctx->data_source[id], fmt);
			ctx->data_source_buf[id][fmt] = xmalloc(len[fmt]);
			memmove(ctx->data_source_buf[id][fmt], data[fmt], len[fmt]);
		}
	}
	if (id == SYNERGY_CLIPBOARD_CLIPBOARD) {
		zwlr_data_control_device_v1_set_selection(ctx->data_device, ctx->data_source[id]);
	} else if (id == SYNERGY_CLIPBOARD_SELECTION) {
		zwlr_data_control_device_v1_set_primary_selection(ctx->data_device, ctx->data_source[id]);
	}
	return true;
}

static struct zwlr_data_control_device_v1_listener data_device_listener = {
	on_data_offer,
	on_clipboard,
	on_finished,
	on_primary
};
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
static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	struct wlContext *ctx = data;
	struct wl_output *wl_output;
	struct zxdg_output_v1 *xdg_output;
	if (strcmp(interface, wl_seat_interface.name) == 0) {
		ctx->seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
	} else if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
		ctx->pointer_manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
	} else if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
		ctx->keyboard_manager = wl_registry_bind(registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) ==0) {
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
		wl_output = wl_registry_bind(registry, name, &wl_output_interface, version);
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
	} else if (strcmp(interface, zwlr_data_control_manager_v1_interface.name) == 0) {
		ctx->data_manager = wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 2);
	}
}

static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	int i;
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


static int button_map[] = {
	0,
	0x110,
	0x112,
	0x111,
	0x150,
	0x151,
	-1
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

int wlSetup(struct wlContext *ctx, int width, int height)
{
	ctx->width = width;
	ctx->height = height;
	ctx->display = wl_display_connect(NULL);
	if (!ctx->display) {
		printf("Couldn't connect, yo\n");
		return 1;
	}
	ctx->registry = wl_display_get_registry(ctx->display);
	wl_registry_add_listener(ctx->registry, &registry_listener, ctx);
	wl_display_dispatch(ctx->display);
	wl_display_roundtrip(ctx->display);
	if (ctx->data_manager) {
		ctx->data_device = zwlr_data_control_manager_v1_get_data_device(ctx->data_manager, ctx->seat);
		zwlr_data_control_device_v1_add_listener(ctx->data_device, &data_device_listener, ctx);
	}
	ctx->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(ctx->pointer_manager, ctx->seat);
	wl_display_dispatch(ctx->display);
	wl_display_roundtrip(ctx->display);
	ctx->keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(ctx->keyboard_manager, ctx->seat);
	wl_display_dispatch(ctx->display);
	wl_display_roundtrip(ctx->display);
	if(wlKeySetConfigLayout(ctx)) {
		return 1;
	}
	/* set FD_CLOEXEC */
	int fd = wl_display_get_fd(ctx->display);
	int flags = fcntl(fd, F_GETFD);
	flags |= FD_CLOEXEC;
	fcntl(fd, F_SETFD, flags);
	return 0;
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
}
