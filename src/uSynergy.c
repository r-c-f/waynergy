/*
uSynergy client -- Implementation for the embedded Synergy client library
Heavily modified from the original version

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
#include "uSynergy.h"
#include <stdio.h>
#include <string.h>
#include "sig.h"
#include "xmem.h"
#include "ssp.h"
#include <stdlib.h>
#include "log.h"
#include <inttypes.h>

//---------------------------------------------------------------------------------------------------------------------
//	Internal helpers
//---------------------------------------------------------------------------------------------------------------------


#define PARSE_ERROR() do { logErr("Parsing Error: %s %s:%d", __func__, __FILE__, __LINE__); return; } while (0)
#define REPLY_ERROR() do { logErr("Error in constructing reply: %s %s:%d", __func__, __FILE__, __LINE__); return; } while (0)



/**
@brief Read 32 bit integer in network byte order and convert to native byte order
**/
static int32_t sNetToNative32(const unsigned char *value)
{
#ifdef USYNERGY_LITTLE_ENDIAN
	return value[3] | (value[2] << 8) | (value[1] << 16) | (value[0] << 24);
#else
	return value[0] | (value[1] << 8) | (value[2] << 16) | (value[3] << 24);
#endif
}




/**
@brief Add string to reply packet
**/
static bool sAddString(uSynergyContext *context, const char *string)
{
	size_t len = strlen(string);
	if (context->m_replyCur - context->m_replyBuffer + len > USYNERGY_REPLY_BUFFER_SIZE)
		return false;
	memcpy(context->m_replyCur, string, len);
	context->m_replyCur += len;
	return true;
}

static bool sAddBin(uSynergyContext *context, const unsigned char *val, size_t len)
{
	if (context->m_replyCur - context->m_replyBuffer + len > USYNERGY_REPLY_BUFFER_SIZE)
		return false;
	memcpy(context->m_replyCur, val, len);
	context->m_replyCur += len;
	return true;
}

/**
@brief Add uint8 to reply packet
**/
static bool sAddUInt8(uSynergyContext *context, uint8_t value)
{
	if (context->m_replyCur - context->m_replyBuffer + sizeof(value) > USYNERGY_REPLY_BUFFER_SIZE)
		return false;
	*context->m_replyCur++ = value;
	return true;
}



/**
@brief Add uint16 to reply packet
**/
static bool sAddUInt16(uSynergyContext *context, uint16_t value)
{
	if (context->m_replyCur - context->m_replyBuffer + sizeof(value) > USYNERGY_REPLY_BUFFER_SIZE)
		return false;
	uint8_t *reply = context->m_replyCur;
	*reply++ = (uint8_t)(value >> 8);
	*reply++ = (uint8_t)value;
	context->m_replyCur = reply;
	return true;
}



/**
@brief Add uint32 to reply packet
**/
static bool sAddUInt32(uSynergyContext *context, uint32_t value)
{
	if (context->m_replyCur - context->m_replyBuffer + sizeof(value) > USYNERGY_REPLY_BUFFER_SIZE)
		return false;
	uint8_t *reply = context->m_replyCur;
	*reply++ = (uint8_t)(value >> 24);
	*reply++ = (uint8_t)(value >> 16);
	*reply++ = (uint8_t)(value >> 8);
	*reply++ = (uint8_t)value;
	context->m_replyCur = reply;
	return true;
}


/**
@brief Mark context as being disconnected
**/
static void sSetDisconnected(uSynergyContext *context, enum uSynergyError err)
{
	context->m_connected		= false;
	context->m_hasReceivedHello = false;
	context->m_isCaptured		= false;
	context->m_replyCur			= context->m_replyBuffer + 4;
	context->m_sequenceNumber	= 0;
	context->m_lastError = err;
}

