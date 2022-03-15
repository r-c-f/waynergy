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
#include "ver.h"

static struct sopt optspec[] = {
	SOPT_INITL('h', "help", "Help text"),
	SOPT_INITL('v', "version", "Version information"),
	SOPT_INIT_ARGL('b', "backend", SOPT_ARGTYPE_STR, "backend", "Input backend -- one of wlr, kde, uinput"),
	SOPT_INIT_ARGL('C', "config", SOPT_ARGTYPE_STR, "path", "Configuration directory"),
	SOPT_INIT_ARGL('c', "host", SOPT_ARGTYPE_STR, "host", "Server to connect to"),
	SOPT_INIT_ARGL('p', "port", SOPT_ARGTYPE_STR, "port", "Port"),
	SOPT_INIT_ARGL('W', "width", SOPT_ARGTYPE_SHORT, "width", "Width of screen in pixels (manual override, must be given with height)"),
	SOPT_INIT_ARGL('H', "height", SOPT_ARGTYPE_SHORT, "height", "Height of screen in pixels (manual override, must be given with width)"),
	SOPT_INIT_ARGL('N', "name", SOPT_ARGTYPE_STR, "name", "Name of client screen"),
	SOPT_INIT_ARGL('l', "logfile", SOPT_ARGTYPE_STR, "file", "Name of logfile to use"),
	SOPT_INIT_ARGL('L', "loglevel", SOPT_ARGTYPE_STR, "level", "Log level -- number, increasing from 0 for more verbosity up to 5, or one of 'none', 'error', 'warn', 'info', 'debug'"),
	SOPT_INITL('n', "no-clip", "Don't synchronize the clipboard"),
	SOPT_INITL('e', "enable-crypto", "Enable TLS encryption"),
	SOPT_INITL('E', "disable-crypto", "Force disable TLS encryption"),
	SOPT_INITL('t', "enable-tofu", "Enable trust-on-first-use for TLS certificate"),
	SOPT_INITL(CHAR_MAX + USYNERGY_ERROR_NONE, "fatal-none", "Consider *normal* disconnect (i.e. CBYE) to be fatal"),
	SOPT_INITL(CHAR_MAX + USYNERGY_ERROR_EBAD, "fatal-ebad", "Protocol errors are fatal"),
	SOPT_INITL(CHAR_MAX + USYNERGY_ERROR_EBSY, "fatal-ebsy", "EBSY (client already exists with our name) errors are fatal"),
	SOPT_INITL(CHAR_MAX + USYNERGY_ERROR_TIMEOUT, "fatal-timeout", "timeouts are fatal"),
	SOPT_INIT_END
};


uSynergyContext synContext;
struct wlContext wlContext = {0};
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

static void uinput_fd_open(int res[static 2])
{
	if ((res[0] = open("/dev/uinput", O_WRONLY | O_CLOEXEC)) == -1) {
		/* can't use normal logs yet, still privileged */
		perror("uinput fd open failed (this is normal if not using uinput backend)");
		return;
	}
	if ((res[1] = open("/dev/uinput", O_WRONLY | O_CLOEXEC)) == -1) {
		perror("uinput fd open failed (this is normal if not using uinput backend)");
		close(res[0]);
		res[0] = -1;
	}
}

