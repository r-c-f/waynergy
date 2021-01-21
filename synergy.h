#pragma once

/*
 * synergy client stuff
 *
 * Based heavily on uSynergy, which was released under the following terms:
	Copyright (C) 2012-2016 Symless Ltd.
	Copyright (c) 2012 Alex Evans

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.

	3. This notice may not be removed or altered from any source
	distribution.
*/

#include <stdio.h>
#include <stdin.h>
#include <stdbool.h>
#include "log.h"


enum synClipboardFormat {
	SYN_CLIPBOARD_FORMAT_TEXT = 0,
	SYN_CLIPBOARD_FORMAT_BITMAP = 1,
	SYN_CLIPBOARD_FORMAT_HTML =2,
};

#define SYNERGY_NUM_JOYSTICKS 		4
#define SYNERGY_PROTOCOL_MAJOR 		1
#define SYNERGY_PROTOCOL_MINOR 		6
#define SYNERGY_IDLE_TIMEOUT 		10000
#define SYNERGY_REPLY_BUFFER_SIZE 	1024
#define SYNERGY_RECEIVE_BUFFER_SIZE 	0xFFFF

enum synKeyboardMod {
	SYNERGY_MODIFIER_SHIFT = 	0x0001,
	SYNERGY_MODIFIER_CTRL = 	0x0002,
	SYNERGY_MODIFIER_ALT = 		0x0004,
	SYNERGY_MODIFIER_META = 	0x0008,
	SYNERGY_MODIFIER_WIN = 		0x0010,
	SYNERGY_MODIFIER_ALT_GR = 	0x0020,
	SYNERGY_MODIFIER_LEVEL5LOCK = 	0x0040,
	SYNERGY_MODIFIER_CAPSLOCK = 	0x1000,
	SYNERGY_MODIFIER_NUMLOCK = 	0x2000,
	SYNERGY_MODIFIER_SCROLLOCK = 	0x4000,
};

enum synMouseButton {
	SYNERGY_MOUSE_BUTTON_NONE = 0,
	SYNERGY_MOUSE_BUTTON_LEFT,
	SYNERGY_MOUSE_BUTTON_MIDDLE,
	SYNERGY_MOUSE_BUTTON_RIGHT,
};

enum synError {
	SYNERGY_ERROR__INIT = -1,
	SYNERGY_ERROR_NONE = 0,
	SYNERGY_ERROR_EBSY,
	SYNERGY_ERROR_EBAD,
	SYNERGY_ERROR_TIMEOUT,
	SYNERGY_ERROR__COUNT,
};

enum synClipboardID {
	SYNERGY_CLIPBOARD_CLIPBOARD = 0,
	SYNERGY_CLIPBOARD_SELECTION = 1,
};

struct synContext {
	/* required */
	bool (*connect)(struct synContext *ctx);
	bool (*send)(struct synContext *ctx, const char *buf, size_t len);
	bool (*recv)(struct synContext *ctx, char *buf, size_t max_len, size_t *out_len);
	void (*sleep)(struct synContext *ctx, int ms);
	uint32_t (*get_time)(void);
	/* optional*/
	bool error_is_fatal[SYNERGY_ERROR__COUNT];
	void (*on_screen_active)(struct synContext *ctx, bool active);
	void (*on_screensaver)(struct synContext *ctx, bool state);
	void (*on_mouse_wheel)(struct synContext *ctx, int16_t x, int16_t y);
	void (*on_mouse_button)(struct synContext *ctx,  enum synMouseButton *button, bool state);
	void (*on_mouse_move)(struct synContext *ctx, bool rel, int16_t x, int16_t y);
	void (*on_key)(struct synContext *ctx, uint16_t key, uint16_t mod, bool down, bool repeat);
	void (*on_clip)(struct synContext *ctx, enum synClipboardID id, enum synClipboardFormat fmt, const char *data, uint32_t size);
	/* internal state data */
	enum synError last_error;
	const char *implementation;
	bool is_connected;
	bool has_receieved_hello;
	bool info_current;
	bool res_changed;
	bool is_captured;
	uint32_t last_msg_time;
	uint32_t seq_number;
	char recv_buf[SYNERGY_RECEIVE_BUFFER_SIZE];
	size_t recv_offset;
	char reply_buf[SYNERGY_REPLY_BUFFER_SIZE];
	char *reply_cur;
	char *clip_buf[2];
	size_t clip_len[2];
	size_t clip_pos[2];
	size_t clip_pos_expect[2];
	bool clip_in_stream[2];
	bool clip_grabbed[2];
};

extern void synInit(struct synContext *ctx);
extern void synUpdate(struct synContext *ctx);
extern void synUpdateClipBuf(struct synContext *ctx, enum synClipboardID id, uint32_t len, const char *data);
extern void synUpdateRes(struct synContext *ctx, int16_t width, int16_t height);


