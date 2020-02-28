#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include "xmem.h"

int osGetAnonFd(void)
{
	#ifdef HAVE_MEMFD
	return memfd_create("swaynergy-anon-fd", MFD_CLOEXEC);
	#endif
	#ifdef HAVE_SHM_ANON
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
char *osGetHomeConfigPath(char *name)
{
	char *res;
        char *env;
	if ((env = getenv("XDG_CONFIG_HOME"))) {
		xasprintf(&res,"%s/swaynergy/%s", env, name);
	} else {
		if (!(env = getenv("HOME")))
			return NULL;
		xasprintf(&res, "%s/.config/swaynergy/%s",env, name);
	}
	return res;
}
