#ifndef _STRING_H
#define _STRING_H

#include "types.h"

void *memset(void *ptr, int v, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

size_t strlen(const char *s);
int strncmp(const char *s1, const char *s2, size_t n);
char *strncpy(char *dst, const char *src, size_t n);
char *strrchr(const char *s, int c);

#endif
