#include <sys/wait.h>
#include "sig.h"
#include "clip.h"
#include "wayland.h"





volatile sig_atomic_t sigDoExit = 0;
volatile sig_atomic_t sigDoRestart = 0;
extern struct wlContext wlContext;
extern uSynergyContext synContext;
extern struct synNetContext synNetContext;

static char **argv_reexec;
static void cleanup(void)
{
	wlIdleInhibit(&wlContext, false);
	/* stop clipboard monitors */
	for (int i = 0; i < 2; ++i) {
		if (clipMonitorPid[i] != -1) {
			kill(clipMonitorPid[i], SIGTERM);
		}
	}
	/*close stuff*/
	synNetDisconnect(&synNetContext);
	logClose();
	wlClose(&wlContext);
	/* kill all related processes that remain alive */
	kill(-1 * getpgid(getpid()), SIGTERM);
	/*unmask any caught signals*/

}

void Exit(int status)
{
	cleanup();
	exit(status);
}
void Restart(void)
{
	cleanup();
	errno = 0;
	execvp(argv_reexec[0], argv_reexec);
	logPErr("reexec");
	exit(EXIT_FAILURE);
}

static void sig_handle(int sig, siginfo_t *si, void *context)
{
	int level;
	switch (sig) {
		case SIGALRM:
			logOutSig(LOG_ERR, "Alarm timeout encountered -- probably disconnecting");
			break;
		case SIGTERM:
		case SIGINT:
		case SIGQUIT:
			if (sigDoExit) {
				logOutSig(LOG_ERR, "received unhandled quit request, aborting");
				abort();
			}
			sigDoExit = sig;
			break;
		case SIGPIPE:
			logOutSig(LOG_WARN, "Broken pipe, restarting");
		case SIGUSR1:
			sigDoRestart = true;
			break;
		case SIGCHLD:
			if (si && si->si_code == CLD_EXITED) {
				level = si->si_status ? LOG_WARN : LOG_DBG;
				logOutSigStart(level);
				logOutSigStr(level, "Child died: PID ");
				logOutSigI32(level, si->si_pid);
				logOutSigStr(level, ", Status ");
				logOutSigI32(level, si->si_status);
				logOutSigEnd(level);
			} else {
				logOutSig(LOG_DBG, "SIGCHLD sent without exit");
			}
			break;
		default:
			logOutSig(LOG_ERR, "Unhandled signal");
	}
}

void sigWaitSIGCHLD(bool state)
{
	struct sigaction sa = {0};
	static struct sigaction sa_old;
	static bool we_set_wait;
	if (state || !we_set_wait) {
		sa.sa_sigaction = sig_handle;
		sa.sa_flags = SA_SIGINFO | SA_RESTART | (state ? 0 : SA_NOCLDWAIT);
		sigaction(SIGCHLD, &sa, &sa_old);
	} else {
		//use old sigaction, in case weird flags exist.
		sa_old.sa_flags &= ~SA_NOCLDWAIT;
		sigaction(SIGCHLD, &sa_old, NULL);
	}
	logDbg("%srequiring wait() on SIGCHLD", (state ? "" : "not "));
}


void sigHandleInit(char **argv)
{
	struct sigaction sa;
	sigset_t set;
	argv_reexec = argv;
	/* set up signal handler */
	sa.sa_sigaction = sig_handle;
	sigemptyset(&sa.sa_mask);
	//alarm should trigger EINTR
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGALRM, &sa, NULL);
	//others can restart
	sa.sa_flags |= SA_RESTART;
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	//don't zombify
	sa.sa_flags |= SA_NOCLDWAIT;
	sigaction(SIGCHLD, &sa, NULL);
	sigemptyset(&set);
	sigaddset(&set, SIGUSR1);
	sigaddset(&set, SIGTERM);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGALRM);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
}
