#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include "xmem.h"

int osGetAnonFd(void)
{
	#ifdef __linux__
	return memfd_create("waynergy-anon-fd", MFD_CLOEXEC);
	#endif
	#ifdef __FreeBSD__
	return shm_open(SHM_ANON, O_RDWR | O_CREAT, 0600);
	#endif
	return fileno(tmpfile());
}
char *osGetRuntimePath(char *name)
{
	char *res;
	char *base;

	if (!(base = getenv("XDG_RUNTIME_DIR"))) {
		base = "/tmp";
	}
	xasprintf(&res, "%s/%s", base, name);
	return res;
}
char *osConfigPathOverride;
char *osGetHomeConfigPath(char *name)
{
	char *res;
        char *env;

	if (osConfigPathOverride) {
		xasprintf(&res, "%s/%s", osConfigPathOverride, name);
	} else if ((env = getenv("XDG_CONFIG_HOME"))) {
		xasprintf(&res,"%s/waynergy/%s", env, name);
	} else {
		if (!(env = getenv("HOME")))
			return NULL;
		xasprintf(&res, "%s/.config/waynergy/%s",env, name);
	}
	return res;
}
