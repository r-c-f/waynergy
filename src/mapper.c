/* mapper -- easily create keyboard maps for waynergy consumption */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <wayland-client.h>
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include "xmem.h"
#include "fdio_full.h"
#include "xdg-shell-client-protocol.h"
#include "keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h"
#include "sopt.h"
#include "ssb.h"

static struct sopt optspec[] = {
	SOPT_INITL('h', "help", "Help text"),
	SOPT_INITL('r', "raw", "Use raw keymap section output (default)"),
	SOPT_INITL('s', "skipped", "Include skipped keys in XKB output"),
	SOPT_INIT_ARGL('x', "xkb", SOPT_ARGTYPE_STR, "name", "Use xkb keycode format for output, with given name"),
	SOPT_INIT_ARGL('o', "out", SOPT_ARGTYPE_STR, "path", "Output to given file (defaults to stdout)"),
	SOPT_INIT_ARGL('l', "keylim", SOPT_ARGTYPE_LONG, "limit", "Maximum local key limit"),
	SOPT_INIT_END
};


#define BTN_LEFT 0x110
#define BTN_RIGHT 0x111
#define BTN_MIDDLE 0x112

/* configuration stuff */
static FILE *out_file = NULL;
static char *xkb_out_name = NULL; /* if NULL, we can assume they want raw */
static uint32_t raw_keymap_max_limit = 0xFFFFFFFF;
static bool xkb_skipped = false;

/* Keyboard handling */

static struct xkb_context *xkb_ctx;
static struct xkb_keymap *xkb_keymap;

/* Raw bytes of the currently-compiled keymap, kept only so a later
 * keyboard_keymap() call can tell "genuinely different keymap" apart
 * from "compositor re-sent the same one" (some legitimately do, on
 * events unrelated to which keyboard device is active) without having
 * to guess from context. */
static char *xkb_keymap_buf;
static uint32_t xkb_keymap_buf_size;

static uint32_t *raw_keymap;
static bool *raw_keymap_set;
static uint32_t raw_keymap_min;
static uint32_t raw_keymap_max;
static uint32_t raw_keymap_pos;

static int started;

static char *get_syms_str(void)
{
	char sym_name[64];
	struct ssb s = {0};
	const xkb_keysym_t *syms_out;
	int syms_count, i, levels, level, layouts, layout;

	layouts = xkb_keymap_num_layouts_for_key(xkb_keymap, raw_keymap_pos);
	for (layout = 0; layout < layouts; ++layout) {
		levels = xkb_keymap_num_levels_for_key(xkb_keymap, raw_keymap_pos, layout);
		for (level = 0; level < levels; ++level) {
			if ((syms_count = xkb_keymap_key_get_syms_by_level(xkb_keymap, raw_keymap_pos, layout, level, &syms_out))) {
				for (i = 0; i < syms_count; ++i) {
					xkb_keysym_get_name(syms_out[i], sym_name, sizeof(sym_name));
					ssb_xprintf(&s, "%s ", sym_name);
				}
			}
		}
	}
	return s.buf;
}

static void raw_keymap_print(void)
{
	uint32_t i;

	fprintf(out_file, "[raw-keymap]\n");

	for (i = raw_keymap_min; i < raw_keymap_max; ++i) {
		if (raw_keymap[i] != i) {
			fprintf(out_file, "%u = %u\n", raw_keymap[i], i);
		}
	}
	exit(0);
}

static void xkb_keycodes_print(char *sec_name)
{
	uint32_t i;
	uint32_t mapped_min = 0xFFFFFFFF;
	uint32_t mapped_max = 0;
	uint32_t count = 0;
	const char *name;

	for (i = raw_keymap_min; i < raw_keymap_max; ++i) {
		if (raw_keymap_set[i] || xkb_skipped) {
			++count;
			if (raw_keymap[i] < mapped_min) {
				mapped_min = raw_keymap[i];
			}
			if (raw_keymap[i] > mapped_max) {
				mapped_max = raw_keymap[i];
			}
		}
	}

	if (!count) {
		fprintf(stderr, "No keycodes defined!\n");
		exit(1);
	}

	fprintf(out_file, "xkb_keycodes \"%s\" {\n", sec_name);
	fprintf(out_file, "\tminimum = %u;\n", mapped_min);
	fprintf(out_file, "\tmaximum = %u;\n", mapped_max);


	for (i = raw_keymap_min; i < raw_keymap_max; ++i) {
		if (raw_keymap_set[i] || xkb_skipped) {
			if ((name = xkb_keymap_key_get_name(xkb_keymap, i))) {
				fprintf(out_file, "\t<%s> = %u;\n", name, raw_keymap[i]);
			}
		}
	}

	fprintf(out_file, "};\n");
	exit(0);
}

