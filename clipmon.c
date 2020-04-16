#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wayland-client.h>
#include <spawn.h>
#include <unistd.h>
#include <sys/wait.h>
#include "wlr-data-control-unstable-v1.prot.h"
#include "xmem.h"
extern char **environ;

char *monitor_sun_path;

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

enum clipboard_id {
	CLIPBOARD = 0,
	PRIMARY
};
static void on_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *dc_offer, enum clipboard_id id)
{
	int pfd[2];
	posix_spawn_file_actions_t fa;
	pid_t pid;
	char *id_str = id ? "primary" : "clipboard";
	char *cid = id ? "p" : "c";
	struct Offer **coffer_ptr = id ? &clip_offer : &prim_offer;
	if (!dc_offer) {
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
	for (size_t i = 0; i < offer->mime_pos; ++i) {
		fprintf(stderr, "\t%s\n", offer->mime[i]);
		/*
		char *argv[] = {
			"waynergy-clip-update",
			get_offer_seq(),	
			cid,
			offer_mime[i],
			monitor_sun_path,
			NULL
		};*/
		char *argv[] = {
			"cat",
			"-",
			NULL
		};
		zwlr_data_control_offer_v1_receive(offer->dc_offer, offer->mime[i], pfd[1]);
		pipe(pfd);
		posix_spawn_file_actions_init(&fa);
		posix_spawn_file_actions_adddup2(&fa, STDIN_FILENO, pfd[0]);
		posix_spawnp(&pid, argv[0], &fa, NULL, argv, environ);
		close(pfd[0]);
	}
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
	struct wl_display *display;
	struct zwp_virtual_keyboard_v1 *keyboard;
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
