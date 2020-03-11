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
	SOPT_INIT_ARGL('W', "width", "width", "Width of screen in pixels (manual override, must be given with height)"),
	SOPT_INIT_ARGL('H', "height", "height", "Height of screen in pixels (manual override, must be given with width)"),
	SOPT_INIT_ARGL('N', "name", "name", "Name of client screen"),
	SOPT_INIT_ARGL('l', "logfile", "file", "Name of logfile to use"),
	SOPT_INIT_ARGL('L', "loglevel", "level", "Log level -- number, increasing from 0 for more verbosity"),
	SOPT_INIT_END
};


uSynergyContext synContext;
struct wlContext wlContext;

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
 		wlKey(&wlContext, key, down, mod);
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

static void cleanup(void)
{
	wlIdleInhibit(&wlContext, false);
	/* stop clipbpoard monitors */
	for (int i = 0; i < 2; ++i) {
		if (clipMonitorPid[i] != -1)
			kill(clipMonitorPid[i], SIGTERM);
	}
	/*close stuff*/
	synNetDisconnect();
	logClose();
	wlClose(&wlContext);
	/*unmask any caught signals*/

}

static char **argv_reexec;
static char *path_reexec;

void sig_handle(int sig)
{
	switch (sig) {
		case SIGTERM:
		case SIGINT:
		case SIGQUIT:
			logInfo("Received signal %d, cleaning up", sig);
			cleanup();
			exit(0);
		case SIGUSR1:
			logInfo("Receieved SIGUSR1, restarting as '%s'", argv_reexec[0]);
			cleanup();
			errno = 0;
			execvp(argv_reexec[0], argv_reexec);
			logErr("Could not rexec: %s", strerror(errno));
			exit(1);
		case SIGCHLD:
			logErr("Somethine we spawned died.");
			//FIXME: should probably use sigaction to get more info here
			break;
		default:
			fprintf(stderr, "UNHANDLED SIGNAL: %d\n", sig);
	}
}
int main(int argc, char **argv)
{
	argv_reexec = argv;
	int opt, optind = 0, optcpos = 0;
	char *optarg, *port, *name, *host, hostname[HOST_NAME_MAX] = {0};
	FILE *logfile = NULL;
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
	path_reexec = configTryString("path_reexec", NULL);
	synContext.m_clientWidth = configTryLong("width", 0);
	synContext.m_clientHeight = configTryLong("height", 0);
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
				errno = 0;
				if ((logfile = fopen(optarg, "w+")))
					break;
				perror("logfile open");
opterror:
			default:
			       	sopt_usage_s();
				return 1;
		}
	}
	/* set up logging */
	logInit(log_level, logfile);
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
	signal(SIGTERM, sig_handle);
	signal(SIGINT, sig_handle);
	signal(SIGQUIT, sig_handle);
	signal(SIGUSR1, sig_handle);
	signal(SIGCHLD, sig_handle);
	sigset_t set;
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
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
	/* wayland context events */
	//first zero everything
	memset(&wlContext, 0, sizeof(wlContext));
	//now callbacks
	wlContext.on_output_update = man_geom ? NULL : wl_output_update_cb;
	/*run*/
	if(!clipSpawnMonitors())
		return 3;
	wlSetup(&wlContext, synContext.m_clientWidth, synContext.m_clientHeight);
	wlIdleInhibit(&wlContext, true);
	while(1) uSynergyUpdate(&synContext);
	return 0;
}




