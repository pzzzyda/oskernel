#include "printk.h"
#include "dev/console.h"
#include "lib/string.h"
#include "lock.h"

bool panicked = false;

static struct spin_lock printk_lock;

/* printk flags */
#define PF_SIGNED (1 << 0)
#define PF_LONG (1 << 5)
#define PF_LONG_LONG (1 << 8)

static const char digits[] = "0123456789abcdef";

void printk_init(void)
{
	spin_lock_init(&printk_lock, "printk");
}

static void print_str(const char *str)
{
	if (!str)
		console_puts("(null)", 6);
	else
		console_puts(str, strlen(str));
}

static void print_ptr(unsigned long long x)
{
	char buf[32];
	size_t pos = sizeof(buf);
	do {
		buf[--pos] = digits[x % 16];
		x /= 16;
	} while (x > 0);
	console_puts("0x", 2);
	console_puts(buf + pos, sizeof(buf) - pos);
}

static void print_int(unsigned long long x, unsigned int base, int flag)
{
	char buf[32];
	size_t pos;
	bool is_negative;
	if (flag & PF_SIGNED) {
		if ((long long)x < 0) {
			is_negative = true;
			x = (-(long long)x);
		} else {
			is_negative = false;
		}
	} else {
		is_negative = false;
	}
	pos = sizeof(buf);
	do {
		buf[--pos] = digits[x % base];
		x /= base;
	} while (x > 0);
	if (is_negative)
		console_putc('-');
	console_puts(buf + pos, sizeof(buf) - pos);
}

void printk(const char *fmt, ...)
{
	va_list args;
	int i, j;
	char c;
	unsigned long long x;
	bool is_valid;
	int flag;
	va_start(args, fmt);
	spin_lock_acquire(&printk_lock);
	for (i = 0; fmt[i]; i++) {
		if (fmt[i] != '%') {
			console_putc(fmt[i]);
			continue;
		}
		is_valid = false;
		flag = 0;
		for (j = i + 1; fmt[j]; j++) {
			c = fmt[j];
			switch (c) {
			case 'd':
			case 'u':
			case 'x':
				if (c == 'd') {
					flag |= PF_SIGNED;
					if (flag & PF_LONG_LONG)
						x = va_arg(args, long long);
					else if (flag & PF_LONG)
						x = va_arg(args, long);
					else
						x = va_arg(args, int);
				} else {
					if (flag & PF_LONG_LONG)
						x = va_arg(args,
							   unsigned long long);
					else if (flag & PF_LONG)
						x = va_arg(args, unsigned long);
					else
						x = va_arg(args, unsigned int);
				}
				if (c == 'd' || c == 'u')
					print_int(x, 10, flag);
				else
					print_int(x, 16, flag);
				is_valid = true;
				goto end;
			case 'p':
				x = (unsigned long long)va_arg(args, void *);
				print_ptr(x);
				is_valid = true;
				goto end;
			case 's':
				print_str(va_arg(args, const char *));
				is_valid = true;
				goto end;
			case 'c':
				console_putc(va_arg(args, int));
				is_valid = true;
				goto end;
			case '%':
				console_putc('%');
				is_valid = true;
				goto end;
			case 'l':
				if (!(flag & PF_LONG))
					flag |= PF_LONG;
				else
					flag |= PF_LONG_LONG;
				continue;
			default:
				goto end;
			}
		}

	end:
		if (!is_valid)
			console_putc('%');
		else
			i = j;
	}
	spin_lock_release(&printk_lock);
	va_end(args);
}

void panic(const char *str)
{
	printk("panic: %s\n", str);
	panicked = true;
	while (true)
		continue;
}
