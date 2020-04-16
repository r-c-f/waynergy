#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wayland-client.h>
#include "wlr-data-control-unstable-v1.prot.h"



static struct wl_seat *seat;
static struct wl_registry *registry;

static struct zwlr_data_control_manager_v1 *dc_manager;
static struct zwlr_data_control_device_v1 *dc_device;
static struct zwlr_data_control_offer_v1 *dc_offer;
#define MIME_MAX 256
char *offer_mime[MIME_MAX];
size_t offer_mime_pos;




static void on_offer(void *data, struct zwlr_data_control_offer_v1 *offer, const char *mime_type)
{
	if (offer != dc_offer) {
		fprintf(stderr, "ERROR: Wrong offer!\n");
		return;
	}
	offer_mime[offer_mime_pos++] = strdup(mime_type);
	fprintf(stderr, "GOT MIME TYPE %s\n", mime_type);
}

static void clear_offer(void)
{
	if (dc_offer)
		zwlr_data_control_offer_v1_destroy(dc_offer);
	dc_offer = NULL;
	for (size_t i = 0; i < offer_mime_pos; ++i) {
		free(offer_mime[i]);
		offer_mime[i] = NULL;
	}
	offer_mime_pos = 0;
}

static struct zwlr_data_control_offer_v1_listener offer_listener = {
	.offer = on_offer
};

static void on_data_offer(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer)
{
	clear_offer();
	dc_offer = offer;
	fprintf(stderr, "Got new offer\n");
	zwlr_data_control_offer_v1_add_listener(offer, &offer_listener, NULL);
}

enum clipboard_id {
	CLIPBOARD = 0,
	PRIMARY
};
static void on_selection(void *data, struct zwlr_data_control_device_v1 *device, struct zwlr_data_control_offer_v1 *offer, enum clipboard_id id)
{
	char *id_str = id ? "primary" : "clipboard"; 
	if (offer != dc_offer) {
		fprintf(stderr, "ERROR: Got wrong %s offer ID!", id_str);
		return;
	}
	fprintf(stderr, "Selected (id %s) types:\n", id_str);
	for (size_t i = 0; i < offer_mime_pos; ++i) {
		fprintf(stderr, "\t%s\n", offer_mime[i]);
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
