#include "log.h"
#include "config.h"
#include "xmem.h"
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

static enum logLevel log_level = LOG_NONE;

static FILE *log_file;


static void log_print_ts(FILE *out)
{
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        fprintf(out, "%" PRIdMAX ".%09ld",
                        (intmax_t)ts.tv_sec,
                        ts.tv_nsec);
}
static char *log_level_get_str(enum logLevel level)
{
	static char *log_level_str[] = {
		"NONE",
		"ERROR",
		"INFO",
		"DEBUG"
	};
	assert(level < (sizeof(log_level_str)/sizeof(*log_level_str)));
	return log_level_str[level];
}

static void log_out_v_(FILE *f, enum logLevel level, const char *fmt, va_list ap)
{
	size_t i;
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

void logOut(enum logLevel level, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_out_v(level, fmt, ap);
	va_end(ap);
}
void logErr(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	log_out_v(LOG_ERR, fmt, ap);
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
	log_level = level;
	if (!path) {
		path = configTryString("log/path", NULL);
	}
	if (path) {
		if (!(log_file = fopen(path, configTryString("log/mode", "w")))) {
			logErr("Could not open extra logfile at path %s", path);
			return false;
		}
	}
	logInfo("Log initialized at level %d\n", level);
	return true;
}
void logClose(void)
{
	if (log_file)
		fclose(log_file);
}