static void print_output(void)
{
	if (xkb_out_name) {
		fprintf(stderr, "Printing xkb_keycodes section....\n");
		xkb_keycodes_print(xkb_out_name);
	} else {
		fprintf(stderr, "Printing [raw-keymap] section....\n");
		raw_keymap_print();
	}
}

static void raw_keymap_next(void)
{
	++raw_keymap_pos;
	if (raw_keymap_pos > raw_keymap_max) {
		print_output();
	}
}

static void raw_keymap_prev(void)
{
	if (raw_keymap_pos > raw_keymap_min) {
		raw_keymap_set[raw_keymap_pos] = false;
		--raw_keymap_pos;
	}
}

static void raw_keymap_store(uint32_t val)
{
	fprintf(stderr, " keycode %u\n", val);
	raw_keymap[raw_keymap_pos] = val;
	raw_keymap_set[raw_keymap_pos] = true;

	raw_keymap_next();
}

static void raw_keymap_init(void)
{
	uint32_t i;
	/* Safe to call more than once (see keyboard_keymap()): frees
	 * whatever was allocated for a previous keymap first, rather than
	 * leaking it. */
	free(raw_keymap);
	free(raw_keymap_set);
	raw_keymap = xcalloc(raw_keymap_max + 1, sizeof(*raw_keymap));
	raw_keymap_set = xcalloc(raw_keymap_max + 1, sizeof(*raw_keymap_set));
	/* start by assuming that each key simply maps to itself */
	for (i = 0; i < raw_keymap_max; ++i) {
		raw_keymap[i] = i;
	}
	/* and start our position at the minimum keycode */
	raw_keymap_pos = raw_keymap_min;
}

static void raw_keymap_prompt(void)
{
	const char *key_name;
	char *sym_name; /* for L1 */

	/* skip invalid keycodes */
	while (!(key_name = xkb_keymap_key_get_name(xkb_keymap, raw_keymap_pos))) {
		raw_keymap_next();
	}

	sym_name = get_syms_str();
	fprintf(stderr,"Key %u (name: %s) (syms: %s)?\n", raw_keymap_pos, key_name, sym_name);
	free(sym_name);
}

/* Shared memory support code */
static void randname(char *buf)
{
	struct timespec ts;
	long r;
	int i;

	clock_gettime(CLOCK_REALTIME, &ts);
	r = ts.tv_nsec;
	for (i = 0; i < 6; ++i) {
		buf[i] = 'A'+(r&15)+(r&16)*2;
		r >>= 5;
	}
}

static int create_shm(size_t size)
{
	int retries = 100;
	int fd;
	int ret;
	char name[] = "/wl_shm-XXXXXX";
	do {
		randname(name + sizeof(name) - 7);
		--retries;
		if ((fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600)) >= 0) {
			shm_unlink(name);
			break;
		}
	} while (retries > 0 && errno == EEXIST);
	if (fd == -1) {
		return -1;
	}

	do {
		ret = ftruncate(fd, size);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		close(fd);
		return -1;
	}
	return fd;
}

static struct wl_seat *wl_seat;
static struct wl_registry *wl_registry;
static struct wl_display *wl_display;
static struct wl_keyboard *wl_keyboard;
static struct wl_pointer *wl_pointer;
static struct wl_shm *wl_shm;
static struct wl_compositor *wl_compositor;
static struct xdg_wm_base *xdg_wm_base;
static struct wl_surface *wl_surface;
static struct xdg_surface *xdg_surface;
static struct xdg_toplevel *xdg_toplevel;
static struct zwp_keyboard_shortcuts_inhibit_manager_v1 *keyboard_shortcuts_inhibit_manager;
static struct zwp_keyboard_shortcuts_inhibitor_v1 *keyboard_shortcuts_inhibitor;


