#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <errno.h>



enum logLevel {
	LOG_NONE = 0,
	LOG_ERR,
	LOG_INFO,
	LOG_DBG,
};
#define LOG_FILE_MAX_COUNT 4
#define LOG_FILE_MAX_COUNT_STR "4"

bool logInit(enum logLevel level, FILE **logfiles);
void logOut(enum logLevel level, const char *fmt, ...);
void logErr(const char *fmt, ...);
void logInfo(const char *fmt, ...);
void logDbg(const char *fmt, ...);
void logClose(void);

