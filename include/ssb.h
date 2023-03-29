/* simple string builder
 *
 * Version 1.3
 *
 * Copyright 2023 Ryan Farley <ryan.farley@gmx.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef SSB_H_INC
#define SSB_H_INC

#if defined(__GNUC__)
#define SHL_UNUSED __attribute__((unused))
#else
#define SHL_UNUSED
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdbool.h>
#include <errno.h>

/* defines the length used for ssb_readfile, if SSB_GROW_EXACT is used */
#if !defined SSB_READ_LEN
#define SSB_READ_LEN 4096
#endif


enum ssb_grow {
	/* grow by a factor of 1.5 */
	SSB_GROW_1_5 = 0,
	/* grow by a factor of 2.0 */
	SSB_GROW_2_0,
	/* grow by exactly the amount needed each time */
	SSB_GROW_EXACT
};

struct ssb {
	enum ssb_grow grow;
	char *buf;
	size_t size;
	size_t pos;
};

SHL_UNUSED static bool ssb_truncate(struct ssb *s, size_t newsize)
{
	char *realloc_buf;

	if (!(realloc_buf = realloc(s->buf, newsize + 1))) {
		return false;
	}
	s->buf = realloc_buf;
	if (s->pos >= newsize) {
		s->buf[newsize] = '\0';
	}

	s->size = newsize + 1;

	return true;
}

SHL_UNUSED static bool ssb_grow_min(struct ssb *s, size_t min)
{
	size_t newsize;

	newsize = min += s->size;

	switch (s->grow) {
		case SSB_GROW_1_5:
			newsize = s->size + (s->size / 2);
			break;
		case SSB_GROW_2_0:
			newsize = s->size * 2;
			break;
		default:
			break;
	}

	return ssb_truncate(s, (newsize >= min ? newsize : min));
}

SHL_UNUSED static int ssb_vprintf(struct ssb *s, const char *fmt, va_list ap)
{
	va_list sizeap;
	int size;

	if (!s) {
		errno = EINVAL;
		return -1;
	}

	va_copy(sizeap, ap);
	if ((size = vsnprintf(s->buf + s->pos, s->size - s->pos, fmt, sizeap)) >= s->size - s->pos) {
		if (!ssb_grow_min(s, size)) {
			return -1;
		}
		vsnprintf(s->buf + s->pos, s->size - s->pos, fmt, ap);
	}
	va_end(sizeap);
	s->pos += size;

	return 0;
}

SHL_UNUSED static int ssb_printf(struct ssb *s, const char *fmt, ...)
{
	int ret;
	va_list ap;
	va_start(ap, fmt);
	ret = ssb_vprintf(s, fmt, ap);
	va_end(ap);
	return ret;
}

SHL_UNUSED static void ssb_rewind(struct ssb *s)
{
	s->pos = 0;
	if (s->buf) {
		*(s->buf) = '\0';
	}
}

SHL_UNUSED static void ssb_free(struct ssb *s)
{
	s->pos = 0;
	if (s->buf) {
		free(s->buf);
	}
	s->size = 0;
	s->buf = NULL;
}

/* add a single character to the buffer */
SHL_UNUSED static bool ssb_addc(struct ssb *s, unsigned char c)
{
	if (s->size - s->pos < 2) {
		if (!ssb_grow_min(s, 1)) {
		       return false;
		}
	}
	s->buf[s->pos++] = c;
	s->buf[s->pos] = '\0';
	return true;
}

/* analogous to getdelim -- returns true if anything is successfully read */
SHL_UNUSED static bool ssb_getdelim(struct ssb *s, int delim, FILE *f)
{
	int c;
	size_t old_pos = s->pos;

	while ((c = getc(f)) != EOF) {
		if (!ssb_addc(s, c)) {
			return false;
		}

		if (c == delim) {
			return true;
		}
	}

	return s->pos - old_pos;
}

/* a bit like getline -- return true if anything is successfully read */
SHL_UNUSED static bool ssb_getline(struct ssb *s, FILE *f)
{
	return ssb_getdelim(s, '\n', f);
}

/* read an entire file into a buffer */
SHL_UNUSED static bool ssb_readfile(struct ssb *s, FILE *f)
{
	size_t read_count = 0;

	do {
		s->pos += read_count;
		if (s->size - s->pos <= 2) {
			if (!ssb_grow_min(s, SSB_READ_LEN)) {
			       return false;
			}
		}
	} while ((read_count = fread(s->buf + s->pos, 1, s->size - s->pos - 1, f)));

	/* ideally the result would be NUL terminated, but we cannot be sure */
	s->buf[s->pos] = '\0';

	return feof(f);
}

/* x variants -- can never fail! */
SHL_UNUSED static void ssb_xvprintf(struct ssb *s, const char *fmt, va_list ap)
{
	if (ssb_vprintf(s, fmt, ap))
		abort();
}
SHL_UNUSED static void ssb_xprintf(struct ssb *s, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	ssb_xvprintf(s, fmt, ap);
	va_end(ap);
}
SHL_UNUSED static void ssb_xtruncate(struct ssb *s, size_t newsize)
{
	if (!ssb_truncate(s, newsize))
		abort();
}
#undef SHL_UNUSED
#endif

