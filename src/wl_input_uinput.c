/* uinput-based input handling as a last resort */


#include "wayland.h"

#ifdef __linux__

#include <linux/uinput.h>

struct state_uinput {
	int key_fd;
	int mouse_fd;
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
	struct state_uinput *state = input->state;
	emit(state->mouse_fd, EV_REL, REL_X, dx);
	emit(state->mouse_fd, EV_REL, REL_Y, dy);
	emit(state->mouse_fd, EV_SYN, SYN_REPORT, 0);
}

static void mouse_motion(struct wlInput *input, int x, int y)
{
	struct state_uinput *state = input->state;

	emit(state->mouse_fd, EV_ABS, ABS_X, x);
	emit(state->mouse_fd, EV_ABS, ABS_Y, y);
	emit(state->mouse_fd, EV_SYN, SYN_REPORT, 0);
}

static void mouse_button(struct wlInput *input, int button, int state)
{
	struct state_uinput *state = input->state;

	emit(state->mouse_fd, EV_KEY, input->button_map[button], state);
	emit(state->mouse_fd, EV_SYN, SYN_REPORT, 0);
}


