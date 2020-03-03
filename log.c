#include "log.h"
#include "xmem.h"
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <assert.h>

static enum logLevel log_level = LOG_NONE;

static FILE *log_file[LOG_FILE_MAX_COUNT] = {0};


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

static void log_out_v(enum logLevel level, const char *fmt, va_list ap)
{
	size_t i;
	va_list aq;
	if (level > log_level)
		return;
	for (i = 0; (i < LOG_FILE_MAX_COUNT) && (log_file[i]); ++i) {
		log_print_ts(log_file[i]);
		fprintf(log_file[i], ": [%s] ", log_level_get_str(level));
		va_copy(aq, ap);
		vfprintf(log_file[i], fmt, aq);
		putc('\n', log_file[i]);
		fflush(log_file[i]);
	}
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
bool logInit(enum logLevel level, FILE **logfiles)
{
	FILE *f;
	size_t i;

	log_level = level;
	for (i = 0; (i < LOG_FILE_MAX_COUNT) && (log_file[i] = logfiles[i]); ++i);
	logInfo("Log initialized at level %d\n", level);
	return true;
}
void logClose(void)
{
	size_t i;
	for (i = 0; (i < LOG_FILE_MAX_COUNT) && (log_file[i]); ++i) {
		fclose(log_file[i]);
	}
}

