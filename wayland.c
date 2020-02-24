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

static struct wl_seat *seat = NULL;
static struct zwlr_virtual_pointer_manager_v1 *pointer_manager = NULL;
static struct zwp_virtual_keyboard_manager_v1 *keyboard_manager = NULL;

static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	if (strcmp(interface, wl_seat_interface.name) == 0) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
	} else if (strcmp(interface, zwlr_virtual_pointer_manager_v1_interface.name) == 0) {
		pointer_manager = wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 1);
	} else if (strcmp(interface, zwp_virtual_keyboard_manager_v1_interface.name) == 0) {
		keyboard_manager = wl_registry_bind(registry, name, &zwp_virtual_keyboard_manager_v1_interface, 1);
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


static struct wl_registry *registry = NULL;
static struct wl_display *display = NULL;
static struct zwlr_virtual_pointer_v1 *pointer = NULL;
static struct zwp_virtual_keyboard_v1 *keyboard = NULL;
static struct wl_event_queue *queue = NULL;

static int wlWidth = 0;
static int wlHeight = 0;
static time_t wlEpoch = -1;

static int button_map[] = {
	0,
	0x110,
	0x112,
	0x111,
	0x150,
	0x151,
	-1
};
static inline uint32_t wlTS(void)
{
	struct timespec ts;
	if (wlEpoch == -1) {
		wlEpoch = time(NULL);
	}
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec -= wlEpoch;
	return (ts.tv_sec * 1000) + (ts.tv_nsec / 1000000);
}

void wlClose(void)
{
	return;
}

/* create a layout file descriptor */
static int set_layout(void)
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
	zwp_virtual_keyboard_v1_keymap(keyboard, WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1, fd, keymap_size);
done:
	free(keymap_str);
	return 0;
}

static bool local_mod_init(void);

int wlSetup(int width, int height)
{
	wlWidth = width;
	wlHeight = height;
	display = wl_display_connect(NULL);
	if (!display) {
		printf("Couldn't connect, yo\n");
		return 1;
	}
	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);

	pointer = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(pointer_manager, seat);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);
	keyboard = zwp_virtual_keyboard_manager_v1_create_virtual_keyboard(keyboard_manager, seat);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);
	if(set_layout()) {
		return 1;
	}
	local_mod_init();
	return 0;
}

void wlMouseRelativeMotion(int dx, int dy)
{
	zwlr_virtual_pointer_v1_motion(pointer, wlTS(), wl_fixed_from_int(dx), wl_fixed_from_int(dy));
	zwlr_virtual_pointer_v1_frame(pointer);
	wl_display_flush(display);
}
void wlMouseMotion(int x, int y)
{
	zwlr_virtual_pointer_v1_motion_absolute(pointer, wlTS(), x, y, wlWidth, wlHeight);
	zwlr_virtual_pointer_v1_frame(pointer);
	wl_display_flush(display);
}
void wlMouseButtonDown(int button)
{
	zwlr_virtual_pointer_v1_button(pointer, wlTS(), button_map[button], 1);
	zwlr_virtual_pointer_v1_frame(pointer);
	wl_display_flush(display);
}
void wlMouseButtonUp(int button)
{
	zwlr_virtual_pointer_v1_button(pointer, wlTS(), button_map[button], 0);
	zwlr_virtual_pointer_v1_frame(pointer);
	wl_display_flush(display);
}
void wlMouseWheel(signed short dx, signed short dy)
{
	//we are a wheel, after all
	zwlr_virtual_pointer_v1_axis_source(pointer, 0);
	uint32_t ts = wlTS();
	if (dx < 0) {
		zwlr_virtual_pointer_v1_axis_discrete(pointer, wlTS(), 1, wl_fixed_from_int(15), 1);
	}else if (dx > 0) {
		zwlr_virtual_pointer_v1_axis_discrete(pointer, wlTS(), 1, wl_fixed_from_int(-15), -1);
	}
	if (dy < 0) {
		zwlr_virtual_pointer_v1_axis_discrete(pointer, wlTS(), 0, wl_fixed_from_int(15),1);
	} else {
		zwlr_virtual_pointer_v1_axis_discrete(pointer, wlTS(), 0, wl_fixed_from_int(-15), -1);
	}
	zwlr_virtual_pointer_v1_frame(pointer);
	wl_display_flush(display);
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
	size_t line_count, i, l;
	long key;
	char *conf[] = {
		"shift",
		"control",
		"alt",
		"meta",
		"super",
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

void wlKey(int key, int state, uint32_t mask)
{
	key -= 8;
	int xkb_sym;
	if ((key & 0xE000) == 0xE000) {
		xkb_sym = key + 0x1000;
	} else {
		xkb_sym = key;
	}
	uint32_t xmodmask = wlModConvert(mask) | intrinsic_mask(key + 8);
	zwp_virtual_keyboard_v1_modifiers(keyboard, xmodmask, 0, 0, 0);
	zwp_virtual_keyboard_v1_key(keyboard, wlTS(), xkb_sym, state);
	if (!state) {
		zwp_virtual_keyboard_v1_modifiers(keyboard, 0, 0, 0, 0);
	}
	wl_display_flush(display);
}

int wlPrepareFd(void)
{
	int fd;

	fd = wl_display_get_fd(display);
//	while (wl_display_prepare_read(display) != 0) {
//		wl_display_dispatch(display);
//	}
//	wl_display_flush(display);
	return fd;
}

void wlPollProc(short revents)
{
	if (revents & POLLIN) {
//		wl_display_cancel_read(display);
		wl_display_dispatch(display);
	}
}


/* FIXME XXX: hacky as fuck way to inhibit idle -- we just execute some commands */
void wlIdleInhibit(bool on)
{
	char *cmd = configTryString(on ? "idle-inhibit/cmd-on" : "idle-inhibit/cmd-off", NULL);
	if (cmd)
		system(cmd);
	free(cmd);
}
