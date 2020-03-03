#include "log.h"
#include "xmem.h"

static enum logLevel log_level = LOG_NONE;

static FILE *log_file[LOG_FILE_MAX_COUNT] = {0};


enum logLevel logLevelParse(char *str)
{
	enum logLevel res = LOG_NONE;
	char *tok;
	char log_str[] = LOG_LEVEL_USAGE_STR;

	errno = 0;
	res = strtol(str, NULL, 0);
	if (!errno && res)
		return res;

	strtok(log_str, " ");
	for (res = LOG_NONE; (tok = strtok(NULL, " ")); ++res) {
		if (!strcmp(tok, str))
			return res;
	}
	return LOG_NONE;
}

static void log_out_v(enum logLevel level, const char *fmt, va_list ap)
{
	size_t i;
	va_list aq;
	if (level > log_level)
		return;
	for (i = 0; (i < LOG_FILE_MAX_COUNT) && (log_file[i]); ++i) {
		va_copy(aq, ap);
		vfprintf(log_file[i], fmt, aq);
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