int main(int argc, char **argv)
{
	int ret = EXIT_SUCCESS;
	int opt;
	union sopt_arg soptarg;
	char *port = NULL;
	char *name = NULL;
	char *host = NULL;
	char *backend = NULL;
	char hostname[_POSIX_HOST_NAME_MAX] = {0};
	char *log_path = NULL;
	enum logLevel log_level;
	bool man_geom = false;
	bool use_clipboard = true;
	bool enable_crypto = false;
	bool enable_tofu = false;

	/* deal with privileged operations first */
	uinput_fd_open(wlContext.uinput_fd);
	/* and drop said privileges */
	osDropPriv();

	/* we default to name being hostname, so get it*/
	if (gethostname(hostname, _POSIX_HOST_NAME_MAX - 1) == -1) {
		perror("gethostname");
		goto error;
	}

	uSynergyInit(&synContext);
	/*Intialize INI configuration*/
	configInitINI();
	/* Load defaults for everything */
	port = configTryString("port", "24800");
	host = configTryString("host", "localhost");
	name = configTryString("name", hostname);
	backend = configTryString("backend", NULL);
	enable_crypto = configTryBool("tls/enable", false);
	enable_tofu = configTryBool("tls/tofu", false);
	synContext.m_clientWidth = configTryLong("width", 0);
	synContext.m_clientHeight = configTryLong("height", 0);
	log_path = configTryString("log/path", NULL);
	log_level = configTryLong("log/level", LOG_WARN);
	sopt_usage_set(optspec, argv[0], "Synergy client for wayland compositors");
	while ((opt = sopt_getopt_s(argc, argv, optspec, NULL, NULL, &soptarg)) != -1) {
		switch (opt) {
			case 'h':
				sopt_usage_s();
				goto done;
			case 'v':
				fprintf(stderr, "%s version %s\n", argv[0], WAYNERGY_VERSION_STR);
				goto done;
			case 'b':
				backend = xstrdup(soptarg.str);
				break;
			case 'C':
				osConfigPathOverride = xstrdup(soptarg.str);
				break;
			case 'c':
				free(host);
				host = xstrdup(soptarg.str);
				break;
			case 'p':
				free(port);
				port = xstrdup(soptarg.str);
				break;
			case 'W':
				synContext.m_clientWidth = soptarg.s;
				break;
			case 'H':
				synContext.m_clientHeight = soptarg.s;
				break;
			case 'N':
				free(name);
				name = xstrdup(soptarg.str);
				break;
			case 'L':
				if ((log_level = logLevelFromString(soptarg.str)) == LOG__INVALID) {
					goto opterror;
				}
				break;
			case 'l':
				log_path = xstrdup(soptarg.str);
				break;
			case 'n':
				use_clipboard = false;
				break;
			case 'e':
				enable_crypto = true;
				break;
			case 'E':
				enable_crypto = false;
				break;
			case 't':
				enable_tofu = true;
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
				goto error;
		}
	}
	/* set up logging */
	logInit(log_level, log_path);
	logInfo("%s version %s", argv[0], WAYNERGY_VERSION_STR);
	/* now we decide if we should use manual geom */
	if (synContext.m_clientWidth || synContext.m_clientHeight) {
		if (!(synContext.m_clientWidth && synContext.m_clientHeight)) {
			logErr("Must specify both manual dimensions");
			sopt_usage_s();
			goto error;
		}
		logInfo("Using manaul dimensions: %dx%d", synContext.m_clientWidth, synContext.m_clientHeight);
		man_geom = true;
	}
	/* set up signal handler */
	sigHandleInit(argv);
	/* we can't override const, so set hostname here*/
	synContext.m_clientName = name;
	if (!synNetInit(&synNetContext, &synContext, host, port, enable_crypto, enable_tofu)) {
		logErr("Could not initialize network code");
		goto error;
	}
	/* key code type */
	synContext.m_useRawKeyCodes = configTryBool("syn_raw_key_codes", true);
	/* populate events */
	synContext.m_mouseMoveCallback = syn_mouse_move_cb;
	synContext.m_mouseButtonDownCallback = syn_mouse_button_down_cb;
	synContext.m_mouseButtonUpCallback = syn_mouse_button_up_cb;
	synContext.m_mouseWheelCallback = syn_mouse_wheel_cb;
	synContext.m_keyboardCallback = syn_key_cb;
	synContext.m_screensaverCallback = syn_screensaver_cb;
	synContext.m_screenActiveCallback = syn_active_cb;
	/* wayland context events */
	wlContext.on_output_update = man_geom ? NULL : wl_output_update_cb;
	/* set up clipboard */
	if (clipHaveWlClipboard() && use_clipboard) {
		synContext.m_clipboardCallback = syn_clip_cb;
		if (!clipSetupSockets())
			goto error;
		if(!clipSpawnMonitors())
			goto error;
	} else if (!use_clipboard) {
		logInfo("Clipboard sync disabled by command line");
	} else {
		logWarn("wl-clipboard not found, no clipboard synchronization support");
	}
	/* setup wayland */
	if (!wlSetup(&wlContext, synContext.m_clientWidth, synContext.m_clientHeight, backend))
		goto error;
	wlIdleInhibit(&wlContext, true);
	/* initialize main loop */
	netPollInit();
	/* and actual main loop */
	while(1) {
		/* no matter what handling signals is a good idea */
	       	sigHandleRun();
		if (!synContext.m_connected) {
			/* always try updating first so we initially connect */
			uSynergyUpdate(&synContext);
		} else {
			netPoll(&synNetContext, &wlContext);
		}
	}
error:
	ret = EXIT_FAILURE;
done:
	free(log_path);
	free(host);
	free(name);
	free(port);
	free(backend);
	return ret;
}




