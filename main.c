#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
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


static struct sopt optspec[] = {
	SOPT_INITL('h', "help", "Help text"),
	SOPT_INIT_ARGL('c', "host", "host", "Server to connect to"),
	SOPT_INIT_ARGL('p', "port", "port", "Port"),
	SOPT_INIT_ARGL('W', "width", "width", "Width of screen in pixels"),
	SOPT_INIT_ARGL('H', "height", "height", "Height of screen in pixels"),
	SOPT_INIT_ARGL('N', "name", "name", "Name of client screen"),
	SOPT_INIT_ARGL('l', "logfile", "file", "Name of logfile (up to " LOG_FILE_MAX_COUNT_STR "), - is stderr"),
	SOPT_INIT_ARGL('L', "loglevel", "level", "Log level -- number, or one of " LOG_LEVEL_USAGE_STR),
	SOPT_INIT_END
};


uSynergyContext synContext;

static void syn_mouse_wheel_cb(uSynergyCookie cookie, int16_t x, int16_t y)
{
	wlMouseWheel(x, y);
}

static void syn_mouse_button_down_cb(uSynergyCookie cookie, enum uSynergyMouseButton button)
{
	wlMouseButtonDown(button);
}
static void syn_mouse_button_up_cb(uSynergyCookie cookie, enum uSynergyMouseButton button)
{
	wlMouseButtonUp(button);
}
static void syn_mouse_move_cb(uSynergyCookie cookie, bool rel, int16_t x, int16_t y)
{
	if (rel) {
		wlMouseRelativeMotion(x, y);
	} else {
		wlMouseMotion(x, y);
	}
}
static void syn_key_cb(uSynergyCookie cookie, uint16_t key, uint16_t mod, bool down, bool repeat)
{
	if (!repeat)
 		wlKey(key, down, mod);
}
static void syn_trace(uSynergyCookie cookie, const char *text)
{
	logInfo("%s\n", text);
}
static void syn_clip_cb(uSynergyCookie cookie, enum uSynergyClipboardId id, uint32_t format, const uint8_t *data, uint32_t size)
{
	clipWlCopy(id, data, size);
}
static void syn_screensaver_cb(uSynergyCookie cookie, bool state)
{
	size_t i;
	int ret;
	char **cmd = configReadLines(state ? "screensaver/start" : "screensaver/stop");
	if (!cmd)
		return;
	for (i = 0; cmd[i]; ++i) {
		ret = system(cmd[i]);
		if (ret) {
			fprintf(stderr, "Screensaver callback state %s command #%d (%s) failed with code %d\n", state ? "start" : "stop", i, cmd[i], ret);
		}
	}
	strfreev(cmd);
}
void wl_output_update_cb(struct wlOutput *output)
{
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
	wlResUpdate(width, height);
}


void sig_handle(int sig)
{
	switch (sig) {
		case SIGTERM:
		case SIGINT:
		case SIGQUIT:
			wlIdleInhibit(false);
			/* stop clipbpoard monitors */
			for (int i = 0; i < 2; ++i) {
				if (clipMonitorPid[i] != -1)
					kill(clipMonitorPid[i], SIGTERM);
			}
			exit(0);
		default:
			fprintf(stderr, "UNHANDLED SIGNAL: %d\n", sig);
	}
}
int main(int argc, char **argv)
{
	int opt, optind = 0, optcpos = 0;
	char *optarg, *port, *name, *host, hostname[HOST_NAME_MAX] = {0};
	FILE *logfiles[LOG_FILE_MAX_COUNT] = {0};
	size_t logfiles_pos = 0;
	enum logLevel log_level = LOG_NONE;
	short optshrt;
	long optlong;
	bool optshrt_valid, optlong_valid;
	bool man_geom = false;

	/* If we are run as swaynergy-clip-update, we're just supposed to write
	 * to the FIFO */
	if (strstr(argv[0], "swaynergy-clip-update")) {
		return clipWriteToFifo(argv[1]);
	}
	/*  proceed as the main process */

	/* we default to name being hostname, so get it*/
	gethostname(hostname, HOST_NAME_MAX - 1);

	uSynergyInit(&synContext);
	/* Load defaults for everything */
	port = configTryString("port", "24800");
	host = configTryString("host", "localhost");
	name = configTryString("name", hostname);
	synContext.m_clientWidth = configTryLong("width", -1);
	synContext.m_clientHeight = configTryLong("height", -1);
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
				log_level = logLevelParse(optarg);
				break;
			case 'l':
				if (logfiles_pos == LOG_FILE_MAX_COUNT) {
					fprintf(stderr, "Too many log files given, ignoring %s\n", optarg);
					break;
				}
				if (!(strcmp(optarg, "-"))) {
					logfiles[logfiles_pos++] = stderr;
					break;
				}
				errno = 0;
				if ((logfiles[logfiles_pos] = fopen(optarg, "w+"))) {
					++logfiles_pos;
					break;
				} else {
					perror("logfile open");
					goto opterror;
				}
				break;
opterror:
			default:
			       	sopt_usage_s();
				return 1;
		}
	}
	/* set up logging */
	logInit(log_level, logfiles);
	/* now we decide if we should use manual geom */
	if ((synContext.m_clientWidth == -1) || (synContext.m_clientHeight == -1)) {
		if ((synContext.m_clientWidth * synContext.m_clientHeight) < 0) {
			logErr("Must specify both manual dimensions");
			sopt_usage_s();
			return 1;
		}
		logInfo("Using manaul dimensions: %dx%d", synContext.m_clientWidth, synContext.m_clientHeight);
		man_geom = true;
	}
	/* set up signal handler */
	signal(SIGTERM, sig_handle);
	signal(SIGINT, sig_handle);
	signal(SIGQUIT, sig_handle);
	/* we can't override const, so set hostname here*/
	synContext.m_clientName = name;

	if (!synNetConfig(&synContext, host, port))
		return 2;
	/* populate events */
	synContext.m_mouseMoveCallback = syn_mouse_move_cb;
	synContext.m_mouseButtonDownCallback = syn_mouse_button_down_cb;
	synContext.m_mouseButtonUpCallback = syn_mouse_button_up_cb;
	synContext.m_mouseWheelCallback = syn_mouse_wheel_cb;
	synContext.m_keyboardCallback = syn_key_cb;
	synContext.m_traceFunc = syn_trace;
	synContext.m_clipboardCallback = syn_clip_cb;
	synContext.m_screensaverCallback = syn_screensaver_cb;
	wlOnOutputsUpdated = man_geom ? NULL : wl_output_update_cb;
	/*run*/
	if(!clipSpawnMonitors())
		return 3;
	wlSetup(synContext.m_clientWidth, synContext.m_clientHeight);
	wlIdleInhibit(true);
	while(1) uSynergyUpdate(&synContext);
	return 0;
}




