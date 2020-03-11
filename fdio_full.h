
#ifndef FDIO_FULL_H_INC
#define FDIO_FULL_H_INC

#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

/* functions to do read/write without shortness */
#ifdef __GNUC__
__attribute__((unused))
#endif
static bool read_full(int fd, void *buf, size_t count)
{
	ssize_t ret;
	char *pos;

	for (pos = buf; count;) {
		errno = 0;
		ret = read(fd, pos, count > SSIZE_MAX ? SSIZE_MAX : count);
		if (ret < 1) {
			if (errno == EINTR)
				continue;
			return false;
		}
		pos += ret;
		count -= ret;
	}
	return true;
}
#ifdef __GNUC__
__attribute__((unused))
#endif
static bool read_full_i(int fd, void *buf, size_t count)
{
	ssize_t ret;
	char *pos;

	for (pos = buf; count;) {
		errno = 0;
		ret = read(fd, pos, count > SSIZE_MAX ? SSIZE_MAX : count);
		if (ret < 1) {
			//if (errno == EINTR)
			//	continue;
			return false;
		}
		pos += ret;
		count -= ret;
	}
	return true;
}
#ifdef __GNUC__
__attribute__((unused))
#endif
static bool write_full(int fd, const void *buf, size_t count)
{
	ssize_t ret;
	const char *pos;

	for (pos = buf; count;) {
		errno = 0;
		ret = write(fd, pos, count > SSIZE_MAX ? SSIZE_MAX : count);
		if (ret < 1) {
			if (errno == EINTR)
				continue;
			return false;
		}
		pos += ret;
		count -= ret;
	}
	return true;
}

#endif
