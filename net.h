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
#include "sig.h"

struct synNetContext {
	uSynergyContext *syn_ctx;
	struct addrinfo *hostinfo;
	int fd;
};
bool synNetInit(struct synNetContext *net_ctx, uSynergyContext *syn_ctx, const char *host, const char *port);
void netPoll(struct synNetContext *snet_ctx, struct wlContext *wl_ctx);
bool synNetDisconnect(struct synNetContext *snet_ctx);

