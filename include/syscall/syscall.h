#ifndef _SYSCALL_H
#define _SYSCALL_H

#include "types.h"

void syscall(void);
uint64_t arg_raw(uint32_t num);
int fetch_str(uint64_t addr, char *buf, size_t max_len);
int fetch_addr(uint64_t addr, uint64_t *buf);

#define ARG(num, type) ((type)arg_raw(num))

#endif
