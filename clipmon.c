#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <spawn.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include "fdio_full.h"
#include "wlr-data-control-unstable-v1.prot.h"
#include "xmem.h"
extern char **environ;

char *monitor_sun_path;

static struct wl_display *display;
static struct wl_seat *seat;
static struct wl_registry *registry;

static struct zwlr_data_control_manager_v1 *dc_manager;
static struct zwlr_data_control_device_v1 *dc_device;

#define MIME_MAX 256
struct Offer {
	struct zwlr_data_control_offer_v1 *dc_offer;
	char *mime[MIME_MAX];
	size_t mime_pos;
	struct Offer *prev;
	struct Offer *next;
};

struct Offer *offers;
struct Offer *clip_offer;
struct Offer *prim_offer;

static void OfferAdd(struct zwlr_data_control_offer_v1 *dc_offer)
{
	struct Offer *offer = xmalloc(sizeof(*offer));
	offer->dc_offer = dc_offer;
	offer->mime_pos = 0;
	offer->prev = NULL;
	offer->next = offers;
	offers = offer;
}
static struct Offer *OfferGet(struct zwlr_data_control_offer_v1 *dc_offer)
{
	struct Offer *offer;
	for (offer = offers; offer; offer = offer->next) {
		if (offer->dc_offer == dc_offer) {
			return offer;
		}
	}
	return NULL;
}
static void OfferDel(struct zwlr_data_control_offer_v1 *dc_offer)
{
	struct Offer *offer = OfferGet(dc_offer);
	if (!offer) {
		fprintf(stderr, "ERROR: Trying to delete nonexistent offer");
		return;
	}
	zwlr_data_control_offer_v1_destroy(offer->dc_offer);
	for (size_t i = 0; i < offer->mime_pos; ++i) {
		free(offer->mime[i]);
	}
	if (offer->next)
		offer->next->prev = offer->prev;
	if (offer->prev) {
		offer->prev->next = offer->next;
	} else {
		offers = offer->next;
	}
	free(offer);
}

unsigned char offer_seq;
static char *get_offer_seq(void)
{
	static char buf[2] = {0};
	buf[0] = 'a' + offer_seq % 26;
	buf[1] = 0;
	return buf;
}


static void on_offer(void *data, struct zwlr_data_control_offer_v1 *dc_offer, const char *mime_type)
{
	struct Offer *offer = OfferGet(dc_offer);
	if (!offer) {
		fprintf(stderr, "ERROR: Got mime for invalid offer!\n");
		return;
	}
	offer->mime[offer->mime_pos++] = strdup(mime_type);
	fprintf(stderr, "GOT MIME TYPE %s\n", mime_type);
}

static struct zwlr_data_control_offer_v1_listener offer_listener = {
	.offer = on_offer
};

static void on_data_offer(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *dc_offer)
{
	OfferAdd(dc_offer);
	fprintf(stderr, "Got new offer\n");
	zwlr_data_control_offer_v1_add_listener(dc_offer, &offer_listener, NULL);
}


static bool buf_append_file(char **buf, size_t *len, size_t *pos, int fd)
{
        size_t read_count;
        while ((read_count = read(fd, *buf + *pos, 1)) == 1) {
                fprintf(stderr, "READ %zd BYTES\n", read_count);
                *pos += read_count;
                if (*len - *pos <= 2) {
                        *buf = xrealloc(*buf, *len *= 2);
                }
        }
	fprintf(stderr, "DONE READING\n");
        return true;
}