/**
@brief Send reply packet
**/
static bool sSendReply(uSynergyContext *context)
{
	// Set header size
	uint8_t		*reply_buf	= context->m_replyBuffer;
	uint32_t	reply_len	= (uint32_t)(context->m_replyCur - reply_buf);				/* Total size of reply */
	uint32_t	body_len	= reply_len - 4;											/* Size of body */
	bool ret;
	reply_buf[0] = (uint8_t)(body_len >> 24);
	reply_buf[1] = (uint8_t)(body_len >> 16);
	reply_buf[2] = (uint8_t)(body_len >> 8);
	reply_buf[3] = (uint8_t)body_len;

	// Send reply
	ret = context->m_sendFunc(context->m_cookie, context->m_replyBuffer, reply_len);

	// Reset reply buffer write pointer
	context->m_replyCur = context->m_replyBuffer+4;
	return ret;
}


/* mouse callbacks */
static void sSendMouseWheelCallback(uSynergyContext *context, int16_t x, int16_t y)
{
	if (!context->m_mouseWheelCallback)
		return;
	if (context->m_resChanged || !context->m_infoCurrent)
		return;
	context->m_mouseWheelCallback(context->m_cookie, x, y);
}
static void sSendMouseButtonDownCallback(uSynergyContext *context, enum uSynergyMouseButton button)
{
	if (!context->m_mouseButtonDownCallback)
		return;
	context->m_mouseButtonDownCallback(context->m_cookie, button);
}
static void sSendMouseButtonUpCallback(uSynergyContext *context, enum uSynergyMouseButton button)
{
	if (!context->m_mouseButtonUpCallback)
		return;
	context->m_mouseButtonUpCallback(context->m_cookie, button);
}
static void sSendMouseMoveCallback(uSynergyContext *context, bool rel, int16_t x, int16_t y)
{
	if (!context->m_mouseMoveCallback)
		return;
	context->m_mouseMoveCallback(context->m_cookie, rel, x, y);
}

/**
@brief Send screensaver callback
**/
static void sSendScreensaverCallback(uSynergyContext *context, bool state)
{
	if (!context->m_screensaverCallback)
		return;
	context->m_screensaverCallback(context->m_cookie, state);
}

/**
@brief Send keyboard callback when a key has been pressed or released
**/
static void sSendKeyboardCallback(uSynergyContext *context, uint16_t key, uint16_t modifiers, bool down, bool repeat)
{
	// Skip if no callback is installed
	if (context->m_keyboardCallback == 0L)
		return;

	// Send callback
	context->m_keyboardCallback(context->m_cookie, key, modifiers, down, repeat);
}



/**
@brief Send joystick callback
**/
static void sSendJoystickCallback(uSynergyContext *context, uint8_t joyNum)
{
	int8_t *sticks;

	// Skip if no callback is installed
	if (context->m_joystickCallback == 0L)
		return;

	// Send callback
	sticks = context->m_joystickSticks[joyNum];
	context->m_joystickCallback(context->m_cookie, joyNum, context->m_joystickButtons[joyNum], sticks[0], sticks[1], sticks[2], sticks[3]);
}

/**
@brief Send clipboard data
**/
void uSynergySendClipboard(uSynergyContext *context, int id, uint32_t len, const unsigned char *text)
{
	char buffer[128];
	int pos;
	uint32_t chunk_len;
	// Calculate maximum size that will fit in a reply packet
	uint32_t overhead_size =	4 +					/* Message size */
								4 +					/* Message ID */
								1 +					/* Clipboard index */
								4 +					/* Sequence number */
								4 +					/* Rest of message size (because it's a Synergy string from here on) */
								4 +					/* Number of clipboard formats */
								4 +					/* Clipboard format */
								4;					/* Clipboard data length */
	uint32_t max_length = USYNERGY_REPLY_BUFFER_SIZE - overhead_size;
	unsigned char chunk[max_length];

	// Assemble start packet.
	sprintf(buffer, "%" PRIu32, len);
	if (!(sAddString(context, "DCLP") &&
	      sAddUInt8(context, id) &&				/* Clipboard index */
	      sAddUInt32(context, context->m_sequenceNumber) &&
	      sAddUInt8(context, SYN_DATA_START) &&
	      sAddUInt32(context, strlen(buffer)) && 			/* Rest of message size: mark, string size of message */
	      sAddString(context, buffer))) {
		REPLY_ERROR();
	}
	sSendReply(context);
	// Now we do the chunks.
	for (pos = 0; pos < len; pos += chunk_len) {
		chunk_len = ((len - pos) > max_length) ? max_length : len - pos;
		memmove(chunk, text + pos, chunk_len);
		if (!(sAddString(context, "DCLP") &&
		      sAddUInt8(context, id) &&
		      sAddUInt32(context, context->m_sequenceNumber) &&
		      sAddUInt8(context, SYN_DATA_CHUNK) &&
		      sAddUInt32(context, chunk_len) &&
		      sAddBin(context, chunk, chunk_len))) {
			REPLY_ERROR();
		}
		sSendReply(context);
	}
	//And then we're done
	if (!(sAddString(context, "DCLP") &&
	      sAddUInt8(context, id) &&
	      sAddUInt32(context, context->m_sequenceNumber) &&
	      sAddUInt8(context, SYN_DATA_END) &&
	      sAddUInt32(context, 0))) {
		REPLY_ERROR();
	}
	sSendReply(context);
}


