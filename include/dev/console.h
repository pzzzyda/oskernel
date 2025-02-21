#ifndef _CONSOLE_H
#define _CONSOLE_H

#include "types.h"

void console_init(void);
void console_putc(int c);
void console_intr(int c);
ssize_t console_read(bool to_user, uint64_t dst, size_t n);
ssize_t console_write(bool from_user, uint64_t src, size_t n);

#endif
