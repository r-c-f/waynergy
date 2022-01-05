#include <sys/mman.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <grp.h>
#include <unistd.h>
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
void osDropPriv(void)
{
	uid_t new_uid, old_uid;
	gid_t new_gid, old_gid;

	new_uid = getuid();
	old_uid = geteuid();
	new_gid = getgid();
	old_gid = getegid();

	if (!old_uid) {
		if (setgroups(1, &new_gid) != 1) {
			/* if we're privileged we have not initialized the
			 * log yet */
			perror("Could not drop ancillary groups");
			abort();
		}
	}
	/* POSIX calls this permanent, and it seems to be the case on the BSDs
	 * and Linux. */
	if (new_gid != old_gid) {
		if (setregid(new_gid, new_gid)) {
			perror("Could not set group IDs");
			abort();
		}
	}
	if (new_uid != old_uid) {
		if (setreuid(new_uid, new_uid)) {
			perror("Could not set user IDs");
			abort();
		}
	}
}