/*keyboard shortcut inhibitor listener */
static void inhibitor_active(void *data, struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
	fprintf(stderr, "Shortcuts inhibited\n");
}

static void inhibitor_inactive(void *data, struct zwp_keyboard_shortcuts_inhibitor_v1 *inhibitor)
{
	fprintf(stderr, "Shortcuts not inhibited\n");
}

static struct zwp_keyboard_shortcuts_inhibitor_v1_listener keyboard_shortcuts_inhibitor_listener = {
	.active = inhibitor_active,
	.inactive = inhibitor_inactive,
};


/* buffer listener */
static void buffer_release(void *data, struct wl_buffer *buffer)
{
	wl_buffer_destroy(buffer);
}

static struct wl_buffer_listener buffer_listener = {
	.release = buffer_release,
};

/* actually draw */
static struct wl_buffer *draw(void)
{
	int width = 640, height = 480;
	int stride = width * 4;
	int size = stride * height;
	int fd;
	uint32_t *data;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;

	if ((fd = create_shm(size)) == -1 ) {
		return NULL;
	}

	if ((data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == MAP_FAILED) {
		close(fd);
		return NULL;
	}

	pool = wl_shm_create_pool(wl_shm, fd, size);
	buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	close(fd);

	/*just draw white for now */
	memset(data, 0, size);

	munmap(data, size);
	wl_buffer_add_listener(buffer, &buffer_listener, NULL);
	return buffer;
}


/* xdg-surface listener */
static void xdg_surface_configure(void *data, struct xdg_surface *surface, uint32_t serial)
{
	struct wl_buffer *buffer;

	xdg_surface_ack_configure(surface, serial);
	if (!(buffer = draw())) {
		fprintf(stderr, "Could not draw buffer\n");
		exit(1);
	}
	wl_surface_attach(wl_surface, buffer, 0, 0);
	wl_surface_commit(wl_surface);
}

static struct xdg_surface_listener xdg_surface_listener = {
	.configure = xdg_surface_configure,
};

/* xdg_wm_base */

static void wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
	xdg_wm_base_pong(xdg_wm_base, serial);
}

static struct xdg_wm_base_listener wm_base_listener = {
	.ping = wm_base_ping,
};

/* keyboard */
/* for debugging purposes.... */
static void keymap_print(char *buf, size_t len)
{
	/* xcalloc(len + 1, 1) already leaves s[len] as a zero terminator;
	 * copying len + 1 bytes (as this used to) reads one byte past the
	 * len-byte buf region, which is only ever exactly len bytes (it's
	 * an mmap() of exactly len bytes -- see the caller). */
	char *s = xcalloc(len + 1, 1);
	memcpy(s, buf, len);
	fprintf(stderr, "\n***\n%s\n***\n", s);
	free(s);
}
/* Print the name of the keymap's xkb_keycodes section (e.g. "mac",
 * "evdev_aliases(qwerty)") if it can be found. This is genuinely useful
 * to see whenever a keymap-changed restart happens below, not just a
 * debugging aid: it's usually the quickest way to recognize *why* one
 * happened -- most commonly because keypresses started arriving from a
 * different keyboard device than before (e.g. local hardware vs. a
 * remote connection's virtual keyboard, each independently keymapped),
 * which this seat-level event doesn't otherwise identify at all. */
/* Returns a newly-allocated copy of buf's xkb_keycodes section name (e.g.
 * "mac", "evdev_aliases(qwerty)"), or NULL if none could be found/parsed
 * -- caller frees. buf is assumed null-terminated XKB_KEYMAP_FORMAT_TEXT_V1
 * text (see the xkb_keymap_new_from_buffer() calls below, which pass
 * size - 1 on that same assumption), so plain string functions are
 * enough here -- no need for buf's actual length or manual pointer-bound
 * arithmetic to stay in-bounds. */
