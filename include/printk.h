#ifndef _PRINTK_H
#define _PRINTK_H

#define va_start(ap, list) (__builtin_va_start(ap, list))
#define va_arg(ap, type) (__builtin_va_arg(ap, type))
#define va_end(ap) (__builtin_va_end(ap))
#define va_copy(d, s) (__builtin_va_copy(d, s))
typedef __builtin_va_list va_list;

void printk_init(void);
void printk(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void panic(const char *str) __attribute__((noreturn));

#endif