enum clipboard_id {
	CLIPBOARD = 0,
	PRIMARY
};
static void on_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *dc_offer, enum clipboard_id id)
{
	int sock;
	char *buf;
	size_t data_len, mime_len, data_pos;
	unsigned char cseq;
	struct sockaddr_un sa = {0};

	strncpy(sa.sun_path, monitor_sun_path, sizeof(sa.sun_path));
        sa.sun_family = AF_UNIX;

	char *id_str = id ? "primary" : "clipboard";
	char cid = id ? 'p' : 'c';
	struct Offer **coffer_ptr = id ? &clip_offer : &prim_offer;
	if (!dc_offer) {
		if (*coffer_ptr)
			OfferDel((*coffer_ptr)->dc_offer);
		*coffer_ptr = NULL;
		return;
	}
	struct Offer *offer = OfferGet(dc_offer);
	if (!offer) {
		fprintf(stderr, "ERROR: Selected nonexistent offer");
		return;
	}
	*coffer_ptr= offer;
	fprintf(stderr, "Offer sequence: %s (%u)", get_offer_seq(), offer_seq);
	fprintf(stderr, "Selected (id %s) types:\n", id_str);
	pid_t pid[offer->mime_pos];
	int pfd[offer->mime_pos][2];
	for (size_t i = 0; i < offer->mime_pos; ++i) {
		fprintf(stderr, "\t%s\n", offer->mime[i]);
		pipe(&pfd[i][0]);
		if ((pid[i] = fork())) {
			/* parent */
			zwlr_data_control_offer_v1_receive(dc_offer, offer->mime[i], pfd[i][1]);
			close(pfd[i][0]);
		} else {
			/* child */
			data_len = 4000;
			data_pos = 0;
			buf = xmalloc(data_len);
			close(pfd[i][1]);
			buf_append_file(&buf, &data_len, &data_pos, pfd[i][0]);
			close(pfd[i][0]);
			mime_len = strlen(offer->mime[i]);
			if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
				exit(EXIT_FAILURE);
			}
			if (connect(sock, (struct sockaddr *)&sa, sizeof(sa)) == -1) {
				exit(EXIT_FAILURE);
			}
			if (!write_full(sock, get_offer_seq(), 1, 0)) {
				exit(EXIT_FAILURE);
			}
			if (!write_full(sock, &cid, 1, 0)) {
				exit(EXIT_FAILURE);
			}
			if (!write_full(sock, &mime_len, sizeof(mime_len), 0)) {
				exit(EXIT_FAILURE);
			}
			if (!write_full(sock, offer->mime[i], mime_len, 0)) {
				exit(EXIT_FAILURE);
			}
			if (!write_full(sock, &data_pos, sizeof(data_pos), 0)) {
				exit(EXIT_FAILURE);
			}
			if (!write_full(sock, buf, data_pos, 0)) {
				exit(EXIT_FAILURE);
			}
			shutdown(sock, SHUT_RDWR);
			close(sock);
			exit(EXIT_SUCCESS);
		}
	}
	wl_display_roundtrip(display);
	for (size_t i = 0; i < offer->mime_pos; ++i) {
		close(pfd[i][1]);
		waitpid(pid[i], NULL, 0);
	}

	fprintf(stderr, "DONE WITH OFFER %s\n", get_offer_seq());
	++offer_seq;
	return;
}
static void on_clipboard(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
	on_selection(data, device, offer, CLIPBOARD);
}

static void on_primary(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
	on_selection(data, device, offer, PRIMARY);
}

static void on_finished(void *data, struct zwlr_data_control_device_v1 *device)
{
	exit(1);
}

struct zwlr_data_control_device_v1_listener device_listener = {
	.data_offer = on_data_offer,
	.selection = on_clipboard,
	.primary_selection = on_primary,
	.finished = on_finished
};





static void on_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
	if (!strcmp(interface, wl_seat_interface.name)) {
		seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
	} else if (!strcmp(interface, zwlr_data_control_manager_v1_interface.name)) {
		dc_manager = wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, version);
	}
}
static void on_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
}
static struct wl_registry_listener registry_listener = {
	.global = on_global,
	.global_remove = on_global_remove
};



int main(int argc, char **argv)
{
	monitor_sun_path = argv[1];

	display = wl_display_connect(NULL);
	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);
	dc_device = zwlr_data_control_manager_v1_get_data_device(dc_manager, seat);
	zwlr_data_control_device_v1_add_listener(dc_device, &device_listener, NULL);
	while (1) {
		wl_display_dispatch(display);
	}
	return 0;
}
