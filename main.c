#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <poll.h>
#include "xmem.h"
#include "fdio_full.h"
#include "sopt.h"
#include "uSynergy.h"
#include "net.h"
#include "wayland.h"
#include "config.h"
#include "clip.h"
#include "log.h"
#include "sig.h"


static struct sopt optspec[] = {
	SOPT_INITL('h', "help", "Help text"),
	SOPT_INIT_ARGL('c', "host", "host", "Server to connect to"),
	SOPT_INIT_ARGL('p', "port", "port", "Port"),
	SOPT_INIT_ARGL('W', "width", "width", "Width of screen in pixels (manual override, must be given with height)"),
	SOPT_INIT_ARGL('H', "height", "height", "Height of screen in pixels (manual override, must be given with width)"),
	SOPT_INIT_ARGL('N', "name", "name", "Name of client screen"),
	SOPT_INIT_ARGL('l', "logfile", "file", "Name of logfile to use"),
	SOPT_INIT_ARGL('L', "loglevel", "level", "Log level -- number, increasing from 0 for more verbosity"),
	SOPT_INITL('n', "no-clip", "Don't synchronize the clipboard"),
	SOPT_INITL(CHAR_MAX + USYNERGY_ERROR_NONE, "fatal-none", "Consider *normal* disconnect (i.e. CBYE) to be fatal"),
	SOPT_INITL(CHAR_MAX + USYNERGY_ERROR_EBAD, "fatal-ebad", "Protocol errors are fatal"),
	SOPT_INITL(CHAR_MAX + USYNERGY_ERROR_EBSY, "fatal-ebsy", "EBSY (client already exists with our name) errors are fatal"),
	SOPT_INITL(CHAR_MAX + USYNERGY_ERROR_TIMEOUT, "fatal-timeout", "timeouts are fatal"),
	SOPT_INIT_END
};


uSynergyContext synContext;
struct wlContext wlContext;
struct synNetContext synNetContext;

static void syn_mouse_wheel_cb(uSynergyCookie cookie, int16_t x, int16_t y)
{
	wlMouseWheel(&wlContext, x, y);
}

static void syn_mouse_button_down_cb(uSynergyCookie cookie, enum uSynergyMouseButton button)
{
	wlMouseButtonDown(&wlContext, button);
}
static void syn_mouse_button_up_cb(uSynergyCookie cookie, enum uSynergyMouseButton button)
{
	wlMouseButtonUp(&wlContext, button);
}
static void syn_mouse_move_cb(uSynergyCookie cookie, bool rel, int16_t x, int16_t y)
{
	if (rel) {
		wlMouseRelativeMotion(&wlContext, x, y);
	} else {
		wlMouseMotion(&wlContext, x, y);
	}
}
static void syn_key_cb(uSynergyCookie cookie, uint16_t key, uint16_t mod, bool down, bool repeat)
{
	if (!repeat)
 		wlKey(&wlContext, key, down);
}
static void syn_clip_cb(uSynergyCookie cookie, enum uSynergyClipboardId id, uint32_t format, const uint8_t *data, uint32_t size)
{
	clipWlCopy(id, data, size);
}
static void syn_screensaver_cb(uSynergyCookie cookie, bool state)
{
	size_t i;
	int ret;
	wlIdleInhibit(&wlContext, !state);
	char **cmd = configReadLines(state ? "screensaver/start" : "screensaver/stop");
	if (!cmd)
		return;
	for (i = 0; cmd[i]; ++i) {
		ret = system(cmd[i]);
		if (ret) {
			fprintf(stderr, "Screensaver callback state %s command #%zd (%s) failed with code %d\n", state ? "start" : "stop", i, cmd[i], ret);
		}
	}
	strfreev(cmd);
}
void wl_output_update_cb(struct wlContext *context)
{
	struct wlOutput *output = context->outputs;
	int b, l, t, r;
	b = 0;
	l = 0;
	t = 0;
	r = 0;
	int width = 0;
	int height = 0;
	for(; output; output = output->next) {
		if (b > output->y)
			b = output->y;
		if (l > output->x)
			l = output->x;
		if (t < (output->y + output->height))
			t = (output->y + output->height);
		if (r < (output->x + output->width))
			r = (output->x + output->width);
	}
	width = r - l;
	height = t - b;
	logInfo("Geometry updated: %dx%d", width, height);
	uSynergyUpdateRes(&synContext, width, height);
	wlResUpdate(&wlContext, width, height);
}
static void syn_active_cb(uSynergyCookie cookie, bool active)
{
	if (!active) {
		wlKeyReleaseAll(&wlContext);
	}
}

