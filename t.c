#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <wayland-client-protocol.h>
#include "wlr-data-control-unstable-v1.prot.h"
#include "fdio_full.h"
#include <poll.h>
#include "os.h"

static struct wl_seat *seat;
static struct wl_display *display;
static struct wl_registry *registry;
static struct zwlr_data_control_manager_v1 *data_manager;


static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
        if (strcmp(interface, wl_seat_interface.name) == 0) {
                seat = wl_registry_bind(registry, name, &wl_seat_interface, version);
        } else if (strcmp(interface, zwlr_data_control_manager_v1_interface.name) == 0) {
		data_manager = wl_registry_bind(registry, name, &zwlr_data_control_manager_v1_interface, 2);
		printf("GOT MANAGER\n");
	}
}


static void handle_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
        //nothing here mf
}

static const struct wl_registry_listener registry_listener = {
        .global = handle_global,
        .global_remove = handle_global_remove,
};

#define CLIPBOARD 0
#define PRIMARY 1
static struct zwlr_data_control_offer_v1 *offer = NULL;

static void on_offer_mime(void *data, struct zwlr_data_control_offer_v1 *id, const char *mime_type)
{
	fprintf(stderr, "Got offered MIME type %s\n", mime_type);
}
static struct zwlr_data_control_offer_v1_listener offer_listener = {
	on_offer_mime
};
static void on_data_offer(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *id)
{
	fprintf(stderr, "on_data_offer\n");
	if (offer) {
		zwlr_data_control_offer_v1_destroy(offer);
	}
	offer = id;
	zwlr_data_control_offer_v1_add_listener(offer, &offer_listener, NULL);
}
static void on_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *id)
{
	fprintf(stderr, "on_selection\n");
	if (!id) {
		fprintf(stderr, "something fucky goes on\n");
		return;
	}
	if (id != offer)
		return;
	
	fprintf(stderr, "END OF DATA\n");
}
static void on_finished(void *data, struct zwlr_data_control_device_v1 *device)
{
	fprintf(stderr, "on_finished\n");
}
static int offer_fd[2] = {
	-1,
	-1
};
static void on_primary_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *id)
{
	fprintf(stderr, "on_primary_selection\n");
	if (!id) {
		fprintf(stderr, "something fucky goes on\n");
		return;
	}
	if (id != offer)
		return;
	if (offer_fd[0]) {
		fprintf(stderr, "CLOSING OLD OFFER FD %d\n", offer_fd[0]);
		close(offer_fd[0]);
	}
	if (offer_fd[1]) {
		fprintf(stderr, "CLOSING OLD OFFER FD %d\n", offer_fd[1]);
		close(offer_fd[1]);
	}
	pipe(offer_fd);
	zwlr_data_control_offer_v1_receive(id, "STRING", offer_fd[1]);
	fprintf(stderr, "GOT DATA\n");
}

static struct zwlr_data_control_device_v1_listener data_control_listener = {
	on_data_offer,
	on_primary_selection,
	on_finished,
	on_primary_selection
};	

static struct zwlr_data_control_device_v1 *data_control;
int main(int argc, char **argv)
{
 	display = wl_display_connect(NULL);
	if (!display)
		return -1;
	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registry_listener, NULL);
	wl_display_dispatch(display);
	wl_display_roundtrip(display);
	data_control = zwlr_data_control_manager_v1_get_data_device(data_manager, seat);
	if (!data_control) {
		return 2;
	}
	zwlr_data_control_device_v1_add_listener(data_control, &data_control_listener, NULL);
	while (1) {
		int ret;
		int nfds = 1;
		struct pollfd pfd[2];
		pfd[0].fd = wl_display_get_fd(display);
		pfd[0].events = POLLIN;
		if (offer_fd[0] != -1) {
			pfd[1].fd = offer_fd[0];
			pfd[1].events = POLLIN | POLLHUP;
			nfds = 2;
		}
		if ((ret = poll(pfd, nfds, 1000)) < 0) {
			return -1;
		} else if (ret == 0) {
			wl_display_flush(display);
			continue;
		}
		if (pfd[0].revents & POLLIN) {
			wl_display_dispatch(display);
		}
		if ((nfds > 1) && (pfd[1].revents & POLLIN)) {
			char buf;
			read(pfd[1].fd, &buf, 1);
			putc(buf, stderr);
		}
		if ((nfds > 1) && pfd[1].revents & POLLHUP) {
			fprintf(stderr, "GOT HANGUP ON DATA OFFER\n");
		}
	}
	return 0;
}
