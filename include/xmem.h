/* xmem -- memory operations that can only fail catastrophically
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
#ifndef XMEM_H_INC
#define XMEM_H_INC

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __GNUC__
__attribute__((unused))
#endif
static void *xmalloc(size_t len)
{
	void *ret;
	if (!(ret = malloc(len)))
		abort();
	return ret;
}
#ifdef __GNUC__
__attribute__((unused))
#endif
static void *xcalloc(size_t nmemb, size_t size)
{
	void *ret;
	if (!(ret = calloc(nmemb, size)))
		abort();
	return ret;
}
#ifdef __GNUC__
__attribute__((unused))
#endif
static void *xrealloc(void *ptr, size_t len)
{
	if (!(ptr = realloc(ptr, len)))
		abort();
	return ptr;
}
#ifdef __GNUC__
__attribute__((unused))
#endif
static void *xreallocarray(void *ptr, size_t nmemb, size_t size)
{
	if (size && nmemb > SIZE_MAX / size) {
		abort();
	}
	return xrealloc(ptr, nmemb * size);
}
#ifdef __GNUC__
__attribute__((unused))
#endif
static char *xstrdup(const char *str)
{
	char *ret;
	if (!str)
		return NULL;
	if (!(ret = strdup(str)))
		abort();
	return ret;
}
#ifdef __GNUC__
__attribute__((unused))
#endif
static void xasprintf(char **strp, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (vasprintf(strp, fmt, ap) == -1)
		abort();
	va_end(ap);
}
#ifdef __GNUC__
__attribute__((unused))
#endif
static void strfreev(char **strv)
{
	size_t i;
	if (strv) {
		for (i = 0; strv[i]; ++i) {
			free(strv[i]);
		}
		free(strv);
	}
}

#endif
