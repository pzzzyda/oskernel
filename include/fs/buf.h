#ifndef _BUF_H
#define _BUF_H

#include "fs/fs.h"
#include "lock.h"

struct buffer {
	bool valid;   /* Has data been read from disk? */
	bool disk;    /* Does disk "own" buf? */
	uint32_t dev; /* Device number */
	uint32_t bno; /* Block number */

	uint32_t refcnt; /* Reference count */

	struct sleep_lock lock;
	uint8_t data[BLOCK_SIZE];

	/* For LRU cache list */
	struct buffer *prev;
	struct buffer *next;
};

void binit(void);
struct buffer *bread(uint32_t dev, uint32_t bno);
void bwrite(struct buffer *b);
void brelse(struct buffer *b);
void bpin(struct buffer *b);
void bunpin(struct buffer *b);

#endif
