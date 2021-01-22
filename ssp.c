#include "ssp.h"
#include <string.h>

bool sspSeek(struct sspBuf *buf, size_t len)
{
	if (!buf) {
		return false;
	}
	if (buf->pos + len > buf->len) {
		return false;
	}
	buf->pos += len;
	return true;
}
bool sspNetInt(struct sspBuf *buf, void *res, size_t len)
{
	ssize_t i;
	unsigned char *res_b = res;
	if (!res) {
		return sspSeek(buf, len);
	}
	if (!buf) {
		return false;
	}
	if (buf->pos + len > buf->len) {
		return false;
	}
	for (i = 0; i < len; ++i) {
		#ifdef USYNERGY_LITTLE_ENDIAN
		res_b[len - i - 1] = buf->data[buf->pos++];
		#else
		res_b[i] = buf->data[buf->pos++];
		#endif
	}
	return true;
}
bool sspMemMove(void *dest, struct sspBuf *buf, size_t len)
{
	if (!(buf && dest)) {
		return false;
	}
	if (buf->pos + len > buf->len) {
		return false;
	}
	memmove(dest, buf->data + buf->pos, len);
	buf->pos += len;
	return true;
}

bool sspAddBin(struct sspBuf *buf, void *data, size_t len)
{
	if (!buf)
		return false;
	if (!data)
		return false;
	if (buf->pos + len > buf->len) {
		return false;
	}
	memmove(buf->data + buf->pos, data, len);
	buf->pos += len;
	return true;
}
bool sspAddNetInt(struct sspBuf *buf, void *val, size_t len)
{
#ifdef USYNERGY_LITTLE_ENDIAN
	ssize_t i;
	unsigned char *val_b = val;
	if (!val) {
		return sspSeek(buf, len);
	}
	if (!buf) {
		return false;
	}
	if (buf->pos + len > buf->len) {
		return false;
	}
	for (i = 0; i < len; ++i) {
		buf->data[buf->pos + len - i - 1] = val_b[i];
	}
	return true;
#else
	return sspAddBin(buf, val, len);
}

