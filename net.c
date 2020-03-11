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

static struct addrinfo *hostinfo;
static int synsock = -1;
extern struct wlContext wlContext;


static bool syn_connect(uSynergyCookie cookie)
{
	synNetDisconnect();
	for (; hostinfo; hostinfo = hostinfo->ai_next) {
		if ((synsock = socket(hostinfo->ai_family, hostinfo->ai_socktype, hostinfo->ai_protocol)) == -1)
			continue;
		if (connect(synsock, hostinfo->ai_addr, hostinfo->ai_addrlen))
			continue;
		return true;
	}
	return false;
}

static bool syn_send(uSynergyCookie cookie, const uint8_t *buf, int len)
{
	return write_full(synsock, buf, len);
}

enum net_pollfd_id {
	POLLFD_WL,
	POLLFD_SYN,
	POLLFD_SEL_0,
	POLLFD_SEL_1,
	POLLFD_COUNT
};
static bool syn_recv(uSynergyCookie cookie, uint8_t *buf, int max_len, int *out_len)
{
	uint32_t psize;
	int wlfd = wlPrepareFd(&wlContext);
	int clipfd[2];
	clipPrepareFd(&wlContext, clipfd);
	struct pollfd pollfds[POLLFD_COUNT]= {
	int pfd_slot = POLLFD_SYN;
	while (1) {
		pollfds[POLLFD_WL].fd = wlfd;
		pollfds[POLLFD_WL].events = POLLIN | POLLHUP;
		pollfds[POLLFD_SYN].fd = synfd;
		pollfds[POLLFD_SYN].events = POLLIN | POLLHUP;
		pfd_slot = POLLFD_SYN;
		for (int i = 0; i < 2; ++i) {
			if (clipfd[i] != -1) {
				++pfd_slot;
				pollfds[pfd_slot].fd = clipfd[i];
				pollfds[pfd_slot].events = POLLIN | POLLHUP;
			}
		}
		if (poll(pollfds, pfd_slot + 1, -1) < 1) {
			return false;
		}
		if (pollfds[POLLFD_SYN].revents & POLLIN) {
			break;
		}
		wlPollProc(&wlContext, pollfds[POLLFD_WL].revents);
		for (int i = POLLFD_SEL_0; i < (pfd_slot + 1); ++i) {
			clipPollProc(pollfds + i);
		}
	}
	if (!read_full(synsock, &psize, sizeof(psize))) {
		perror("read_full");
		return false;
	}
	memmove(buf, &psize, sizeof(psize));
	buf += 4;
	max_len -=4;
	*out_len = 4;
	psize = ntohl(psize);
	if (!read_full(synsock, buf, psize)) {
		perror("read_full");
		return false;
	}
	*out_len += psize;
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

bool synNetConfig(uSynergyContext *context, char *host, char *port)
{
	/* trim away any newline garbage */
	for (char *c = host; *c; ++c) {
		if (*c == '\n') {
			*c = '\0';
			break;
		}
	}
	if (getaddrinfo(host, port, NULL, &hostinfo))
		return false;
	context->m_connectFunc = syn_connect;
	context->m_sendFunc = syn_send;
	context->m_receiveFunc = syn_recv;
	context->m_sleepFunc = syn_sleep;
	context->m_getTimeFunc = syn_get_time;
	return true;
}

bool synNetDisconnect(void)
{
	if (synsock == -1)
		return false;
	shutdown(synsock, SHUT_RDWR);
	close(synsock);
	return true;
}

