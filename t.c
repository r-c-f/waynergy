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
#include "xmem.h"

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
	int fd[2];
	pipe(fd);
	zwlr_data_control_offer_v1_receive(id, "text/plain", fd[1]);
	if (!fork()) {
		close(fd[1]);
		dup2(fd[0], STDIN_FILENO);
		execlp("cat", "cat", NULL);
	}
	close(fd[0]);
	close(fd[1]);
}

static struct zwlr_data_control_device_v1_listener data_control_listener = {
	on_data_offer,
	on_primary_selection,
	on_finished,
	on_primary_selection
};	

static struct zwlr_data_control_source_v1 *data_source;
static char *data_source_buf;
static size_t data_source_len;
static void on_send(void *data, struct zwlr_data_control_source_v1 *source, const char *mime_type, int32_t fd)
{
	if (source != data_source) {
		fprintf(stderr, "UNKNOWN SOURCE");
	}
	fprintf(stderr, "SENDING MIME TYPE %s", mime_type);
	if (!write_full(fd, data_source_buf, data_source_len)) {
		fprintf(stderr, "COULD NOT SEND");
	}
	write_full(STDERR_FILENO, data_source_buf, data_source_len);
	close(fd);
}
static void on_cancelled(void *data, struct zwlr_data_control_source_v1 *source)
{
	if (source == data_source) {
		free(data_source_buf);
		data_source_buf = NULL;
		data_source_len = 0;
		zwlr_data_control_source_v1_destroy(data_source);
		data_source = NULL;
	}
}
static struct zwlr_data_control_source_v1_listener source_listener = {
	.send = on_send,
	.cancelled = on_cancelled
};

static struct zwlr_data_control_device_v1 *data_control;
static void wl_clip(char *data, size_t len)
{
	if (data_source) {
		on_cancelled(NULL, data_source);
	}
	data_source = zwlr_data_control_manager_v1_create_data_source(data_manager);
	data_source_buf = xmalloc(len);
	data_source_len = len;
	memmove(data_source_buf, data, len);
	zwlr_data_control_source_v1_add_listener(data_source, &source_listener, NULL);
	zwlr_data_control_source_v1_offer(data_source, "text/plain");
	zwlr_data_control_source_v1_offer(data_source, "text/plain;charset=utf-8");
	zwlr_data_control_source_v1_offer(data_source, "TEXT");
	zwlr_data_control_source_v1_offer(data_source, "STRING");
	zwlr_data_control_source_v1_offer(data_source, "UTF8_STRING");
	zwlr_data_control_device_v1_set_selection(data_control, data_source);
	wl_display_roundtrip(display);
}
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
	wl_display_roundtrip(display);
	wl_clip(argv[1], strlen(argv[1]));
	wl_display_roundtrip(display);
	while (1) {
		wl_display_dispatch(display);
	}
	return 0;
}