static char *get_keycodes_section_name(const char *buf)
{
	const char *needle = "xkb_keycodes \"";
	const char *p, *end;
	char *ret = NULL;

	if ((p = strstr(buf, needle)) && (end = strchr(p += strlen(needle), '"'))) {
		xasprintf(&ret, "%.*s", (int)(end - p), p);
	}
	return ret;
}

/* Compile buf (size bytes, in the given wl_keyboard::keymap format) into
 * a new xkb_keymap, or print the raw content and exit if it doesn't
 * parse -- shared by both the first-keymap and keymap-changed paths in
 * keyboard_keymap() below. */
static struct xkb_keymap *compile_keymap_or_die(char *buf, uint32_t size, uint32_t format)
{
	struct xkb_keymap *km;

	if (!(km = xkb_keymap_new_from_buffer(xkb_ctx, buf, size - 1, format, XKB_KEYMAP_COMPILE_NO_FLAGS))) {
		fprintf(stderr, "could not compile keymap:\n");
		keymap_print(buf, size);
		exit(1);
	}
	return km;
}

static void keyboard_keymap(void *data, struct wl_keyboard *wl_kb, uint32_t format, int32_t fd, uint32_t size)
{
	char *buf;

	/* format is one of only two protocol-defined values:
	 * WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1 (a real, null-terminated XKB
	 * keymap string -- the only thing this tool can do anything with)
	 * or WL_KEYBOARD_KEYMAP_FORMAT_NO_KEYMAP ("client must understand
	 * how to interpret the raw keycode", i.e. there is no keymap
	 * behind fd/size at all). Attempting to mmap() and compile that as
	 * if it were real keymap text would reintroduce exactly the crash
	 * this whole restart-on-change design exists to avoid, just via a
	 * different trigger -- the seat having one keymapped device and
	 * one that isn't is a real possibility this tool should tolerate,
	 * given it's specifically meant to handle multiple keyboard
	 * devices sharing a seat in the first place. */
	if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
		close(fd);
		if (xkb_keymap) {
			fprintf(stderr, "keymap event with format=%u (not a real keymap) -- ignoring, keeping the current one\n", format);
			return;
		}
		fprintf(stderr, "keymap event with format=%u (not a real keymap), and no usable keymap has been received yet -- nothing to capture against\n", format);
		exit(1);
	}

	if ((buf = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED) {
		fprintf(stderr, "could not map keyboard map fd\n");
		exit(1);
	}

	/* Some compositors re-send wl_keyboard::keymap on events that
	 * aren't actually a keymap change (e.g. surface refocus) -- when
	 * the content is byte-for-byte identical to what's already
	 * compiled, there's nothing to restart against, so don't. */
	if (xkb_keymap_buf && size == xkb_keymap_buf_size && !memcmp(buf, xkb_keymap_buf, size)) {
		fprintf(stderr, "keymap event received again with identical content -- ignoring, no restart needed\n");
		munmap(buf, size);
		close(fd);
		return;
	}

	/* A second, actually-different keymap event isn't a problem to
	 * treat as fatal either. It happens whenever the compositor
	 * considers a different keyboard device "active" for the seat than
	 * before -- and if you're mapping keys arriving over a remote
	 * connection (the reason this tool exists in the first place),
	 * that's expected to happen often: the remote connection's own
	 * virtual keyboard and your local hardware each get keymapped
	 * independently, and any incidental local input while capturing
	 * switches which one the compositor treats as current. These are
	 * two genuinely different, both individually valid keymaps, not a
	 * duplicate notification. Whatever's been captured so far was
	 * recorded against the old keymap's key numbering, which a new
	 * keymap has no reason to preserve (its <NAME>=N assignments, and
	 * even its overall key count, can differ completely), so there's
	 * no way to carry it forward correctly -- restart the capture
	 * against the new keymap instead of silently mixing numbering
	 * schemes into the output. */
	if (xkb_keymap) {
		struct xkb_keymap *new_keymap = compile_keymap_or_die(buf, size, format);
		char *old_name = get_keycodes_section_name(xkb_keymap_buf);
		char *new_name = get_keycodes_section_name(buf);
		uint32_t old_keys = xkb_keymap_max_keycode(xkb_keymap) - xkb_keymap_min_keycode(xkb_keymap) + 1;
		uint32_t new_keys = xkb_keymap_max_keycode(new_keymap) - xkb_keymap_min_keycode(new_keymap) + 1;

		fprintf(stderr,
			"keyboard change detected. please use only the remote keyboard throughout the process. restarting now.\n"
			"  (received new keymap message -- old device type \"%s\" with %u keys, new device type \"%s\" with %u keys)\n",
			old_name ? old_name : "(unknown)", old_keys,
			new_name ? new_name : "(unknown)", new_keys);

		free(old_name);
		free(new_name);
		xkb_keymap_unref(xkb_keymap);
		xkb_keymap = new_keymap;
	} else {
		xkb_keymap = compile_keymap_or_die(buf, size, format);
	}

	free(xkb_keymap_buf);
	xkb_keymap_buf = xcalloc(size, 1);
	memcpy(xkb_keymap_buf, buf, size);
	xkb_keymap_buf_size = size;

	munmap(buf, size);
	close(fd);

	/* set up the raw keymap for our consumption */
	raw_keymap_min = xkb_keymap_min_keycode(xkb_keymap);
	raw_keymap_max = xkb_keymap_max_keycode(xkb_keymap);
	if (raw_keymap_max_limit < raw_keymap_max) {
		raw_keymap_max = raw_keymap_max_limit;
		fprintf(stderr, "limiting maximum local keycode to %u\n", raw_keymap_max);
	}
	raw_keymap_init();
	if (started) {
		/* Only the very first keymap prints its own initial prompt,
		 * from pointer_button() on the click that sets `started`. A
		 * restart happening after that point needs to prompt again
		 * itself, or the user has no indication their progress just
		 * got reset. */
		raw_keymap_prompt();
	}
}

