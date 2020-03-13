#pragma once
#include <signal.h>
#include <stdbool.h>
#include <sys/wait.h>
#include "wayland.h"
#include "log.h"
#include "net.h"

extern volatile sig_atomic_t sigDoExit;
extern volatile sig_atomic_t sigDoRestart;
void Exit(void);
void Restart(void);
void sigHandleInit(char **argv);

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
