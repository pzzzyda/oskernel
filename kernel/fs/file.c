#include "fs/file.h"
#include "fs/fs.h"
#include "fs/inode.h"
#include "fs/log.h"
#include "fs/pipe.h"
#include "fs/stat.h"
#include "lib/string.h"
#include "lock.h"
#include "mm/mm.h"
#include "param.h"
#include "printk.h"
#include "sched/cpu.h"

struct file_table {
	struct file files[N_FILE];
	struct spin_lock lock;
};

struct device devlist[N_DEV];
static struct file_table ftable;

#define FIRST_FILE (&ftable.files[0])
#define LAST_FILE (&ftable.files[N_FILE - 1])

void file_init(void)
{
	spin_lock_init(&ftable.lock, "ftable");
}

struct file *file_alloc(void)
{
	struct file *f;
	spin_lock_acquire(&ftable.lock);
	for (f = FIRST_FILE; f <= LAST_FILE; f++) {
		if (f->refcnt == 0) {
			f->refcnt = 1;
			spin_lock_release(&ftable.lock);
			return f;
		}
	}
	spin_lock_release(&ftable.lock);
	return NULL;
}

void file_close(struct file *f)
{
	spin_lock_acquire(&ftable.lock);
	if (f->refcnt < 1)
		panic("close an invalid file");
	f->refcnt--;
	if (f->refcnt == 0) {
		if (f->type == FD_PIPE)
			pipe_close(f->pi, f->writable);
		else
			iput(f->inode);
		f->type = FD_NONE;
	}
	spin_lock_release(&ftable.lock);
}

struct file *file_dup(struct file *f)
{
	spin_lock_acquire(&ftable.lock);
	if (f->refcnt < 1)
		panic("duplicate an invalid file");
	f->refcnt++;
	spin_lock_release(&ftable.lock);
	return f;
}

int file_stat(struct file *f, uint64_t pstat)
{
	struct stat st;

	if (f->type == FD_INODE || f->type == FD_DEVICE) {
		ilock(f->inode);
		stati(f->inode, &st);
		iunlock(f->inode);
		return copy_out(running_proc()->page_table, pstat, &st,
				sizeof(st));
	} else {
		return -1;
	}
}

off_t file_lseek(struct file *f, off_t offset, int whence)
{
	off_t ret;

	if (f->type != FD_INODE)
		return -1;

	switch (whence) {
	case SEEK_SET:
		ret = lseeki(f->inode, offset);
		break;
	case SEEK_CUR:
		ret = lseeki(f->inode, f->off + offset);
		break;
	case SEEK_END:
		ret = lseeki(f->inode, f->inode->size + offset);
		break;
	default:
		ret = -1;
		break;
	}
	if (ret != -1)
		f->off = ret;
	return ret;
}

ssize_t file_read(struct file *f, uint64_t dst, size_t n)
{
	ssize_t ret;

	if (!f->readable)
		return -1;

	switch (f->type) {
	case FD_DEVICE:
		if (f->major >= N_DEV || !devlist[f->major].read)
			return -1;
		ret = devlist[f->major].read(true, dst, n);
		break;
	case FD_INODE:
		ilock(f->inode);
		ret = readi(f->inode, true, dst, f->off, n);
		if (ret > 0)
			f->off += ret;
		iunlock(f->inode);
		break;
	case FD_PIPE:
		ret = pipe_read(f->pi, dst, n);
		break;
	default:
		ret = -1;
		panic("unknown file descriptor type");
		break;
	}
	return ret;
}

ssize_t file_write(struct file *f, uint64_t src, size_t n)
{
	ssize_t ret;
	size_t max, i, len;

	if (!f->writable)
		return -1;

	switch (f->type) {
	case FD_DEVICE:
		if (f->major >= N_DEV || !devlist[f->major].write)
			return -1;
		ret = devlist[f->major].write(true, src, n);
		break;
	case FD_INODE:
		/*
		 * write a few blocks at a time to avoid exceeding
		 * the maximum log transaction size, including
		 * i-node, indirect block, allocation blocks,
		 * and 2 blocks of slop for non-aligned writes.
		 * this really belongs lower down, since writei()
		 * might be writing a device like the console.
		 */
		max = ((MAX_OP_BLKS - 1 - 1 - 2) / 2) * BLOCK_SIZE;
		i = 0;
		while (i < n) {
			len = n - i;
			if (len > max)
				len = max;
			begin_op();
			ilock(f->inode);
			ret = writei(f->inode, true, src, f->off, n);
			if (ret > 0)
				f->off += ret;
			iunlock(f->inode);
			end_op();
			if (ret != len)
				break;
			i += ret;
		}
		ret = (i == n) ? n : -1;
		break;
	case FD_PIPE:
		ret = pipe_write(f->pi, src, n);
		break;
	default:
		ret = -1;
		panic("unknown file descriptor type");
		break;
	}
	return ret;
}
