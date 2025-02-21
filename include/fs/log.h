#ifndef _LOG_H
#define _LOG_H

#include "types.h"

struct buffer;
struct super_block;

void log_init(uint32_t dev, struct super_block *sb);
void begin_op(void);
void end_op(void);
void log_write(struct buffer *b);

#endif
