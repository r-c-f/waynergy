#include "../include/os.h"
#include "../include/log.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


bool get_anon_fd(void)
{
	return (osGetAnonFd() != -1);
}

bool get_peer_proc_name(char *orig_name)
{
	pid_t host, child;
	int sock[2];
	int child_stat;
	char *name;

	if (socketpair(AF_UNIX, SOCK_STREAM, 0, sock) == -1) {
		logPErr("socketpair");
		return false;
	}
	
	host = getpid();
	child = fork();
	if (child == -1) {
		logPErr("fork");
		return false;
	}

	if (child) {
		/* host side */
		close(sock[1]);
		if (!(name = osGetPeerProcName(sock[0]))) {
			logPErr("Failed on host");
			return false;
		}
		if (waitpid(child, &child_stat, 0) != child) {
			logPErr("Could not wait on child");
			return false;
		}
		switch (child_stat) {
			case 0:
				break;
			case 1:
				logPErr("Child could not get valid name");
				return false;
			case 2:
				logPErr("Child name mismatch");
				return false;
			default:
				logErr("Unknown exit code %d", child_stat);
				return false;
		}
	} else {
		/* child side */
		close(sock[0]);
		if (!(name = osGetPeerProcName(sock[1]))) {
			exit(1);
		}
		if (strcmp(name, orig_name)) {
			exit(2);
		}
		exit(0);
	}
	return true;
}



int main(int argc, char **argv)
{
	char *orig_name;
	/* strip out leading components */
	orig_name = strrchr(argv[0], '/');
	if (orig_name) {
		++orig_name;
	}
	if (!(*orig_name)) {
		logErr("invalid process name");
		return 1;
	}
	logInit(LOG_DBG, NULL);


	bool stat = true;
	stat = stat && get_anon_fd();
       	stat = stat && get_peer_proc_name(orig_name);
	return !stat;	
}

