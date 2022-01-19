#pragma once
#include <stdbool.h>

extern char *osConfigPathOverride;
extern int osGetAnonFd(void);
extern char *osGetRuntimePath(char *name);
extern char *osGetHomeConfigPath(char *name);
/* check if a file exists */
extern bool osFileExists(const char *path);
/* drop setuid and setgid privileges; aborts on failure, as it should. */
extern void osDropPriv(void);