static void keyboard_enter(void *data, struct wl_keyboard *wl_kb, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
}
static void keyboard_leave(void *data, struct wl_keyboard *wl_kb, uint32_t serial, struct wl_surface *surface)
{
}
static void keyboard_key(void *data, struct wl_keyboard *wl_kb, uint32_t serial, uint32_t time, uint32_t
key, uint32_t state)
{
	key += 8;
	if (started && !state) {
		raw_keymap_store(key);
		raw_keymap_prompt();
	}
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

/* pointer */
static void pointer_enter(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t x, wl_fixed_t y)
{
}
static void pointer_leave(void *data, struct wl_pointer *pointer, uint32_t serial, struct wl_surface *surface)
{
}
static void pointer_motion(void *data, struct wl_pointer *pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y)
{
}
static void pointer_button(void *data, struct wl_pointer *pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
	if (state) {
		return;
	}

	if (!started) {
		started = 1;
		fprintf(stderr, "Starting keymapping\n");
		fprintf(stderr, "To skip, left click\n");
		fprintf(stderr, "To exit and print, right click\n");
		fprintf(stderr, "To undo, middle click\n");
	} else {
		switch (button) {
			case BTN_LEFT:
				raw_keymap_next();
				break;
			case BTN_MIDDLE:
				raw_keymap_prev();
				break;
			case BTN_RIGHT:
				print_output();
				break;
		}
	}
	raw_keymap_prompt();
}
static void pointer_axis(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t val)
{
}
static void pointer_frame(void *data, struct wl_pointer *pointer)
{
}
static void pointer_axis_source(void *data, struct wl_pointer *pointer, uint32_t axis_source)
{
}
static void pointer_axis_stop(void *data, struct wl_pointer *pointer, uint32_t time, uint32_t axis)
{
}
static void pointer_axis_discrete(void *data, struct wl_pointer *pointer, uint32_t axis, int32_t discrete)
{
}

static struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.button = pointer_button,
	.axis = pointer_axis,
	.frame = pointer_frame,
	.axis_source = pointer_axis_source,
	.axis_stop = pointer_axis_stop,
	.axis_discrete = pointer_axis_discrete,
};
/* seat */
static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
	if (caps & (WL_SEAT_CAPABILITY_KEYBOARD | WL_SEAT_CAPABILITY_POINTER)) {
		wl_keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(wl_keyboard, &keyboard_listener, NULL);
		wl_pointer = wl_seat_get_pointer(wl_seat);
		wl_pointer_add_listener(wl_pointer, &pointer_listener, NULL);
		fprintf(stderr, "To start, click input window\n");
	} else {
		fprintf(stderr, "seat does not have input\n");
		exit(1);
	}
}

