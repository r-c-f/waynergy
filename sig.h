#pragma once
#include <signal.h>
#include <stdbool.h>
#include "log.h"


extern volatile sig_atomic_t sigDoExit;
extern volatile sig_atomic_t sigDoRestart;
void Exit(void);
void Restart(void);

static inline bool sigHandleCheck(void)
{
	return sigDoExit || sigDoRestart;
}
static inline void sigHandleRun(void)
{
	if (sigDoExit) {
		logInfo("Exit signal %s received, exiting...", sys_siglist[sigDoExit]);
		Exit();
	}
	if (sigDoRestart) {
		logInfo("Restart signal %s received, restarting...", sys_siglist[sigDoRestart]);
		Restart();
	}
}
