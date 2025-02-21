#ifndef _TYPES_H
#define _TYPES_H

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

typedef unsigned long size_t;
typedef long ssize_t;

typedef long off_t;

typedef int pid_t;

#define bool _Bool
#define false 0
#define true 1

#ifdef NULL
#undef NULL
#define NULL ((void *)0)
#else
#define NULL ((void *)0)
#endif

#endif
