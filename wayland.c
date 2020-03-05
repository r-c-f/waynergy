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




void wlOutputAppend(struct wlOutput **outputs, struct wl_output *output, struct zxdg_output_v1 *xdg_output)
{
	struct wlOutput *l;
	struct wlOutput *n = xmalloc(sizeof(*n));
	memset(n, 0, sizeof(*n));
	n->wl_output = output;
	n->xdg_output = xdg_output;
	if (!*outputs) {
		*outputs = n;
	} else {
		for (l = *outputs; l->next; l = l->next);
		l->next = n;
	}
}
struct wlOutput *wlOutputGet(struct wlOutput *outputs, struct wl_output *wl_output)
{
	struct wlOutput *l;
	for (l = outputs; l; l = l->next) {
		if (l->wl_output == wl_output) {
			break;
		}
	}
	return l;
}
struct wlOutput *wlOutputGetXdg(struct wlOutput *outputs, struct zxdg_output_v1 *xdg_output)
{
	struct wlOutput *l;
	for (l = outputs; l; l = l->next) {
		if (l->xdg_output == xdg_output)
			break;
	}
	return l;
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
void (*wlOnOutputsUpdated)(struct wlOutput *outputs) = NULL;
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
		wlOutputAppend(&ctx->outputs, wl_output, xdg_output);
	}

}
static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
	//nothing here mf
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
static inline uint32_t wlTS(struct wlContext *ctx)
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

/* create a layout file descriptor */
static int set_layout(struct wlContext *ctx)
{
	int ret = 0;
	int fd;
	char nul = 0;
	char *keymap_str = configTryString("xkb_keymap", "xkb_keymap { \
	xkb_keycodes  { include \"xfree86+aliases(qwerty)\"	}; \
	xkb_types     { include \"complete\"	}; \
	xkb_compat    { include \"complete\"	}; \
	xkb_symbols   { include \"pc+us+inet(evdev)\"	}; \
	xkb_geometry  { include \"pc(pc105)\"	}; \
};");
	if ((fd = osGetAnonFd()) == -1) {
		ret = 1;
		goto done;
	}
	size_t keymap_size = strlen(keymap_str) + 1;
	if (lseek(fd, keymap_size, SEEK_SET) != keymap_size) {
		ret = 2;
		goto done;
	}
	if (write(fd, &nul, 1) != 1) {
		ret = 3;
		goto done;
	}
	void *ptr = mmap(NULL, keymap_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (ptr == MAP_FAILED) {
		ret = 4;
		goto done;
	}
	strcpy(ptr, keymap_str);
	zwp_virtual_keyboard_v1_keymap(ctx->keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, keymap_size);
done:
	free(keymap_str);
	return ret;
}

static bool local_mod_init(void);

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

	ctx->pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(ctx->pointer_manager, ctx->seat);
	wl_display_dispatch(ctx->display);
	wl_display_roundtrip(ctx->display);
	ctx->keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(ctx->keyboard_manager, ctx->seat);
	wl_display_dispatch(ctx->display);
	wl_display_roundtrip(ctx->display);
	if(set_layout(ctx)) {
		return 1;
	}
	local_mod_init();
	return 0;
}
void wlResUpdate(struct wlContext *ctx, int width, int height)
{
	ctx->width = width;
	ctx->height = height;
}
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
	} else {
		zwlr_virtual_pointer_v1_axis_discrete(ctx->pointer, wlTS(ctx), 0, wl_fixed_from_int(-15), -1);
	}
	zwlr_virtual_pointer_v1_frame(ctx->pointer);
	wl_display_flush(ctx->display);
}
#define SMOD_SHIFT 		0x0001
#define XMOD_SHIFT 		0x0001

#define SMOD_CONTROL 		0x0002
#define XMOD_CONTROL 		0x0004

#define SMOD_ALT 		0x0004
#define XMOD_ALT 		0x0008

#define SMOD_META 		0x0008
#define XMOD_META 		0x0008

