#pragma once
/* safe stream parsing -- because the existing network code here is a total
 * clusterfuck */

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

struct sspBuf {
	const unsigned char *data;
	size_t pos;
	size_t len;
};

extern bool sspSeek(struct sspBuf *buf, size_t len);
extern bool sspNetInt(struct sspBuf *buf, void *res, size_t len);
extern bool sspMemMove(void *dest, struct sspBuf *buf, size_t len);

static inline bool sspChar(struct sspBuf *buf, char *res)
{
	return sspNetInt(buf, res, 1);
}
static inline bool sspUChar(struct sspBuf *buf, unsigned char *res)
{
	return sspNetInt(buf, res, 1);
}
static inline bool sspNet16(struct sspBuf *buf, int16_t *res)
{
	return sspNetInt(buf, res, 2);
}
static inline bool sspNetU16(struct sspBuf *buf, uint16_t *res)
{
	return sspNetInt(buf, res, 2);
}
static inline bool sspNet32(struct sspBuf *buf, int32_t *res)
{
	return sspNetInt(buf, res, 4);
}
static inline bool sspNetU32(struct sspBuf *buf, uint32_t *res)
{
	return sspNetInt(buf, res, 4);
}

