#pragma once
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include "wayland.h"
#include "log.h"
#include "net.h"


enum sigExitStatus {
	SES_SUCCESS = EXIT_SUCCESS, /* everything normal */
	SES_FAILURE = EXIT_FAILURE, /* general failure */
	SES_ERROR_SYN, 	 /* synergy protocol error */
	SES_ERROR_WL, 	 /* wayland error. DO NOT attempt cleanup functions related to wayland. */
};

extern volatile sig_atomic_t sigDoExit;
extern volatile sig_atomic_t sigDoRestart;
void Exit(enum sigExitStatus status);
void Restart(enum sigExitStatus status);
/* exit or restart, for situations like wayland protocol errors beyond our
 * control that don't necessarily mean the compositor no longer exists or the
 * session actually ended */
void ExitOrRestart(enum sigExitStatus status);
void sigHandleInit(char **argv);
void sigWaitSIGCHLD(bool state);

static inline bool sigHandleCheck(void)
{
	return sigDoExit || sigDoRestart;
}
static inline void sigHandleRun(void)
{
	if (sigDoExit) {
		logInfo("Exit signal %s received, exiting...", strsignal(sigDoExit));
		Exit(SES_SUCCESS);
	}
	if (sigDoRestart) {
		logInfo("Restart signal %s received, restarting...", strsignal(sigDoRestart));
		Restart(SES_SUCCESS);
	}
}