/**
@brief Check if the given message contains a valid welcome message, to allow for
barrier compatibility
**/
static char *sImplementations[] = {
	"Barrier",
	"Synergy",
	NULL
};
static char *sIsWelcome(struct sspBuf *msg)
{
	char **i;
	for (i = sImplementations; *i; ++i) {
		if (strlen(*i) > msg->len)
			continue;
		if (memcmp(msg->data, *i, strlen(*i)) == 0) {
			sspSeek(msg, strlen(*i));
			return *i;
		}
	}
	return NULL;
}


/**
@brief Parse a single client message, update state, send callbacks and send replies
**/
static void sProcessMessage(uSynergyContext *context, struct sspBuf *msg)
{
	// We have a packet!
	const char *imp;
	char pkt_id[5] = {0};
	if ((imp = sIsWelcome(msg)))
	{
		// Welcome message
		//		kMsgHello			= "Synergy%2i%2i"
		//		kMsgHelloBack		= "Synergy%2i%2i%s"
		uint16_t server_major, server_minor;
		if (!(sspNetU16(msg, &server_major) && sspNetU16(msg, &server_minor))) {
			PARSE_ERROR();
		}
		logInfo("Server is %s %" PRIu16 ".%" PRIu16, imp, server_major, server_minor);
		if(!(sAddString(context, imp) &&
		      sAddUInt16(context, USYNERGY_PROTOCOL_MAJOR) &&
		      sAddUInt16(context, USYNERGY_PROTOCOL_MINOR) &&
		      sAddUInt32(context, (uint32_t)strlen(context->m_clientName)) &&
		      sAddString(context, context->m_clientName))) {
			REPLY_ERROR();
		}
		if (!sSendReply(context))
		{
			// Send reply failed, let's try to reconnect
			logErr("SendReply failed, trying to reconnect in a second");
			context->m_connected = false;
			context->m_sleepFunc(context->m_cookie, 1000);
		}
		else
		{
			// Let's assume we're connected
			logInfo("Connected as client \"%s\"", context->m_clientName);
			context->m_hasReceivedHello = true;
			context->m_implementation = imp;
		}
		return;
	}
	if (!sspMemMove(pkt_id, msg, 4)) {
		PARSE_ERROR();
	}
	else if (!strcmp(pkt_id, "QINF"))
	{
		// Screen info. Reply with DINF
		//		kMsgQInfo			= "QINF"
		//		kMsgDInfo			= "DINF%2i%2i%2i%2i%2i%2i%2i"
		uint16_t x = 0, y = 0, warp = 0;
		if (!(sAddString(context, "DINF") &&
		      sAddUInt16(context, x) &&
		      sAddUInt16(context, y) &&
		      sAddUInt16(context, context->m_clientWidth) &&
		      sAddUInt16(context, context->m_clientHeight) &&
		      sAddUInt16(context, warp) &&
		      sAddUInt16(context, 0) &&			// mx?
		      sAddUInt16(context, 0))) {		// my?
			REPLY_ERROR();
		}
		sSendReply(context);
		context->m_infoCurrent = false;
		return;
	}
	else if (!strcmp(pkt_id, "CIAK"))
	{
		//		kMsgCInfoAck		= "CIAK"
		context->m_infoCurrent = true;
		return;
	}
	else if (!strcmp(pkt_id, "CROP"))
	{
		// Do nothing?
		//		kMsgCResetOptions	= "CROP"
		return;
	}
	else if (!strcmp(pkt_id, "CINN"))
	{
		// Screen enter. Reply with CNOP
		//		kMsgCEnter 			= "CINN%2i%2i%4i%2i"
		// Obtain the Synergy sequence number
		if (!(sspNet16(msg, NULL) &&
		      sspNet16(msg, NULL) &&
		      sspNetU32(msg, &context->m_sequenceNumber))) {
			PARSE_ERROR();
		}
		context->m_isCaptured = true;

		// Call callback
		if (context->m_screenActiveCallback != 0L)
			context->m_screenActiveCallback(context->m_cookie, true);
	}
	else if (!strcmp(pkt_id, "COUT"))
	{
		// Screen leave
		//		kMsgCLeave 			= "COUT"
		context->m_isCaptured = false;

		// Send clipboard data
		for (int id = 0; id < 2; ++id) {
			if (context->m_clipGrabbed[id]) {
				uSynergySendClipboard(context, id, context->m_clipPos[id], context->m_clipBuf[id]);
				context->m_clipGrabbed[id] = false;
			}
		}

		// Call callback
		if (context->m_screenActiveCallback != 0L)
			context->m_screenActiveCallback(context->m_cookie, false);
	}
	else if (!strcmp(pkt_id, "CSEC"))
	{
		//Screensaver state
		char active;
		if (!sspChar(msg, &active)) {
			PARSE_ERROR();
		}
		sSendScreensaverCallback(context, active);
	}
	else if (!strcmp(pkt_id, "DMDN"))
	{
		// Mouse down
		//		kMsgDMouseDown		= "DMDN%1i"
		char btn;
		if (!sspChar(msg, &btn)) {
			PARSE_ERROR();
		}
		sSendMouseButtonDownCallback(context, btn);
	}
	else if (!strcmp(pkt_id, "DMUP"))
	{
		// Mouse up
		//		kMsgDMouseUp		= "DMUP%1i"
		char btn;
		if (!sspChar(msg, &btn)) {
			PARSE_ERROR();
		}
		sSendMouseButtonUpCallback(context, btn);
	}
	else if (!strcmp(pkt_id, "DMMV"))
	{
		// Mouse move. Reply with CNOP
		//		kMsgDMouseMove		= "DMMV%2i%2i"
		int16_t x, y;
		if (!(sspNet16(msg, &x) && sspNet16(msg, &y))) {
			PARSE_ERROR();
		}
		sSendMouseMoveCallback(context, false, x, y);
	}
	else if (!strcmp(pkt_id, "DMRM"))
	{
		//Relative mouse move.
		int16_t x, y;
		if (!(sspNet16(msg, &x) && sspNet16(msg, &y))) {
			PARSE_ERROR();
		}
		sSendMouseMoveCallback(context, true, x, y);
	}
	else if (!strcmp(pkt_id, "DMWM"))
	{
		// Mouse wheel
		//		kMsgDMouseWheel		= "DMWM%2i%2i"
		//		kMsgDMouseWheel1_0	= "DMWM%2i"
		int16_t x, y;
		if (!(sspNet16(msg, &x) && sspNet16(msg, &y))) {
			PARSE_ERROR();
		}
		sSendMouseWheelCallback(context, x, y);
	}
	else if (!strcmp(pkt_id, "DKDN"))
	{
		// Key down
		//		kMsgDKeyDown		= "DKDN%2i%2i%2i"
		//		kMsgDKeyDown1_0		= "DKDN%2i%2i"
		uint16_t id, mod, key;
		if (!(sspNetU16(msg, &id) && sspNetU16(msg, &mod) && sspNetU16(msg, &key))) {
			PARSE_ERROR();
		}
		sSendKeyboardCallback(context, context->m_useRawKeyCodes ? key : id, mod, true, false);
	}
	else if (!strcmp(pkt_id, "DKRP"))
	{
		// Key repeat
		//		kMsgDKeyRepeat		= "DKRP%2i%2i%2i%2i"
		//		kMsgDKeyRepeat1_0	= "DKRP%2i%2i%2i"
		uint16_t id, mod, count, key;
		if (!(sspNetU16(msg, &id) &&
		      sspNetU16(msg, &mod) &&
		      sspNetU16(msg, &count) &&
		      sspNetU16(msg, &key))) {
			PARSE_ERROR();
		}
		sSendKeyboardCallback(context, context->m_useRawKeyCodes ? key : id, mod, true, true);
	}
	else if (!strcmp(pkt_id, "DKUP"))
	{
		// Key up
		//		kMsgDKeyUp			= "DKUP%2i%2i%2i"
		//		kMsgDKeyUp1_0		= "DKUP%2i%2i"
		uint16_t id, mod, key;
		if (!(sspNetU16(msg, &id) && sspNetU16(msg, &mod) && sspNetU16(msg, &key))) {
			PARSE_ERROR();
		}
		sSendKeyboardCallback(context, context->m_useRawKeyCodes ? key : id, mod, false, false);
	}
	else if (!strcmp(pkt_id, "DGBT"))
	{
		// Joystick buttons
		//		kMsgDGameButtons	= "DGBT%1i%2i";
		uint8_t	joy_num;
		uint16_t state;
		if (!(sspUChar(msg, &joy_num) && sspNetU16(msg, &state))) {
			PARSE_ERROR();
		}
		if (joy_num<USYNERGY_NUM_JOYSTICKS)
		{
			context->m_joystickButtons[joy_num] = state;
			sSendJoystickCallback(context, joy_num);
		}
	}
	else if (!strcmp(pkt_id, "DGST"))
	{
		// Joystick sticks
		//		kMsgDGameSticks		= "DGST%1i%1i%1i%1i%1i";
		uint8_t	joy_num;
		int8_t state[4];
		if (!(sspUChar(msg, &joy_num) && sspMemMove(state, msg, sizeof(state)))) {
			PARSE_ERROR();
		}
		if (joy_num<USYNERGY_NUM_JOYSTICKS)
		{
			// Copy stick state, then send callback
			memcpy(context->m_joystickSticks[joy_num], state, sizeof(state));
			sSendJoystickCallback(context, joy_num);
		}
	}
	else if (!strcmp(pkt_id, "DSOP"))
	{
		// Set options
		//		kMsgDSetOptions		= "DSOP%4I"
	}
	else if (!strcmp(pkt_id, "CALV"))
	{
		// Keepalive, reply with CALV and then CNOP
		//		kMsgCKeepAlive		= "CALV"
		logDbg("Got CALV");
		if (!sAddString(context, "CALV")) {
			REPLY_ERROR();
		}
		sSendReply(context);
		// now reply with CNOP
	}
	else if (!strcmp(pkt_id, "CCLP"))
	{
		// Clipboard grab
		// CCLP%1i%4i
		//
		// 1 uint32: size
		// 4 char: identifier ("CCLP")
		// 1 uint8_t: clipboard ID
		// 1 uint32_t: sequence number
		unsigned char id;
		uint32_t seq;
		if (!(sspUChar(msg, &id) && sspNetU32(msg, &seq))) {
			PARSE_ERROR();
		}
		/* XXX: I think the sequence number is always zero on receive?*/
		context->m_clipGrabbed[id] = false;
	}
	else if (!strcmp(pkt_id, "DCLP"))
	{
		// Clipboard message
		//		kMsgDClipboard		= "DCLP%1i%4i%s"
		//
		// The clipboard message contains:
		//		1 uint32:	The size of the message
		//		4 chars: 	The identifier ("DCLP")
		//		1 uint8: 	The clipboard index
		//		1 uint32:	The sequence number. It's zero, because this message is always coming from the server?
		//		1 uint32:	The total size of the remaining 'string' (as per the Synergy %s string format (which is 1 uint32 for size followed by a char buffer (not necessarily null terminated)).
		//		1 uint32:	The number of formats present in the message
		// And then 'number of formats' times the following:
		//		1 uint32:	The format of the clipboard data
		//		1 uint32:	The size n of the clipboard data
		//		n uint8:	The clipboard data
		unsigned char id, mark;
		uint32_t seq, len;
		if (!(sspUChar(msg, &id) &&
		      sspNetU32(msg, &seq) &&
		      sspUChar(msg, &mark) &&
		      sspNetU32(msg, &len))) {
			PARSE_ERROR();
		}
		if (mark ==  SYN_DATA_START) {
			context->m_clipGrabbed[id] = false;
			context->m_clipInStream[id] = true;
			context->m_clipPos[id] = 0;
			char expected_len[len + 1];
			sspMemMove(expected_len, msg, len);
			expected_len[len] = '\0';
			context->m_clipPosExpect[id] = atoi(expected_len);
			if (context->m_clipPosExpect[id] > context->m_clipLen[id]) {
				context->m_clipBuf[id] = xrealloc(context->m_clipBuf[id], context->m_clipPosExpect[id]);
			}
		} else if (mark == SYN_DATA_CHUNK && context->m_clipInStream[id]) {
			if ((context->m_clipPos[id] + len) > context->m_clipPosExpect[id]) {
				logErr("Packet too long!");
				return;
			}
			sspMemMove(context->m_clipBuf[id] + context->m_clipPos[id], msg, len);
			context->m_clipPos[id] += len;
		} else if (mark ==  SYN_DATA_END && context->m_clipInStream[id]) {
			struct sspBuf clipmsg = {
				.data = context->m_clipBuf[id],
				.pos = 0,
				.len = context->m_clipPosExpect[id]
			};
			uint32_t num_formats, format, size;
			if (!sspNetU32(&clipmsg, &num_formats)) {
				PARSE_ERROR();
			}
			for (; num_formats; num_formats--)
			{
				// Parse clipboard format header
				if (!(sspNetU32(&clipmsg, &format) &&
				      sspNetU32(&clipmsg, &size))) {
					PARSE_ERROR();
				}

				// Call callback
				if (context->m_clipboardCallback) {
					//First check size against buffer
					if (clipmsg.pos + size > clipmsg.len) {
						PARSE_ERROR();
					}
					context->m_clipboardCallback(context->m_cookie, id, format, clipmsg.data + clipmsg.pos, size);
				}
				if (!sspSeek(&clipmsg, size)) {
					PARSE_ERROR();
				}
			}
			context->m_clipInStream[id] = false;
		}
	}
	else if (!strcmp(pkt_id, "CBYE")) {
		logInfo("Server disconnected");
		sSetDisconnected(context, USYNERGY_ERROR_NONE);
		return;
	}
	else if (!strcmp(pkt_id, "EBAD")) {
		logErr("Protocol error");
		sSetDisconnected(context, USYNERGY_ERROR_EBAD);
		return;
	}
	else if (!strcmp(pkt_id, "EBSY")) {
		logErr("Other screen already connected with our name");
		sSetDisconnected(context, USYNERGY_ERROR_EBSY);
		return;
	}
	else
	{
		// Unknown packet, could be any of these
		//		kMsgCNoop 			= "CNOP"
		//		kMsgCClose 			= "CBYE"
		//		kMsgCClipboard 		= "CCLP%1i%4i"
		//		kMsgCScreenSaver 	= "CSEC%1i"
		//		kMsgDKeyRepeat		= "DKRP%2i%2i%2i%2i"
		//		kMsgDKeyRepeat1_0	= "DKRP%2i%2i%2i"
		//		kMsgDMouseRelMove	= "DMRM%2i%2i"
		//		kMsgEIncompatible	= "EICV%2i%2i"
		//		kMsgEBusy 			= "EBSY"
		//		kMsgEUnknown		= "EUNK"
		//		kMsgEBad			= "EBAD"
		logWarn("Unknown packet '%s'", pkt_id);
		return;
	}
	// Reply with CNOP maybe?
	if(!sAddString(context, "CNOP")) {
		REPLY_ERROR();
	}
	sSendReply(context);
}
#undef USYNERGY_IS_PACKET






