/* uinput-based input handling as a last resort */


#include "wayland.h"
#include "log.h"

#if !defined(__linux__)
bool wlInputInitUinput(struct wlContext *ctx)
{
	logDbg("uinput only works on Linux systems");
	return false;
}
#else

#include <linux/uinput.h>

struct state_uinput {
	int key_fd;
	int mouse_fd;
};

static int button_map[] = {
	0,
	0x110,
	0x112,
	0x111,
	0x150,
	0x151,
};

static void emit(int fd, int type, int code, int val)
{
	struct input_event ie = {
		.type = type,
		.code = code,
		.value = val,
	};
	write(fd, &ie, sizeof(ie));
}

static void mouse_rel_motion(struct wlInput *input, int dx, int dy)
{
	struct state_uinput *ui = input->state;
	emit(ui->mouse_fd, EV_REL, REL_X, dx);
	emit(ui->mouse_fd, EV_REL, REL_Y, dy);
	emit(ui->mouse_fd, EV_SYN, SYN_REPORT, 0);
}

static void mouse_motion(struct wlInput *input, int x, int y)
{
	struct state_uinput *ui = input->state;

	emit(ui->mouse_fd, EV_ABS, ABS_X, x);
	emit(ui->mouse_fd, EV_ABS, ABS_Y, y);
	emit(ui->mouse_fd, EV_SYN, SYN_REPORT, 0);
}

static void mouse_button(struct wlInput *input, int button, int state)
{
	struct state_uinput *ui = input->state;

	emit(ui->mouse_fd, EV_KEY, button_map[button], state);
	emit(ui->mouse_fd, EV_SYN, SYN_REPORT, 0);
}

static void mouse_wheel(struct wlInput *input, signed short dx, signed short dy)
{
}
static void key(struct wlInput *input, int code, int state)
{
	struct state_uinput *ui = input->state;

	if (code > KEY_MAX) {
		logErr("Keycode %d is unsupported by uinput (max %d), dropping", code, KEY_MAX);
		return;
	}

	emit(ui->key_fd, EV_KEY, code, state);
	emit(ui->key_fd, EV_SYN, SYN_REPORT, 0);
}
static bool key_map(struct wlInput *input, char *map)
{
	return true;
}

#define TRY_IOCTL(fd, req, ...) \
       	do { \
		if (ioctl(fd, req, __VA_ARGS__) == -1) { \
			logPDbg("ioctl " #req " failed"); \
			return false; \
		} \
	} while (0)

#define TRY_IOCTL0(fd, req) \
       	do { \
		if (ioctl(fd, req) == -1) { \
			logPDbg("ioctl " #req " failed"); \
			return false; \
		} \
	} while (0)

static bool init_key(struct state_uinput *ui)
{
	int i;
	struct uinput_setup usetup = {0};
	TRY_IOCTL(ui->key_fd, UI_SET_EVBIT, EV_KEY);
	for (i = 0; i <= KEY_MAX; ++i) {
		TRY_IOCTL(ui->key_fd, UI_SET_KEYBIT, i);
	}
	usetup.id.bustype = BUS_VIRTUAL;
	strcpy(usetup.name, "waynergy keyboard");

	TRY_IOCTL(ui->key_fd, UI_DEV_SETUP, &usetup);
	TRY_IOCTL0(ui->key_fd, UI_DEV_CREATE);
	return true;
}

static bool init_mouse(struct state_uinput *ui)
{
	int i;
	struct uinput_setup usetup = {0};

	TRY_IOCTL(ui->mouse_fd, UI_SET_EVBIT, EV_KEY);
	for (i = 0; i < (sizeof(button_map)/sizeof(*button_map)); ++i) {
		TRY_IOCTL(ui->mouse_fd, UI_SET_KEYBIT, button_map[i]);
	}

	TRY_IOCTL(ui->mouse_fd, UI_SET_EVBIT, EV_REL);
	TRY_IOCTL(ui->mouse_fd, UI_SET_RELBIT, REL_X);
	TRY_IOCTL(ui->mouse_fd, UI_SET_RELBIT, REL_Y);
	TRY_IOCTL(ui->mouse_fd, UI_SET_EVBIT, EV_ABS);
	TRY_IOCTL(ui->mouse_fd, UI_SET_ABSBIT, ABS_X);
	TRY_IOCTL(ui->mouse_fd, UI_SET_ABSBIT, ABS_Y);

	usetup.id.bustype = BUS_VIRTUAL;
	strcpy(usetup.name, "Waynergy mouse");
	TRY_IOCTL(ui->mouse_fd, UI_DEV_SETUP, &usetup);
	TRY_IOCTL0(ui->mouse_fd, UI_DEV_CREATE);

	return true;
}

bool wlInputInitUinput(struct wlContext *ctx)
{
	struct wlInput *input;
	struct state_uinput *ui;

	ui = xmalloc(sizeof(*ui));
	ui->key_fd = -1;
	ui->mouse_fd = -1;
	input = xcalloc(1, sizeof(*input));

	if ((ui->key_fd = open("/dev/uinput", O_WRONLY)) == -1) {
		logPDbg("could not open uinput for keyboard device");
		goto error;
	}
	if ((ui->mouse_fd = open("/dev/uinput", O_WRONLY)) == -1) {
		logPDbg("Could not open uinput for mouse device");
		goto error;
	}
	if (!init_key(ui))
		goto error;
	if (!init_mouse(ui))
		goto error;

	input->state = ui;
	input->wl_ctx = ctx;
	input->mouse_motion = mouse_motion;
	input->mouse_rel_motion = mouse_rel_motion;
	input->mouse_button = mouse_button;
	input->mouse_wheel = mouse_wheel;
	input->key = key;
	input->key_map = key_map;
	ctx->input = input;
	return true;
error:
	if (ui->key_fd != -1)
		close(ui->key_fd);
	if (ui->mouse_fd != -1)
		close(ui->mouse_fd);
	free(ui);
	free(input);
	return false;
}

#endif /* !defined(__linux__) */


