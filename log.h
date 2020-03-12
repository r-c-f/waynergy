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

bool logInit(enum logLevel level, char *path);
void logOut(enum logLevel level, const char *fmt, ...);
/* signal-safe version of logOut */
void logOutSig(enum logLevel level, const char *msg);
void logErr(const char *fmt, ...);
void logInfo(const char *fmt, ...);
void logDbg(const char *fmt, ...);
void logClose(void);

