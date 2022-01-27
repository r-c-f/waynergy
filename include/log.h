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
	LOG_WARN,
	LOG_INFO,
	LOG_DBG,
	LOG__INVALID, /* also serves as count */
};
enum logLevel logLevelFromString(const char *s);

bool logInit(enum logLevel level, char *path);
void logOut(enum logLevel level, const char *fmt, ...);
/* signal-safe version of logOut */
void logOutSig(enum logLevel level, const char *msg);
void logErr(const char *fmt, ...);
void logWarn(const char *fmt, ...);
void logInfo(const char *fmt, ...);
void logDbg(const char *fmt, ...);
#define logPErr(msg) do { \
	logErr("%s: %s: %s", __func__, (msg), strerror(errno)); \
} while (0)
#define logPWarn(msg) do { \
	logWarn("%s: %s: %s", __func__, (msg), strerror(errno)); \
} while (0)
#define logPInfo(msg) do { \
	logInfo("%s: %s: %s", __func__, (msg), strerror(errno)); \
} while (0)
#define logPDbg(msg) do { \
	logDbg("%s: %s: %s", __func__, (msg), strerror(errno)); \
} while (0)
void logClose(void);

