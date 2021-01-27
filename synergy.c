#include "synergy.h"


static void set_disconnected(synContext *ctx, enum synError err)
{
	ctx->connected = false;
	ctx->has_received_hello = false;
	ctx->is_captured = false;
	ctx->seq_number = 0;
	ctx->last_error = err;
}
static bool send_reply(synContext *ctx)
{
	unsigned char body_len_b[4];
	body_len_b[0] = (unsigned char)(ctx->reply_buf.pos >> 24);
	body_len_b[1] = (unsigned char)(ctx->reply_buf.pos >> 16);
	body_len_b[2] = (unsigned char)(ctx->reply_buf.pos >> 8);
	body_len_b[3] = (unsigned char)(ctx->reply_buf.pos);
	//Send the length
	if !(ctx->send(ctx, body_len_b, 4)) {
		return false;
	}
	//Send the real reply
	if (!ctx->send(ctx, ctx->reply_buf.data, ctx->reply_buf.pos)) {
		return false;
	}
	//Reset buffer
	ctx->reply_buf.pos = 0;
	return true;
}


void synSendClipboard(struct synContext *ctx, unsigned char id, uint32_t len, const char *text)
{
	char buffer[128];
	int pos;
	uint32_t chunk_len;
	uint32_t overhead_size =        4 +                                     /* Message size */
                                                                4 +                                     /* Message ID */
                                                                1 +                                     /* Clipboard index */
                                                                4 +                                     /* Sequence number */
                                                                4 +                                     /* Rest of message size (because it's a Synergy string from here on) */
                                                                4 +                                     /* Number of clipboard formats */
                                                                4 +                                     /* Clipboard format */
                                                                4;                                      /* Clipboard data length */
	uint32_t max_len = SYNERGY_REPLY_BUFFER_SIZE - overhead_size;
	char chunk[max_length];
	struct sspBuf *buf = &ctx->reply_buf;

	//Assemble start packet
	sprintf(buffer, "%d", len);
	sspAddString(buf, "DCLP");
	sspAddNetU8(buf, id);
	sspAddNetU32(buf, ctx->seq_number);
	sspAddNetU8(buf, SYN_DATA_START);
	sspAddNetU32(buf, strlen(buffer));
	sspAddString(buf, buffer);
	send_reply(ctx);

	//Now do chunks
	for (pos = 0; pos < len; pos += chunk_len) {
		chunk_len = ((len - pos) > max_length) ? max_length : len - pos;
		memmove(chunk, text + pos, chunk_len);
		sspAddString(buf, "DCLP");
		sspAddNetU8(buf, id);
		sspAddNetU32(buf, ctx->seq_number);
		sspAddNetU8(buf, SYN_DATA_CHUNK);
		sspAddNetU32(buf, chunk_len);
		sspAddBin(buf, chunk, chunk_len);
		send_reply(ctx);
	}
	// And done.
	sspAddString(buf, "DCLP");
	sspAddNetU8(buf, id);
	sspAddNetU32(buf, ctx->seq_number);
	sspAddNetU8(buf, SYN_DATA_END);
	sspAddNetU32(buf, 0);
	send_reply(ctx);
}

static char *implementations[] = {
	"Barrier",
	"Synergy",
	NULL
};
static char *is_welcome(struct sspBuf *msg)
{
	char **i;
	for (i = implementations; *i; ++i) {
		if (strlen(*i) > msg->len)
			continue;
		if (memcmp(msg->data, *i, strlen(*i)) == 0) {
			sspSeek(msg, strlen(*i));
			return *i;
		}
	}
	return NULL;
}
#define PARSE_ERROR() do { logErr("Parsing Error: %s %s:%d", __func__, __FILE__, __LINE__); return; } while (0)
static void process_message(struct synContext *ctx, struct sspBuf *msg)
{
        const char *imp;
        char pkt_id[5] = {0};
	struct sspBuf *reply = ctx->reply_buf;
        if ((imp = is_welcome(msg)))
        {
                // Welcome message
                //              kMsgHello                       = "Synergy%2i%2i"
                //              kMsgHelloBack           = "Synergy%2i%2i%s"
                uint16_t server_major, server_minor;
                if (!(sspNetU16(msg, &server_major) && sspNetU16(msg, &server_minor))) {
                        PARSE_ERROR();
                }
                logInfo("Server is %s %" PRIu16 ".%" PRIu16, imp, server_major, server_minor);
                sspAddString(reply, imp);
                sspAddNetU16(reply, USYNERGY_PROTOCOL_MAJOR);
                sspAddNetU16(reply, USYNERGY_PROTOCOL_MINOR);
                sspAddNetU32(reply, (uint32_t)strlen(reply->m_clientName));
                sspAddString(reply, reply->m_clientName);
                if (!send_reply(ctx, reply))
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
