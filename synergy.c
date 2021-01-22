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

