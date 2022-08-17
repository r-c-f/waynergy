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
	int sock, lsock;
	int child_stat;
	char *name;

	struct sockaddr_un addr = {
		.sun_family = AF_UNIX,
		.sun_path = "peer_sock_test",
	};
	
	unlink("peer_sock_test");

	host = getpid();
	child = fork();
	if (child == -1) {
		logPErr("fork");
		return false;
	}

	if (child) {
		/* host side */
		if ((lsock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
			logPErr("socket");
			return false;
		}
		if ((bind(lsock, (struct sockaddr *)&addr, SUN_LEN(&addr))) == -1) {
			logPErr("bind");
			return false;
		}
		if ((listen(lsock, 2)) == -1) {
			logPErr("listen");
			return false;
		}
		if ((sock = accept(lsock, NULL, NULL)) == -1) {
			logPErr("accept");
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
		sleep(5);
		
		if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
			logPErr("child: socket");
			exit(1);
		}
		if (connect(sock, (struct sockaddr *)&addr, SUN_LEN(&addr)) == -1) {
			logPErr("child: connect");
			exit(2);
		}

		logDbg("Child pid %d, host pid %d", child, host);
		if (!(name = osGetPeerProcName(sock))) {
			exit(3);
		}
		logDbg("Got peer name: %s\n", name);
		if (strcmp(name, orig_name)) {
			exit(4);
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

