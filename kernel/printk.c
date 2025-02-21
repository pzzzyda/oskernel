#include "printk.h"
#include "dev/console.h"
#include "lib/string.h"
#include "lock.h"

bool panicked = false;

static struct spin_lock printk_lock;

/* printk flags */
#define PF_SIGNED (1 << 0)
#define PF_PADDING (1 << 1)
#define PF_ACCURACY (1 << 2)
#define PF_ALIGN (1 << 3)
#define PF_ZERO_PADDING (1 << 4)
#define PF_LONG (1 << 5)
#define PF_UPPER (1 << 6)
#define PF_POINTER (1 << 7)
#define PF_LONG_LONG (1 << 8)

static const char digits_l[] = "0123456789abcdef";
static const char digits_u[] = "0123456789ABCDEF";

void printk_init(void)
{
	spin_lock_init(&printk_lock, "printk");
}

static void print_str(const char *str, size_t len)
{
	size_t i = 0;
	while (i < len)
		console_putc(str[i++]);
}

static void print_int(long long x, int base, int left, int right, int flag)
{
	char buf[70];
	const char *digits;
	int pos, len;
	bool is_negative;
	char padding_char;

	is_negative = (flag & PF_SIGNED) && (int64_t)x < 0;

	if (flag & PF_UPPER)
		digits = digits_u;
	else
		digits = digits_l;

	pos = sizeof(buf);
	do {
		--pos;
		buf[pos] = digits[x % base];
		x /= base;
	} while (x);
	len = sizeof(buf) - pos;

	if (flag & PF_POINTER) {
		--pos;
		buf[pos] = 'x';
		--pos;
		buf[pos] = '0';
		len += 2;
	}

	if ((flag & PF_ACCURACY) && !(flag & PF_POINTER) && (len < right)) {
		while (len < right) {
			--pos;
			buf[pos] = '0';
			++len;
		}
	}

	if (is_negative) {
		--pos;
		buf[pos] = '-';
		++len;
	}

	if (flag & PF_ALIGN) {
		print_str(buf + pos, len);
		while (len < left) {
			print_str(" ", 1);
			++len;
		}
		return;
	}

	if (flag & PF_PADDING) {
		if ((flag & PF_ZERO_PADDING) && !(flag & PF_POINTER))
			padding_char = '0';
		else
			padding_char = ' ';
		while (len < left) {
			--pos;
			buf[pos] = padding_char;
			++len;
		}
	}

	print_str(buf + pos, len);
}

void printk(const char *fmt, ...)
{
	va_list args;
	int i, j, k;
	long long x;
	char c, *s;
	bool parsing;
	int flag, left, right;

	parsing = false;
	flag = 0;
	left = 0;
	right = 0;
	k = 0;
	va_start(args, fmt);
	spin_lock_acquire(&printk_lock);
	for (i = 0; fmt[i]; i++) {
		c = fmt[i];

		if (!parsing) {
			if (c == '%') {
				parsing = true;
				k = i;
				/* check '-' */
				if (fmt[i + 1] == '-') {
					i++;
					flag |= PF_ALIGN;
				}
			} else {
				print_str(&c, 1);
			}
			continue;
		}

		switch (c) {
		case 'i':
		case 'd':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			flag |= PF_SIGNED;
			print_int(x, 10, left, right, flag);
			break;
		case 'u':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			print_int(x, 10, left, right, flag);
			break;
		case 'o':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			print_int(x, 8, left, right, flag);
			break;
		case 'b':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			print_int(x, 2, left, right, flag);
			break;
		case 'p':
			flag |= PF_LONG;
			flag |= PF_POINTER;
		case 'x':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			print_int(x, 16, left, right, flag);
			break;
		case 'X':
			if (flag & PF_LONG_LONG)
				x = va_arg(args, long long);
			else if (flag & PF_LONG)
				x = va_arg(args, long);
			else
				x = va_arg(args, int);
			flag |= PF_UPPER;
			print_int(x, 16, left, right, flag);
			break;
		case 's':
			s = va_arg(args, char *);
			if (!s) {
				print_str("null", 4);
			} else {
				if (flag & PF_ACCURACY) {
					for (j = 0; j < right; j++) {
						if (s[j])
							print_str(s + j, 1);
						else
							print_str(" ", 1);
					}
				} else {
					print_str(s, strlen(s));
				}
			}
			break;
		case 'c':
			c = va_arg(args, int);
			print_str(&c, 1);
			break;
		case 'l':
			if ((flag & PF_LONG) == 0)
				flag |= PF_LONG;
			else
				flag |= PF_LONG_LONG;
			continue;
		case '.':
			flag |= PF_ACCURACY;
			continue;
		case '-':
			/* ignore */
			continue;
		case '0':
			if (left == 0) {
				flag |= PF_ZERO_PADDING;
				/* skip the other '0's after the first '0' */
				while (fmt[i + 1] == '0')
					i++;
				continue;
			}
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			flag |= PF_PADDING;
			if (flag & PF_ACCURACY)
				right = right * 10 + c - '0';
			else
				left = left * 10 + c - '0';
			continue;
		default:
			print_str(fmt + k, i - k + 1);
			break;
		}
		parsing = false;
		flag = 0;
		left = 0;
		right = 0;
		k = 0;
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
