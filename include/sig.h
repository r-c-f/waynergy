#pragma once
#include <signal.h>
#include <string.h>
#include <stdbool.h>
#include <sys/wait.h>
#include "wayland.h"
#include "log.h"
#include "net.h"

extern volatile sig_atomic_t sigDoExit;
extern volatile sig_atomic_t sigDoRestart;
void Exit(int status);
void Restart(void);
/* exit or restart, for situations like wayland protocol errors beyond our
 * control that don't necessarily mean the compositor no longer exists or the
 * session actually ended */
void ExitOrRestart(int status);
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
		Exit(EXIT_SUCCESS);
	}
	if (sigDoRestart) {
		logInfo("Restart signal %s received, restarting...", strsignal(sigDoRestart));
		Restart();
	}
}
