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

extern bool sspAddBin(struct sspBuf *buf, void *data, size_t len);
extern bool sspAddNetInt(struct sspBuf *buf, void *val, size_t len);

static inline bool sspAddString(struct sspBuf *buf, const char *str)
{
	size_t len = strlen(str);
	return sspAddBin(buf, str, len);
}
static inline bool sspAddChar(struct sspBuf *buf, char val)
{
	return sspAddNetInt(buf, &val, 1);
}
static inline bool sspAddUChar(struct sspBuf *buf, unsigned char val)
{
	return sspAddNetInt(buf, &val, 1);
}
static inline bool sspAddNet16(struct sspBuf *buf, int16_t val)
{
	return sspAddNetInt(buf, &val, 2);
}
static inline bool sspAddNetU16(struct sspBuf *buf, uint16_t val)
{
	return sspAddNetInt(buf, &val, 2);
}
static inline bool sspAddNet32(struct sspBuf *buf, int32_t val)
{
	return sspAddNetInt(buf, &val, 4);
}
static inline bool sspAddNetU32(struct sspBuf *buf, uint32_t val)
{
	return sspAddNetInt(buf, &val, 4);
}