int main(int argc, char **argv)
{
	int opt, optind = 0, optcpos = 0;
	char *optarg, *port, *name, *host, hostname[_POSIX_HOST_NAME_MAX] = {0};
	char *log_path = NULL;
	enum logLevel log_level;
	short optshrt;
	long optlong;
	bool optshrt_valid, optlong_valid;
	bool man_geom = false;
	bool use_clipboard = true;

	/* we default to name being hostname, so get it*/
	gethostname(hostname, _POSIX_HOST_NAME_MAX - 1);

	uSynergyInit(&synContext);
	/* Load defaults for everything */
	port = configTryString("port", "24800");
	host = configTryString("host", "localhost");
	name = configTryString("name", hostname);
	synContext.m_clientWidth = configTryLong("width", 0);
	synContext.m_clientHeight = configTryLong("height", 0);
	log_path = configTryString("log/path", NULL);
	log_level = configTryLong("log/level", LOG_WARN);
	sopt_usage_set(optspec, argv[0], "Synergy client for wlroots compositors");
	while ((opt = sopt_getopt(argc, argv, optspec, &optcpos, &optind, &optarg)) != -1) {
		if (optarg) {
			errno = 0;
			optlong = strtol(optarg, NULL, 0);
			optlong_valid = !errno;
			if (optlong_valid) {
				optshrt = optlong & 0xFFFF;
				optshrt_valid = ((long)optshrt == optlong);
			} else {
				optshrt_valid = false;
			}
		}
		switch (opt) {
			case 'h':
				sopt_usage_s();
				return 0;
			case 'c':
				free(host);
				host = xstrdup(optarg);
				break;
			case 'p':
				free(port);
				port = xstrdup(optarg);
				break;
			case 'W':
				if (!optshrt_valid)
					goto opterror;
				synContext.m_clientWidth = optshrt;
				break;
			case 'H':
				if (!optshrt_valid)
				       	goto opterror;
				synContext.m_clientHeight = optshrt;
				break;
			case 'N':
				free(name);
				name = xstrdup(optarg);
				break;
			case 'L':
				log_level = optshrt;
				break;
			case 'l':
				log_path = xstrdup(optarg);
				break;
			case 'n':
				use_clipboard = false;
				break;
			case CHAR_MAX + USYNERGY_ERROR_NONE:
			case CHAR_MAX + USYNERGY_ERROR_EBAD:
			case CHAR_MAX + USYNERGY_ERROR_EBSY:
			case CHAR_MAX + USYNERGY_ERROR_TIMEOUT:
				synContext.m_errorIsFatal[opt - CHAR_MAX] = true;
				break;
opterror:
			default:
			       	sopt_usage_s();
				return 1;
		}
	}
	/* set up logging */
	logInit(log_level, log_path);
	/* now we decide if we should use manual geom */
	if (synContext.m_clientWidth || synContext.m_clientHeight) {
		if (!(synContext.m_clientWidth && synContext.m_clientHeight)) {
			logErr("Must specify both manual dimensions");
			sopt_usage_s();
			return 1;
		}
		logInfo("Using manaul dimensions: %dx%d", synContext.m_clientWidth, synContext.m_clientHeight);
		man_geom = true;
	}
	/* set up signal handler */
	sigHandleInit(argv);
	/* we can't override const, so set hostname here*/
	synContext.m_clientName = name;

	if (!synNetInit(&synNetContext, &synContext, host, port))
		return 2;
	/* populate events */
	synContext.m_mouseMoveCallback = syn_mouse_move_cb;
	synContext.m_mouseButtonDownCallback = syn_mouse_button_down_cb;
	synContext.m_mouseButtonUpCallback = syn_mouse_button_up_cb;
	synContext.m_mouseWheelCallback = syn_mouse_wheel_cb;
	synContext.m_keyboardCallback = syn_key_cb;
	synContext.m_screensaverCallback = syn_screensaver_cb;
	synContext.m_screenActiveCallback = syn_active_cb;
	/* wayland context events */
	//first zero everything
	memset(&wlContext, 0, sizeof(wlContext));
	//now callbacks
	wlContext.on_output_update = man_geom ? NULL : wl_output_update_cb;
	/*run*/
	if (clipHaveWlClipboard() && use_clipboard) {
		synContext.m_clipboardCallback = syn_clip_cb;
		if (!clipSetupSockets())
			return 4;
		if(!clipSpawnMonitors())
			return 3;
	} else if (!use_clipboard) {
		logInfo("Clipboard sync disabled by command line");
	} else {
		logWarn("wl-clipboard not found, no clipboard synchronization support");
	}
	wlSetup(&wlContext, synContext.m_clientWidth, synContext.m_clientHeight);
	wlIdleInhibit(&wlContext, true);
	netPollInit();
	while(1) {
		if (!synContext.m_connected) {
			/* always try updating first so we initially connect */
			uSynergyUpdate(&synContext);
		} else {
			netPoll(&synNetContext, &wlContext);
		}
	}
	return 0;
}




