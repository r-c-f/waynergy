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
        /* stop clipbpoard monitors */
        for (int i = 0; i < 2; ++i) {
                if (clipMonitorPid[i] != -1) {
                        kill(clipMonitorPid[i], SIGTERM);
                        waitpid(clipMonitorPid[i], NULL, 0);
                }
        }
        /*close stuff*/
        synNetDisconnect(&synNetContext);
        logClose();
        wlClose(&wlContext);
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
        logErr("Could not re-exec: %s", strerror(errno));
        exit(EXIT_FAILURE);
}

static void sig_handle(int sig, siginfo_t *si, void *context)
{
        switch (sig) {
                case SIGALRM:
                        logOutSig(LOG_ERR, "Alarm timeout encountered -- probably disconnecting");
                        break;
                case SIGTERM:
                case SIGINT:
                case SIGQUIT:
                        sigDoExit = sig;
                        break;
                case SIGUSR1:
                        sigDoRestart = true;
                        break;
                case SIGCHLD:
			if (si->si_status) {
				logOutSig(LOG_WARN, "Child died with nonzero status");
			} else {
				logOutSig(LOG_DBG, "Child died -- successful status");
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
		sa.sa_flags = SA_RESTART | (state ? 0 : SA_NOCLDWAIT);
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
        //otherse can restart
        sa.sa_flags |= SA_RESTART;
        sigaction(SIGTERM, &sa, NULL);
        sigaction(SIGINT, &sa, NULL);
        sigaction(SIGQUIT, &sa, NULL);
        sigaction(SIGUSR1, &sa, NULL);
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
