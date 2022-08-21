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
static struct timespec log_start = {0};


static void log_print_ts(FILE *out)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec -= log_start.tv_sec;
	ts.tv_nsec -= log_start.tv_nsec;
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
	va_end(aq);
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
	log_level = level;
	clock_gettime(CLOCK_MONOTONIC, &log_start);
#if !defined(WAYNERGY_TEST)
	char *mode = configTryString("log/mode", "w");
	if (path) {
		if (!(log_file = fopen(path, mode))) {
			logErr("Could not open extra logfile at path %s", path);
			free(mode);
			return false;
		}
		log_file_fd = fileno(log_file);
	}
	free(mode);
#endif
	logInfo("Log initialized at level %d", level);
	return true;
}
void logClose(void)
{
	if (log_file)
		fclose(log_file);
}

/* signal-safe logging */ 

#define INT32_BUFLEN 12
static char *uint32_to_str(uint32_t in, char *out)
{
        int i;
        int digits;
        if (!in) {
                strcpy(out, "0");
                return out;
        }
        for (i = INT32_BUFLEN - 2; in; --i) {
                out[i] = '0' + (in % 10);
                in /= 10;
        }
        /* shift back by number of unused digits */
        digits = INT32_BUFLEN - 2 - i;
        memmove(out, out + i + 1, digits);
        out[digits] = 0;
        return out;
}
static char *int32_to_str(int32_t in, char out[static INT32_BUFLEN])
{
        if (in == INT_MIN) {
                strcpy(out, "INT_MIN");
                return out;
        } else if (in < 0) {
                in *= -1;
                uint32_to_str(in, out + 1);
                out[0] = '-';
        } else {
                uint32_to_str(in, out);
        }
        return out;
}
static void log_out_ss(enum logLevel level, const char *str)
{
	if (level > log_level)
		return;
	write_full(STDERR_FILENO, str, strlen(str), 0);
	if (log_file) {
		write_full(log_file_fd, str, strlen(str), 0);
	}
}
static void log_print_ts_ss(int out_fd)
{
	char buf[INT32_BUFLEN];
	char zero = '0';
	char dot = '.';
	struct timespec ts;
	int i, numlen;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ts.tv_sec -= log_start.tv_sec;
	ts.tv_nsec -= log_start.tv_nsec;
	if (ts.tv_nsec < 0 ) {
		--ts.tv_sec;
		ts.tv_nsec += 1000000000;
	}
	uint32_to_str(ts.tv_sec, buf);
	write_full(out_fd, buf, strlen(buf), 0);
	write_full(out_fd, &dot, 1, 0);
	uint32_to_str(ts.tv_nsec, buf);
	numlen = strlen(buf);
	for (i = 0; i < 9 - numlen; ++i ) {
		write_full(out_fd, &zero, 1, 0);
	}
	write_full(out_fd, buf, strlen(buf), 0);
}
void logOutSigStart(enum logLevel level)
{
	log_print_ts_ss(STDERR_FILENO);
	if (log_file) {
		log_print_ts_ss(log_file_fd);
	}
	log_out_ss(level, ": [");
	log_out_ss(level, log_level_get_str(level));
	log_out_ss(level, "] ");
}
void logOutSigChar(enum logLevel level, char c)
{
	char buf[2] = {c};
	log_out_ss(level, buf);
}
void logOutSigEnd(enum logLevel level)
{
	logOutSigChar(level, '\n');
}
void logOutSigStr(enum logLevel level, const char *str)
{
	log_out_ss(level, str);
}
void logOutSig(enum logLevel level, const char *msg)
{
	logOutSigStart(level);
	logOutSigStr(level, msg);
	logOutSigEnd(level);
}
void logOutSigI32(enum logLevel level, int32_t val)
{
	char buf[INT32_BUFLEN];
	int32_to_str(val, buf);
	log_out_ss(level, buf);
}
void logOutSigU32(enum logLevel level, uint32_t val)
{
	char buf[INT32_BUFLEN];
	uint32_to_str(val, buf);
	log_out_ss(level, buf);
}