/**
@brief Update a connected context
**/
static void sUpdateContext(uSynergyContext *context)
{
	/* Receive data (blocking) */
	int receive_size = USYNERGY_RECEIVE_BUFFER_SIZE - context->m_receiveOfs;
	int num_received = 0;
	uint32_t packlen = 0;
	if (context->m_receiveFunc(context->m_cookie, context->m_receiveBuffer + context->m_receiveOfs, receive_size, &num_received) == false)
	{
		/* Receive failed, let's try to reconnect */
		logErr("Receive failed (%d bytes asked, %d bytes received), trying to reconnect in a second", receive_size, num_received);
		/* The *only* way this can occur normally is with a timeout so that's what we assume*/
		sSetDisconnected(context, USYNERGY_ERROR_TIMEOUT);
		context->m_sleepFunc(context->m_cookie, 1000);
		return;
	}
	context->m_receiveOfs += num_received;

	/*	If we didn't receive any data then we're probably still polling to get connected and
		therefore not getting any data back. To avoid overloading the system with a Synergy
		thread that would hammer on polling, we let it rest for a bit if there's no data. */
	if (num_received == 0)
		context->m_sleepFunc(context->m_cookie, 500);

	/* Check for timeouts */
	if (context->m_hasReceivedHello)
	{
		uint32_t cur_time = context->m_getTimeFunc();
		if (num_received == 0)
		{
			/* Timeout after 2 secs of inactivity (we received no CALV) */
			if ((cur_time - context->m_lastMessageTime) > USYNERGY_IDLE_TIMEOUT)
				sSetDisconnected(context, USYNERGY_ERROR_TIMEOUT);
		}
		else
			context->m_lastMessageTime = cur_time;
	}

	/* Eat packets */
	for (;;)
	{
		/* Grab packet length and bail out if the packet goes beyond the end of the buffer */
		packlen = sNetToNative32(context->m_receiveBuffer);
		if (packlen+4 > context->m_receiveOfs)
			break;

		/* Process message */
		struct sspBuf msg = {
			.data = context->m_receiveBuffer + 4,
			.pos = 0,
			.len = packlen
		};
		sProcessMessage(context, &msg);

		/* Move packet to front of buffer */
		memmove(context->m_receiveBuffer, context->m_receiveBuffer+packlen+4, context->m_receiveOfs-packlen-4);
		context->m_receiveOfs -= packlen+4;
	}

	/* Throw away over-sized packets */
	if (packlen > USYNERGY_RECEIVE_BUFFER_SIZE)
	{
		/* Oversized packet, ditch tail end */
		logWarn("Oversized packet: '%c%c%c%c' (length %d)", context->m_receiveBuffer[4], context->m_receiveBuffer[5], context->m_receiveBuffer[6], context->m_receiveBuffer[7], packlen);
		num_received = context->m_receiveOfs-4; // 4 bytes for the size field
		while (num_received != packlen)
		{
			int buffer_left = packlen - num_received;
			int to_receive = buffer_left < USYNERGY_RECEIVE_BUFFER_SIZE ? buffer_left : USYNERGY_RECEIVE_BUFFER_SIZE;
			int ditch_received = 0;
			if (context->m_receiveFunc(context->m_cookie, context->m_receiveBuffer, to_receive, &ditch_received) == false)
			{
				/* Receive failed, let's try to reconnect */
				logErr("Receive failed, trying to reconnect in a second");
				/* this only happens with timeout*/
				sSetDisconnected(context, USYNERGY_ERROR_TIMEOUT);
				context->m_sleepFunc(context->m_cookie, 1000);
				break;
			}
			else
			{
				num_received += ditch_received;
			}
		}
		context->m_receiveOfs = 0;
	}
}


