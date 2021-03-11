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
#include <tls.h>
#include <assert.h>

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
		if (snet_ctx->tls) {
			if (!(snet_ctx->tls_ctx = tls_client())) {
				logErr("Could not create tls client context");
				return false;
			}
			struct tls_config *cfg;
			if (!(cfg = tls_config_new())) {
				logErr("Could not create tls configuration structure");
				tls_free(snet_ctx->tls_ctx);
				return false;
			}
			/* figure out certificate hash business */
			if (!(snet_ctx->tls_hash = configTryString("tls/hash", NULL))) {
				if (snet_ctx->tls_tofu) {
					logErr("No certificate hash available");
					tls_free(snet_ctx->tls_ctx);
					return false;
				}
				/* if we are trusting on frist use we just defer this
				 * until a successful handshake */
			}
			/* we operate on hashes instead -- this is fine for now */
			tls_config_insecure_noverifycert(cfg);
			tls_config_insecure_noverifyname(cfg);
			if (tls_configure(snet_ctx->tls_ctx, cfg)) {
				logErr("Could not configure TLS context: %s", tls_error(snet_ctx->tls_ctx));
				tls_config_free(cfg);
				tls_free(snet_ctx->tls_ctx);
				return false;
			}
			tls_config_free(cfg);
			if (tls_connect_socket(snet_ctx->tls_ctx, snet_ctx->fd, snet_ctx->host)) {
				logErr("tls_connect error: %s", tls_error(snet_ctx->tls_ctx));
				synNetDisconnect(snet_ctx);
				continue;
			}
			if (tls_handshake(snet_ctx->tls_ctx)) {
				logErr("tls_handshake error: %s", tls_error(snet_ctx->tls_ctx));
				synNetDisconnect(snet_ctx);
				continue;
			}
			const char *peer_hash = tls_peer_cert_hash(snet_ctx->tls_ctx);
			assert(peer_hash);
			if (!snet_ctx->tls_hash) {
				logInfo("Trust-on-first-use enabled, saving hash %s", tls_peer_cert_hash(snet_ctx->tls_ctx));
				snet_ctx->tls_hash = xstrdup(peer_hash);
				if (!(configWriteString("tls/hash", peer_hash))) {
					logErr("Could not save hash");
				}
			}
			if (strcasecmp(snet_ctx->tls_hash, peer_hash)) {
				logErr("CERTIFICATE HASH MISMATCH: %s (client) != %s (server)", snet_ctx->tls_hash, peer_hash);
				synNetDisconnect(snet_ctx);
				continue;
			}
		}
		syn_ctx->m_lastMessageTime = syn_ctx->m_getTimeFunc();
		return true;
	}
	return false;
}
static bool tls_write_full(struct tls *ctx, const unsigned char *buf, size_t len)
{
	while (len) {
		ssize_t ret;
		ret = tls_write(ctx, buf, len);
		if (ret == TLS_WANT_POLLIN || ret == TLS_WANT_POLLOUT) {
			continue;
		}
		if (ret == -1) {
			logErr("tls_write failed: %s", tls_error(ctx));
			return false;
		}
		buf += ret;
		len -= ret;
	}
	return true;
}

static bool syn_send(uSynergyCookie cookie, const uint8_t *buf, int len)
{
	struct synNetContext *snet_ctx = cookie;
	return snet_ctx->tls_ctx ?
		tls_write_full(snet_ctx->tls_ctx, buf, len) :
		write_full(snet_ctx->fd, buf, len, 0);
}
struct pollfd netPollFd[POLLFD_COUNT];
void netPollInit(void)
{
	for (int i = 0; i < POLLFD_COUNT; ++i) {
		netPollFd[i].events = POLLIN;
		netPollFd[i].fd = -1;
	}
}
void netPoll(struct synNetContext *snet_ctx, struct wlContext *wl_ctx)
{
	int ret;
	uSynergyContext *syn_ctx = snet_ctx->syn_ctx;
	if (snet_ctx->fd == -1) {
		logErr("INVALID FILE DESCRIPTOR for synergy context");
	}
	int wlfd = wlPrepareFd(wl_ctx);
	netPollFd[POLLFD_SYN].fd = snet_ctx->fd;
	netPollFd[POLLFD_WL].fd = wlfd;
	netPollFd[POLLFD_CLIP_MON].fd = clipMonitorFd;
	int nfd = syn_ctx->m_connected ? POLLFD_COUNT : 1;
	while ((ret = poll(netPollFd, nfd, USYNERGY_IDLE_TIMEOUT)) > 0) {
		sigHandleRun();
		if (netPollFd[POLLFD_SYN].revents & POLLIN) {
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
			wlPollProc(wl_ctx, netPollFd[POLLFD_WL].revents);
			sigHandleRun();
			clipMonitorPollProc(&netPollFd[POLLFD_CLIP_MON]);
			sigHandleRun();
			for (int i = POLLFD_CLIP_UPDATER; i < POLLFD_COUNT; ++i) {
				clipMonitorPollProc(netPollFd + i);
				sigHandleRun();
			}
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
	if (snet_ctx->tls_ctx) {
		do {
			*out_len = tls_read(snet_ctx->tls_ctx, buf, max_len);
		} while (*out_len == TLS_WANT_POLLIN || *out_len == TLS_WANT_POLLOUT);
	} else {
		*out_len = read(snet_ctx->fd, buf, max_len);
	}
	alarm(0);
	if (*out_len < 1) {
		logErr("Synergy receive timed out");
		snet_ctx->syn_ctx->m_lastError = USYNERGY_ERROR_TIMEOUT;
		return false;
	}
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

bool synNetInit(struct synNetContext *snet_ctx, uSynergyContext *context, const char *host, const char *port, bool tls, bool tofu)
{
	logInfo("Going to connect to %s at port %s", host, port);
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM
	};
	snet_ctx->host = xstrdup(host);
	if (getaddrinfo(host, port, &hints, &snet_ctx->hostinfo))
		return false;
	snet_ctx->syn_ctx = context;
	snet_ctx->fd = -1;
	snet_ctx->tls = tls;
	snet_ctx->tls_tofu = tofu;
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
	if (snet_ctx->tls_ctx) {
		if (tls_close(snet_ctx->tls_ctx)) {
			logErr("tls_close error: %s", snet_ctx->tls_ctx);
		}
		tls_free(snet_ctx->tls_ctx);
		snet_ctx->tls_ctx = NULL;
	}
	shutdown(snet_ctx->fd, SHUT_RDWR);
	close(snet_ctx->fd);
	snet_ctx->fd = -1;
	snet_ctx->syn_ctx->m_connected = false;
	return true;
}

