#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

__attribute__((unused))
static void *xmalloc(size_t len)
{
	void *ret;
	if (!(ret = malloc(len)))
		abort();
	return ret;
}
__attribute__((unused))
static void *xcalloc(size_t nmemb, size_t size)
{
	void *ret;
	if (!(ret = calloc(nmemb, size)))
		abort();
	return ret;
}
__attribute__((unused))
static void *xrealloc(void *ptr, size_t len)
{
	if (!(ptr = realloc(ptr, len)))
		abort();
	return ptr;
}
__attribute__((unused))
static char *xstrdup(const char *str)
{
	char *ret;
	if (!(ret = strdup(str)))
		abort();
	return ret;
}
__attribute__((unused))
static void xasprintf(char **strp, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	if (vasprintf(strp, fmt, ap) == -1)
		abort();
	va_end(ap);
}
__attribute__((unused))
static void strfreev(char **strv)
{
	size_t i;
	for (i = 0; strv[i]; ++i) {
		free(strv[i]);
	}
	free(strv);
}
