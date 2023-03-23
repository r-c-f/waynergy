/* fdio_full -- read/write without shortness
 *
 * Version 1.1
 *
 * Copyright 2021 Ryan Farley <ryan.farley@gmx.com>
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
#ifndef FDIO_FULL_H_INC
#define FDIO_FULL_H_INC

#if defined(__GNUC__)
#define SHL_UNUSED __attribute__((unused))
#else
#define SHL_UNUSED
#endif

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

enum fdio_full_flag {
	FDIO_FULL_FLAG_NONE = 0,
	FDIO_FULL_FLAG_NB = 1, //don't block on non-blocking sockets
	FDIO_FULL_FLAG_INTR = 2, //bail out on EINTR
};

SHL_UNUSED static bool read_full(int fd, void *buf, size_t count, enum fdio_full_flag flags)
{
	ssize_t ret;
	char *pos;

	for (pos = buf; count;) {
		errno = 0;
		ret = read(fd, pos, count > SSIZE_MAX ? SSIZE_MAX : count);
		if (ret < 1) {
			switch (errno) {
				case EINTR:
					if (flags & FDIO_FULL_FLAG_INTR)
						return false;
					continue;
				case EAGAIN:
				#if EAGAIN != EWOULDBLOCK
				case EWOULDBLOCK:
				#endif
					if (flags & FDIO_FULL_FLAG_NB)
						return false;
					continue;
				default:
					return false;
			}
		}
		pos += ret;
		count -= ret;
	}
	return true;
}

SHL_UNUSED static bool write_full(int fd, const void *buf, size_t count, enum fdio_full_flag flags)
{
	ssize_t ret;
	const char *pos;

	for (pos = buf; count;) {
		errno = 0;
		ret = write(fd, pos, count > SSIZE_MAX ? SSIZE_MAX : count);
		if (ret < 1) {
			switch (errno) {
				case EINTR:
					if (flags & FDIO_FULL_FLAG_INTR)
						return false;
					continue;
				case EAGAIN:
				#if EAGAIN != EWOULDBLOCK
				case EWOULDBLOCK:
				#endif
					if (flags & FDIO_FULL_FLAG_NB)
						return false;
					continue;
				default:
					return false;
			}
		}
		pos += ret;
		count -= ret;
	}
	return true;
}

#undef SHL_UNUSED
#endif
