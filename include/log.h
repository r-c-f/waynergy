#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
	LOG_DBGSYN,
	LOG__INVALID, /* also serves as count */
};
enum logLevel logLevelFromString(const char *s);

bool logInit(enum logLevel level, char *path);
void logOutV(enum logLevel level, const char *fmt, va_list ap);
void logOut(enum logLevel level, const char *fmt, ...);
/* standard log functions */
void logErr(const char *fmt, ...);
void logWarn(const char *fmt, ...);
void logInfo(const char *fmt, ...);
void logDbg(const char *fmt, ...);
void logDbgSyn(const char *fmt, ...);
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
#define logPDbgSyn(msg) do { \
	logDbgSyn("%s: %s: %s", __func__, (msg), strerror(errno)); \
} while (0)
void logClose(void);

/* Signal-safe logging
 *
 * - Start with logOutSigStart(YOU_LEVEL)
 * - Use any combination of the logOutSigTYPE functions
 * - End with logOutSigEnd()
*/
void logOutSigStart(enum logLevel level);
void logOutSigEnd(enum logLevel level);
void LogOutSigChar(enum logLevel level, char c);
void logOutSigStr(enum logLevel level, const char *str);
void logOutSigI32(enum logLevel level, int32_t val);
void logOutSigU32(enum logLevel level, uint32_t val);
/* logOutSig retains normal behavior */
void logOutSig(enum logLevel level, const char *msg);
