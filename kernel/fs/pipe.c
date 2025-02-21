#include "fs/pipe.h"
#include "lock.h"
#include "mm/mm.h"
#include "printk.h"
#include "sched/cpu.h"

#define PIPE_SIZE 1024

struct pipe {
	struct spin_lock lock;
	uint32_t r;
	uint32_t w;
	bool read_open;
	bool write_open;
	char data[PIPE_SIZE];
};

int pipe_alloc(struct file **rfile, struct file **wfile)
{
	struct pipe *pi = NULL;
	if (!(*rfile = file_alloc()) || !(*wfile = file_alloc()))
		goto bad;
	if (!(pi = pm_alloc()))
		goto bad;
	pi->read_open = true;
	pi->write_open = true;
	pi->w = 0;
	pi->r = 0;
	spin_lock_init(&pi->lock, "pipe");
	(*rfile)->type = FD_PIPE;
	(*rfile)->readable = true;
	(*rfile)->writable = false;
	(*rfile)->pi = pi;
	(*wfile)->type = FD_PIPE;
	(*wfile)->readable = false;
	(*wfile)->writable = true;
	(*wfile)->pi = pi;
	return 0;

bad:
	if (pi)
		pm_free(pi);
	if (*rfile) {
		file_close(*rfile);
		*rfile = NULL;
	}
	if (*wfile) {
		file_close(*wfile);
		*wfile = NULL;
	}
	return -1;
}

void pipe_close(struct pipe *pi, bool writable)
{
	spin_lock_acquire(&pi->lock);
	if (writable) {
		pi->write_open = false;
		wake_up(&pi->r);
	} else {
		pi->read_open = false;
		wake_up(&pi->w);
	}
	if (!pi->read_open && !pi->write_open) {
		spin_lock_release(&pi->lock);
		pm_free(pi);
	} else {
		spin_lock_release(&pi->lock);
	}
}

ssize_t pipe_read(struct pipe *pi, uint64_t dst, size_t n)
{
	char c = 0;
	size_t i = 0;
	struct process *p = running_proc();

	spin_lock_acquire(&pi->lock);
	while (pi->r == pi->w && pi->write_open) {
		if (killed(p)) {
			spin_lock_release(&pi->lock);
			return -1;
		}
		sleep_on(&pi->r, &pi->lock);
	}
	for (i = 0; i < n; i++) {
		if (pi->r == pi->w)
			break;
		c = pi->data[pi->r];
		pi->r = (pi->r + 1) % PIPE_SIZE;
		if (copy_out(p->page_table, dst + i, &c, 1))
			break;
	}
	wake_up(&pi->w);
	spin_lock_release(&pi->lock);
	return i;
}

ssize_t pipe_write(struct pipe *pi, uint64_t src, size_t n)
{
	char c = 0;
	size_t i = 0;
	struct process *p = running_proc();

	spin_lock_acquire(&pi->lock);
	while (i < n) {
		if (!pi->read_open || killed(p)) {
			spin_lock_release(&pi->lock);
			return -1;
		}
		if ((pi->w + 1) % PIPE_SIZE == pi->r) {
			wake_up(&pi->r);
			sleep_on(&pi->w, &pi->lock);
		} else {
			if (copy_in(p->page_table, &c, src, 1))
				break;
			pi->data[pi->w] = c;
			pi->w = (pi->w + 1) % PIPE_SIZE;
			i++;
			src++;
		}
	}
	wake_up(&pi->r);
	spin_lock_release(&pi->lock);
	return i;
}
