#include "lib/string.h"

void *memset(void *ptr, int v, size_t n)
{
	while (n > 0)
		((uint8_t *)ptr)[--n] = v;
	return ptr;
}

void *memmove(void *dst, const void *src, size_t n)
{
	size_t i;
	if (dst < src) {
		for (i = 0; i < n; ++i)
			((uint8_t *)dst)[i] = ((const uint8_t *)src)[i];
	} else {
		for (i = n; i--;)
			((uint8_t *)dst)[i] = ((const uint8_t *)src)[i];
	}
	return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
	return memmove(dst, src, n);
}

size_t strlen(const char *s)
{
	size_t len = 0;
	while (s[len])
		len++;
	return len;
}

int strncmp(const char *s1, const char *s2, size_t n)
{
	while ((n > 0) && (*s1 != 0) && (*s1 == *s2)) {
		n--;
		s1++;
		s2++;
	}
	return n == 0 ? 0 : (*s1 - *s2);
}

char *strncpy(char *dst, const char *src, size_t n)
{
	char *ret = dst;
	for (; n > 0 && *src; --n, ++src, ++dst)
		*dst = *src;
	for (; n > 0; --n, ++dst)
		*dst = 0;
	return ret;
}

char *strrchr(const char *s, int c)
{
	const char *pc = NULL;
	for (; *s; s++) {
		if (*s == c)
			pc = s;
	}
	return (char *)pc;
}
