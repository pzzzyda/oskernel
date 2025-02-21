#ifndef _TIMER_H
#define _TIMER_H

#include "types.h"

void timer_init(void);
void timer_intr(void);
void timer_set_next(void);
int timer_sleep(uint64_t n);

#endif