//---------------------------------------------------------------------------------------------------------------------
//	Public interface
//---------------------------------------------------------------------------------------------------------------------



/**
@brief Initialize uSynergy context
**/
void uSynergyInit(uSynergyContext *context)
{
	/* Zero memory */
	memset(context, 0, sizeof(uSynergyContext));

	/* Initialize to default state */
	sSetDisconnected(context, USYNERGY_ERROR__INIT);
}


/**
@brief Update uSynergy
**/
void uSynergyUpdate(uSynergyContext *context)
{
	if (context->m_connected)
	{
		/* Update context, receive data, call callbacks */
		sUpdateContext(context);
	}
	else
	{
		/* Try to connect */
		if (context->m_lastError > 0) {
			if (context->m_errorIsFatal[context->m_lastError]) {
				logErr("Last error received (code %d) is configured as fatal, exiting", context->m_lastError);
				Exit(context->m_lastError);
			}
		}
		if (context->m_connectFunc(context->m_cookie)) {
			context->m_connected = true;
		} else {
			logErr("Connection attempt failed, trying to reconnect in a second");
			context->m_sleepFunc(context->m_cookie, 1000);
		}
	}
}




/* check all formats in the clipboard message buffer */
static bool uSynergyClipBufContains(uSynergyContext *context, enum uSynergyClipboardId id, uint32_t len, const char *data)
{
	uint32_t formats, flen;
	struct sspBuf buf = {
		.data = context->m_clipBuf[id],
		.pos = 0,
		.len = context->m_clipLen[id]
	};
	if (!buf.data)
		return false;
	if (context->m_clipInStream[id])
		return false;
	if (!sspNetU32(&buf, &formats)) {
		logErr("Clipboard parse error: %d, %d", buf.pos, buf.len);
		return false;
	}
	for (int i = 0; i < formats; ++i) {
		//skip the format, only check the raw data
		if (!(sspNetU32(&buf, NULL) && sspNetU32(&buf, &flen))) {
			logErr("Clipboard format parse error");
			return false;
		}
		if (flen == len) {
			if (!memcmp(data, buf.data + buf.pos, len)) {
				return true;
			}
		}
	}
	return false;
}