static void seat_name(void *data, struct wl_seat *seat, const char *name)
{
}

static struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};

/* registry */
static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, wl_seat_interface.name) == 0) {
		wl_seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
		wl_seat_add_listener(wl_seat, &seat_listener, NULL);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		wl_shm = wl_registry_bind(registry, name, &wl_shm_interface, version);
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		wl_compositor = wl_registry_bind(registry, name, &wl_compositor_interface, version);
	} else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
		xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, version);
		xdg_wm_base_add_listener(xdg_wm_base, &wm_base_listener, NULL);
	} else if (strcmp(interface, zwp_keyboard_shortcuts_inhibit_manager_v1_interface.name) == 0) {
		keyboard_shortcuts_inhibit_manager = wl_registry_bind(registry, name, &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, version);
	}
}
static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}
static struct wl_registry_listener registry_listener = {
	.global = registry_global,
	.global_remove = registry_global_remove,
};


int main(int argc, char **argv)
{

	union sopt_arg soptarg = {0};
	int opt;
	char *out_file_path = NULL;

	sopt_usage_set(optspec, argv[0], "Interactively create keymaps/keycode sections for waynergy configuration");

	while ((opt = sopt_getopt_s(argc, argv, optspec, NULL, NULL, &soptarg)) != -1) {
		switch (opt) {
			case 'h':
				sopt_usage_s();
				return 0;
			case 's':
				xkb_skipped = true;
				break;
			case 'o':
				out_file_path = xstrdup(soptarg.str);
				break;
			case 'r':
				xkb_out_name = NULL;
				break;
			case 'x':
				xkb_out_name = xstrdup(soptarg.str);
				break;
			case 'l':
				raw_keymap_max_limit = soptarg.l;
				break;
			default:
				sopt_usage_s();
				return 1;
		}
	}

	if (out_file_path) {
		if (!(out_file = fopen(out_file_path, "w+"))) {
			perror("could not open output file");
			return 1;
		}
		fprintf(stderr, "Printing output to %s\n", out_file_path);
	} else {
		fprintf(stderr, "Printing output to stdout\n");
		out_file = stdout;
	}

	if (!(xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS))) {
		fprintf(stderr, "could not create xkb context\n");
		exit(1);
	}


	if (!(wl_display = wl_display_connect(NULL))) {
		fprintf(stderr, "could not connect to wayland socket\n");
		return 1;
	}
	wl_registry = wl_display_get_registry(wl_display);
	wl_registry_add_listener(wl_registry, &registry_listener, NULL);
	wl_display_dispatch(wl_display);
	wl_display_roundtrip(wl_display);

	wl_surface = wl_compositor_create_surface(wl_compositor);
	xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, wl_surface);
	xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);
	xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
	xdg_toplevel_set_title(xdg_toplevel, argv[0]);
	wl_surface_commit(wl_surface);

	if (keyboard_shortcuts_inhibit_manager) {
		keyboard_shortcuts_inhibitor = zwp_keyboard_shortcuts_inhibit_manager_v1_inhibit_shortcuts(keyboard_shortcuts_inhibit_manager, wl_surface, wl_seat);
		zwp_keyboard_shortcuts_inhibitor_v1_add_listener(keyboard_shortcuts_inhibitor, &keyboard_shortcuts_inhibitor_listener, NULL);
	} else {
		fprintf(stderr, "WARNING: could not inhibit compositor keyboard shortcuts, keyboard input might trigger unexpected behavior\n");
	}

	while (wl_display_dispatch(wl_display) != -1);

	return 0;
}

