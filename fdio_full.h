
#ifndef FDIO_FULL_H_INC
#define FDIO_FULL_H_INC

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

/* functions to do read/write without shortness */
#ifdef __GNUC__
__attribute__((unused))
#endif
static bool read_full(int fd, void *buf, size_t count, enum fdio_full_flag flags)
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
#ifdef __GNUC__
__attribute__((unused))
#endif
static bool write_full(int fd, const void *buf, size_t count, enum fdio_full_flag flags)
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

#endif
