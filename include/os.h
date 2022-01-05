#pragma once

extern char *osConfigPathOverride;
extern int osGetAnonFd(void);
extern char *osGetRuntimePath(char *name);
extern char *osGetHomeConfigPath(char *name);
/* drop setuid and setgid privileges; aborts on failure, as it should. */
extern void osDropPriv(void);
