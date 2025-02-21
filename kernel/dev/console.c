#include "dev/console.h"
#include "dev/uart.h"
#include "fs/file.h"
#include "lock.h"
#include "param.h"
#include "sched/cpu.h"

#define BACKSPACE 0x100
#define C(x) ((x) - '@')

struct console {
	struct spin_lock lock;
#define INPUT_SIZE 128
	char buf[INPUT_SIZE];
	uint32_t r;
	uint32_t w;
	uint32_t e;
};

extern struct device devlist[N_DEV];
static struct console cons;

void console_init(void)
{
	spin_lock_init(&cons.lock, "console");
	uart_init();
	devlist[CONSOLE].read = console_read;
	devlist[CONSOLE].write = console_write;
}

void console_putc(int c)
{
	if (c == BACKSPACE) {
		uart_putc_sync('\b');
		uart_putc_sync(' ');
		uart_putc_sync('\b');
	} else {
		uart_putc_sync(c);
	}
}

void console_intr(int c)
{
	spin_lock_acquire(&cons.lock);
	switch (c) {
	case C('P'):
		proc_dump();
		break;
	case '\x7f': /* Delete key */
		if (cons.e != cons.w) {
			cons.e--;
			console_putc(BACKSPACE);
		}
		break;
	default:
		if (c != 0 && ((cons.e + 1) % INPUT_SIZE) != cons.r) {
			c = (c == '\r') ? '\n' : c;
			console_putc(c);
			cons.buf[cons.e] = c;
			cons.e = (cons.e + 1) % INPUT_SIZE;
			if (c == '\n' || c == C('D') ||
			    ((cons.e + 1) % INPUT_SIZE) == cons.r) {
				cons.w = cons.e;
				wake_up(&cons.r);
			}
		}
		break;
	}
	spin_lock_release(&cons.lock);
}

ssize_t console_read(bool to_user, uint64_t dst, size_t n)
{
	size_t target;
	int c;
	char cbuf;

	target = n;

	spin_lock_acquire(&cons.lock);
	while (n > 0) {
		while (cons.r == cons.w) {
			if (killed(running_proc())) {
				spin_lock_release(&cons.lock);
				return -1;
			}
			sleep_on(&cons.r, &cons.lock);
		}

		c = cons.buf[cons.r];
		cons.r = (cons.r + 1) % INPUT_SIZE;

		if (c == C('D')) {
			if (n < target)
				cons.r--;
			break;
		}

		cbuf = c;
		if (either_copy_out(to_user, dst, &cbuf, sizeof(char)) != 0)
			break;

		dst++;
		n--;
	}
	spin_lock_release(&cons.lock);

	return target - n;
}

ssize_t console_write(bool from_user, uint64_t src, size_t n)
{
	size_t i;
	char c;

	spin_lock_acquire(&cons.lock);
	for (i = 0; i < n; i++) {
		if (either_copy_in(from_user, &c, src, sizeof(char)) != 0)
			break;
		uart_putc(c);
		src++;
	}
	spin_lock_release(&cons.lock);

	return i;
}
