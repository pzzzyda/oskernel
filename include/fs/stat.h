#ifndef _STAT_H
#define _STAT_H

#include "types.h"

#define FT_NONE 0
#define FT_DIR 1
#define FT_FILE 2
#define FT_DEVICE 3

struct stat {
	uint16_t type;
	uint16_t ino;
	uint16_t nlink;
	uint32_t size;
};

#endif