/* generic functions to add values to raw buffer */
static uint8_t *buf_add_int32(uint8_t *buf, uint32_t val)
{
	buf[0] = (val >> 24) & 0xFF;
	buf[1] = (val >> 16) & 0xFF;
	buf[2] = (val >> 8) & 0xFF;
	buf[3] = val & 0xFF;
	return buf + 4;
}
/* Update clipboard buffer from local clipboard */
void uSynergyUpdateClipBuf(uSynergyContext *context, enum uSynergyClipboardId id, uint32_t len, const char *data)
{
	/* to prevent feedback loops, check to make sure the data is actually
	 * different from what we've already got */
	if (uSynergyClipBufContains(context, id, len, data))
		return;
	/* grab the clipboard, initialize the buffer */
	context->m_clipInStream[id] = false;
	context->m_clipGrabbed[id] = true;
	context->m_clipPos[id] = len + 4 + 4 + 4; //format count, format ID, size, data
	if (context->m_clipLen[id] < context->m_clipPos[id]) {
		context->m_clipLen[id] = context->m_clipPos[id];
		context->m_clipBuf[id] = xrealloc(context->m_clipBuf[id], context->m_clipLen[id]);
	}
	/*populate buffer*/
	uint8_t *buf = context->m_clipBuf[id];
	buf = buf_add_int32(buf, 1); //formats
	buf = buf_add_int32(buf, USYNERGY_CLIPBOARD_FORMAT_TEXT); //type, text only for now
	buf = buf_add_int32(buf, len); //length of actual data
	memmove(buf, data, len);
	/* send CCLP  -- CCLP%1i%4i */
	if (!(sAddString(context, "CCLP") &&
	      sAddUInt8(context, id) &&
	      sAddUInt32(context, context->m_sequenceNumber))) {
		REPLY_ERROR();
	}
	sSendReply(context);
}

/* Update resolution */
void uSynergyUpdateRes(uSynergyContext *context, int16_t width, int16_t height)
{
	context->m_clientWidth = width;
	context->m_clientHeight = height;
	if (context->m_connected) {
		logDbg("Sending DINF to update screen resolution");
		/* send information update */
		uint16_t x = 0, y = 0, warp = 0;
		sAddString(context, "DINF");
		sAddUInt16(context, x);
		sAddUInt16(context, y);
		sAddUInt16(context, context->m_clientWidth);
		sAddUInt16(context, context->m_clientHeight);
		sAddUInt16(context, warp);
		sAddUInt16(context, 0);         // mx?
		sAddUInt16(context, 0);         // my?
		sSendReply(context);
		context->m_infoCurrent = false;
	}
}

