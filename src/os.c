#include <sys/mman.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ssb.h"
#include "xmem.h"
#include "log.h"

#ifdef __FreeBSD__
#include <sys/param.h>
#endif

bool osFileExists(const char *path)
{
	struct stat buf;

	if (stat(path, &buf)) {
		return false;
	}
	if (!S_ISREG(buf.st_mode)) {
		logWarn("%s is not a regular file as expected", path);
		return false;
	}
	return true;
}
bool osMakeParentDir(const char *path, mode_t mode)
{
	char *c;
	char parent_path[strlen(path) + 1];

	if (!strchr(path, '/'))
		return true;
	strcpy(parent_path, path);

	for (c = strchr(parent_path + 1, '/'); c; c = strchr(c + 1, '/')) {
		*c = '\0';
		if (mkdir(parent_path, mode)) {
			if (errno != EEXIST) {
				*c = '/';
				return false;
			}
		}
		*c = '/';
	}
	return true;
}

int osGetAnonFd(void)
{
	#if defined(__linux__) || ((defined(__FreeBSD__) && (__FreeBSD_version >= 1300048)))
	return memfd_create("waynergy-anon-fd", MFD_CLOEXEC);
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
		fprintf(stderr, "Running as root, dropping ancillary groups\n");
		if (setgroups(1, &new_gid)) {
			/* if we're privileged we have not initialized the
			 * log yet */
			perror("Could not drop ancillary groups");
			abort();
		}
	}
	/* POSIX calls this permanent, and it seems to be the case on the BSDs
	 * and Linux. */
	if (new_gid != old_gid) {
		fprintf(stderr, "Dropping gid from %d to %d\n", old_gid, new_gid);
		if (setregid(new_gid, new_gid)) {
			perror("Could not set group IDs");
			abort();
		}
	}
	if (new_uid != old_uid) {
		fprintf(stderr, "Dropping uid from %d to %d\n", old_uid, new_uid);
		if (setreuid(new_uid, new_uid)) {
			perror("Could not set user IDs");
			abort();
		}
	}
}


#ifdef __linux__
char *osGetPeerProcName(int fd)
{
	struct ucred uc;
	socklen_t len = sizeof(uc);
	char *lf = NULL;
	char *path = NULL;
	FILE *f = NULL;
	struct ssb s = {0};

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &uc, &len) == -1) {
		logPErr("GetPeerProcName: getsockopt() failure");
		return NULL;
	}

	xasprintf(&path, "/proc/%d/comm", uc.pid);
	if (!(f = fopen(path, "r"))) {
		logPErr("Could not open file");
		goto done;
	}
	if (!ssb_readfile(&s, f)) {
		logPErr("Could not read process name");
		ssb_free(&s);
		goto done;
	}
	/* strip the new line */
	lf = strchr(s.buf, '\n');
	ssb_xtruncate(&s, lf - s.buf);
done:
	free(path);
	if (f) {
		fclose(f);
	}
	return s.buf;
}
#elif defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/user.h>
#include <sys/ucred.h>
#include <sys/sysctl.h>
char *osGetPeerProcName(int fd)
{
	char *name = NULL;

	struct xucred cred = {0};
	socklen_t slen = sizeof(cred);
	size_t len = sizeof(cred);

	struct kinfo_proc *kip = NULL;
	int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID};

	if (getsockopt(fd, SOL_LOCAL, LOCAL_PEERCRED, &cred, &slen) == -1) {
		logPErr("GetPeerProcName: getsockopt() failure");
		goto done;
	}
	logDbg("Got peer pid of %d", cred.cr_pid);

	mib[3] = cred.cr_pid;
	if (sysctl(mib, nitems(mib), NULL, &len, NULL, 0) == -1) {
		logPErr("sysctl failed to get size");
		goto done;
	}
	kip = xmalloc(len);

	if (sysctl(mib, nitems(mib), kip, &len, NULL, 0) == -1) {
		logPErr("sysctl failed to get proc info");
		goto done;
	}
	if ((len != sizeof(*kip)) ||
	    (kip->ki_structsize != sizeof(*kip)) ||
	    (kip->ki_pid != cred.cr_pid)) {
		logErr("returned procinfo is unusable");
		goto done;
	}
	name = xstrdup(kip->ki_comm);

done:
	free(kip);
	return name;
}
#else
char *osGetPeerProcName(int fd)
{
	logErr("osGetPeerProcName not implemented for this platform");
	return NULL;
}
#endif

