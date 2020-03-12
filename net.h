#pragma once
#include "uSynergy.h"                                                           
#include "wayland.h"                                                            
#include "fdio_full.h"                                                          
#include "clip.h"
#include <stdlib.h>                                                             
#include <stdbool.h>                                                            
#include <unistd.h>                                                             
#include <sys/types.h>                                                          
#include <fcntl.h>                                                              
#include <poll.h>                                                               
#include <limits.h>                                                             
#include <errno.h>                                                              
#include <sys/socket.h>                                                         
#include <netinet/in.h>                                                         
#include <arpa/inet.h>                                                          
#include <sys/stat.h>                                                           
#include <netdb.h>                                                              
#include <time.h>
#include <signal.h>

bool synNetConfig(uSynergyContext *context, char *host, char *port);
bool netPollLoop(void);
bool synNetDisconnect(void);

extern volatile sig_atomic_t sigDoExit;
extern volatile sig_atomic_t sigDoRestart;
void Exit(void);
void Restart(void);
