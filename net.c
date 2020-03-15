#include "uSynergy.h"
#include "wayland.h"
#include "fdio_full.h"
#include "clip.h"
#include "net.h"
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


static bool syn_connect(uSynergyCookie cookie)
{
	struct addrinfo *h;
	struct synNetContext *snet_ctx = cookie;
	uSynergyContext *syn_ctx = snet_ctx->syn_ctx;
	logDbg("syn_connect trying to connect");
	synNetDisconnect(snet_ctx);
	for (h = snet_ctx->hostinfo; h; h = h->ai_next) {
		if ((snet_ctx->fd = socket(h->ai_family, h->ai_socktype | SOCK_CLOEXEC, h->ai_protocol)) == -1)
			continue;
		if (connect(snet_ctx->fd, h->ai_addr, h->ai_addrlen))
			continue;
		syn_ctx->m_lastMessageTime = syn_ctx->m_getTimeFunc();
		return true;
	}
	return false;
}

static bool syn_send(uSynergyCookie cookie, const uint8_t *buf, int len)
{
	struct synNetContext *snet_ctx = cookie;
	return write_full(snet_ctx->fd, buf, len);
}

enum net_pollfd_id {
	POLLFD_SYN,
	POLLFD_WL,
	POLLFD_CB,
	POLLFD_P,
	POLLFD_COUNT
};
void netPoll(struct synNetContext *snet_ctx, struct wlContext *wl_ctx)
{
	int ret;
	uSynergyContext *syn_ctx = snet_ctx->syn_ctx;
	if (snet_ctx->fd == -1) {
		logErr("INVALID FILE DESCRIPTOR for synergy context");
	}
	int wlfd = wlPrepareFd(wl_ctx);
	struct pollfd pfd[] = {
		{
			.fd = snet_ctx->fd,
			.events = POLLIN | POLLHUP
		},
		{
			.fd = wlfd,
			.events = POLLIN | POLLHUP
		},
		{
			.fd = clipMonitorFd[0],
			.events = POLLIN | POLLHUP
		},
		{
			.fd = clipMonitorFd[1],
			.events = POLLIN | POLLHUP
		}
	};
	int nfd = syn_ctx->m_connected ? POLLFD_COUNT : 1;
	while ((ret = poll(pfd, nfd, USYNERGY_IDLE_TIMEOUT)) > 0) {
		sigHandleRun();
		if (pfd[POLLFD_SYN].revents & POLLIN) {
			uSynergyUpdate(syn_ctx);
		}
		if ((syn_ctx->m_getTimeFunc() - syn_ctx->m_lastMessageTime) > USYNERGY_IDLE_TIMEOUT) {
			logErr("Synergy imeout encountered -- disconnecting");
			synNetDisconnect(snet_ctx);
			return;
		}
		sigHandleRun();
		/* ignore everything else until synergy is ready */
		if (syn_ctx->m_connected) {
			wlPollProc(wl_ctx, pfd[POLLFD_WL].revents);
			sigHandleRun();
			clipMonitorPollProc(&pfd[POLLFD_CB]);
			sigHandleRun();
			clipMonitorPollProc(&pfd[POLLFD_P]);
			sigHandleRun();
		}
		nfd = syn_ctx->m_connected ? POLLFD_COUNT : 1;
	}
	if (!ret) {
		logErr("Poll timeout encountered -- disconnectin synergy");
		synNetDisconnect(snet_ctx);
	}
	sigHandleRun();
}



static bool syn_recv(uSynergyCookie cookie, uint8_t *buf, int max_len, int *out_len)
{
	struct synNetContext *snet_ctx = cookie;
	alarm(USYNERGY_IDLE_TIMEOUT/1000);
	*out_len = read(snet_ctx->fd, buf, max_len);
	alarm(0);
	if (*out_len < 1)
		return false;
	return true;
}

static void syn_sleep(uSynergyCookie cookie, int ms)
{
	usleep(ms * 1000);
}

static uint32_t syn_get_time(void)
{
	uint32_t ms;
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	ms = ts.tv_sec * 1000;
	ms += ts.tv_nsec / 1000000;
	return ms;
}

bool synNetInit(struct synNetContext *snet_ctx, uSynergyContext *context, const char *host, const char *port)
{
	/* trim away any newline garbage */
	char host_noline[strlen(host) + 1];
	strcpy(host_noline, host);
	for (char *c = host_noline; *c; ++c) {
		if (*c == '\n') {
			*c = '\0';
			break;
		}
	}
	if (getaddrinfo(host_noline, port, NULL, &snet_ctx->hostinfo))
		return false;
	snet_ctx->syn_ctx = context;
	snet_ctx->fd = -1;
	context->m_connectFunc = syn_connect;
	context->m_sendFunc = syn_send;
	context->m_receiveFunc = syn_recv;
	context->m_sleepFunc = syn_sleep;
	context->m_getTimeFunc = syn_get_time;
	context->m_cookie = snet_ctx;
	return true;
}

bool synNetDisconnect(struct synNetContext *snet_ctx)
{
	if (snet_ctx->fd == -1)
		return false;
	shutdown(snet_ctx->fd, SHUT_RDWR);
	close(snet_ctx->fd);
	snet_ctx->fd = -1;
	snet_ctx->syn_ctx->m_connected = false;
	return true;
}

