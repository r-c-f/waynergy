#include "log.h"
#include "config.h"
#include "xmem.h"
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>
#include "fdio_full.h"
static enum logLevel log_level = LOG_NONE;

static FILE *log_file;
static int log_file_fd;

static void log_print_ts(FILE *out)
{
	static struct timespec start = {0};
	if (!(start.tv_sec || start.tv_nsec)) {
		clock_gettime(CLOCK_MONOTONIC, &start);
	}
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec -= start.tv_sec;
	ts.tv_nsec -= start.tv_nsec;
	if (ts.tv_nsec < 0) {
		--ts.tv_sec;
		ts.tv_nsec += 1000000000;
	}
	fprintf(out, "%" PRIdMAX ".%09ld",
			(intmax_t)ts.tv_sec,
			ts.tv_nsec);
}
static char *log_level_str[] = {
	"NONE",
	"ERROR",
	"WARN",
	"INFO",
	"DEBUG",
	NULL,
};
enum logLevel logLevelFromString(const char *s)
{
	enum logLevel ll;
	char *endptr;

	for (ll = LOG_NONE; ll < LOG__INVALID; ++ll) {
		if (!strcasecmp(log_level_str[ll], s)) {
			return ll;
		}
	}
	/* try numeric value */
	errno = 0;
	ll = strtol(s, &endptr, 0);
	if (endptr == s) {
		fprintf(stderr, "loglevel: invalid string, and no digits\n");
		ll = LOG__INVALID;
	} else if (errno) {
		perror("loglevel: strtol error");
		ll = LOG__INVALID;
	} else if (ll >= LOG__INVALID) {
		fprintf(stderr, "loglevel: invalid numerical value\n");
		ll = LOG__INVALID;
	}
	return ll;
}

static char *log_level_get_str(enum logLevel level)
{
	assert(level < (sizeof(log_level_str)/sizeof(*log_level_str)));
	return log_level_str[level];
}
static void log_out_v_(FILE *f, enum logLevel level, const char *fmt, va_list ap)
{
	va_list aq;
	if (level > log_level)
		return;
	log_print_ts(f);
	fprintf(f, ": [%s] ", log_level_get_str(level));
	va_copy(aq, ap);
	vfprintf(f, fmt, aq);
	putc('\n', f);
	fflush(f);
}
static void log_out_v(enum logLevel level, const char *fmt, va_list ap)
{
	log_out_v_(stderr, level, fmt, ap);
	if (log_file)
		log_out_v_(log_file, level, fmt, ap);
}
static void log_out_ss(enum logLevel level, const char *msg)
{
	char lf = '\n';
	if (level > log_level)
		return;
	write_full(STDERR_FILENO, msg, strlen(msg), 0);
	write_full(STDERR_FILENO, &lf, 1, 0);
	if (log_file) {
		write_full(log_file_fd, msg, strlen(msg), 0);
		write_full(log_file_fd, &lf, 1, 0);
	}
}

void logOut(enum logLevel level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_out_v(level, fmt, ap);
	va_end(ap);
}
void logOutSig(enum logLevel level, const char *msg)
{
	log_out_ss(level, msg);
}
void logErr(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_out_v(LOG_ERR, fmt, ap);
	va_end(ap);
}
void logWarn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_out_v(LOG_WARN, fmt, ap);
	va_end(ap);
}
void logInfo(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_out_v(LOG_INFO, fmt, ap);
	va_end(ap);
}
void logDbg(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_out_v(LOG_DBG, fmt, ap);
	va_end(ap);
}
bool logInit(enum logLevel level, char *path)
{
	char *mode;

	mode = configTryString("log/mode", "w");
	log_level = level;
	if (path) {
		if (!(log_file = fopen(path, mode))) {
			logErr("Could not open extra logfile at path %s", path);
			free(mode);
			return false;
		}
		log_file_fd = fileno(log_file);
	}
	logInfo("Log initialized at level %d\n", level);
	free(mode);
	return true;
}
void logClose(void)
{
	if (log_file)
		fclose(log_file);
}