#define SMOD_SUPER 		0x0010
#define XMOD_SUPER 		0x0040

static uint32_t smod_to_xmod[][2] = {
	{SMOD_SHIFT, XMOD_SHIFT},
	{SMOD_CONTROL, XMOD_CONTROL},
	{SMOD_ALT, XMOD_ALT},
	{SMOD_META, XMOD_META},
	{SMOD_SUPER, XMOD_SUPER},
	{0,0}
};
static inline uint32_t wlModConvert(uint32_t smod)
{
	uint32_t xmod = 0;
	int i;
	for (i = 0; smod_to_xmod[i][0] && smod_to_xmod[i][1]; ++i) {
		if (smod & smod_to_xmod[i][0])
			xmod |= smod_to_xmod[i][1];
	}
	return xmod;
}

/* FIXME XXX XXX
 * jesus christ this is fucking hacky as shit we should automate this behind
 * xkb or something fuck me
*/
static long *local_mod = NULL;
static size_t local_mod_len = 0;

static bool local_mod_init(void) {
#ifdef USE_INTRINSIC_MASK
	char **lines;
	size_t i, l;
	long key;
	char *conf[] = {
		"intrinsic_mask/shift",
		"instrinsic_mask/control",
		"intrinsic_mask/alt",
		"intrinsic_mask/meta",
		"intrinsic_mask/super",
		NULL
	};
	uint32_t masks[] = {
		XMOD_SHIFT,
		XMOD_CONTROL,
		XMOD_ALT,
		XMOD_META,
		XMOD_SUPER,
		0
	};
	//start off with something sensible -- 255 seems good
	local_mod_len = 256;
	local_mod = xcalloc(sizeof(*local_mod), local_mod_len);
	for (i = 0; conf[i] && masks[i]; ++i) {
		if (!(lines = configReadLines(conf[i])))
			continue;
		for (l = 0; lines[l]; ++l) {
			errno = 0;
			key = strtol(lines[l], NULL, 0);
			if (errno)
				continue;
			/* resize array if needed */
			if (key >= local_mod_len) {
				local_mod = xrealloc(local_mod, sizeof(*local_mod) * (key + 1));
				/* be sure to zero all that shit */
				memset(local_mod + local_mod_len, 0, (key + 1) - local_mod_len);
				local_mod_len = key + 1;
			}
			/* now we process as normal */
			local_mod[key] |= masks[i];
		}
		strfreev(lines);
	}
#endif
	return true;
}



static inline long intrinsic_mask(int key)
{
#ifdef USE_INTRINSIC_MASK
	if (key < local_mod_len) {
		return local_mod[key];
	}
#endif
	return 0;
}

void wlKey(struct wlContext *ctx, int key, int state, uint32_t mask)
{
	key -= 8;
	int xkb_sym;
	uint32_t xmodmask = wlModConvert(mask);
	if ((key & 0xE000) == 0xE000) {
		xkb_sym = key + 0x1000;
	} else {
		xkb_sym = key;
	}
	logDbg("Got modifier mask: %" PRIx32, xmodmask); 
	xmodmask |= intrinsic_mask(key + 8);
	zwp_virtual_keyboard_v1_modifiers(ctx->keyboard, xmodmask, 0, 0, 0);
	zwp_virtual_keyboard_v1_key(ctx->keyboard, wlTS(ctx), xkb_sym, state);
	if (!state) {
		zwp_virtual_keyboard_v1_modifiers(ctx->keyboard, 0, 0, 0, 0);
	}
	wl_display_flush(ctx->display);
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


/* FIXME XXX: hacky as fuck way to inhibit idle -- we just execute some commands */
void wlIdleInhibit(struct wlContext *ctx, bool on)
{
	char *cmd = configTryString(on ? "idle-inhibit/cmd-on" : "idle-inhibit/cmd-off", NULL);
	if (cmd)
		system(cmd);
	free(cmd);
}
