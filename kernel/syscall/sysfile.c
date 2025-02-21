#include "fs/fcntl.h"
#include "fs/file.h"
#include "fs/fs.h"
#include "fs/inode.h"
#include "fs/log.h"
#include "fs/pipe.h"
#include "lib/string.h"
#include "printk.h"
#include "riscv.h"
#include "sched/cpu.h"
#include "syscall/syscall.h"

#define LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

extern int do_execve(char *path, char **argv, char **env);

static int arg_fd(uint32_t num)
{
	int fd = ARG(num, int);
	if (fd < 0 || fd >= N_OFILE || !running_proc()->ofile[fd])
		return -1;
	else
		return fd;
}

static int fd_alloc(struct file *f)
{
	struct process *p = running_proc();
	int fd = 0;
	for (; fd < N_OFILE; fd++) {
		if (!p->ofile[fd]) {
			p->ofile[fd] = f;
			return fd;
		}
	}
	return -1;
}

uint64_t sys_read(void)
{
	int fd;
	uint64_t dst;
	size_t n;

	fd = arg_fd(0);
	if (fd < 0)
		return -1;
	dst = ARG(1, uint64_t);
	n = ARG(2, size_t);
	return file_read(running_proc()->ofile[fd], dst, n);
}

uint64_t sys_write(void)
{
	int fd;
	uint64_t src;
	size_t n;

	fd = arg_fd(0);
	if (fd < 0)
		return -1;
	src = ARG(1, uint64_t);
	n = ARG(2, size_t);
	return file_write(running_proc()->ofile[fd], src, n);
}

static struct m_inode *create(char *path, uint16_t type, uint16_t major,
			      uint16_t minor)
{
	struct m_inode *inode, *parent;
	char name[DIR_SIZE];

	parent = parenti(path, name);
	if (!parent)
		return NULL;

	ilock(parent);

	inode = dir_lookup(parent, name, NULL);
	if (inode) {
		iunlock(parent);
		iput(parent);
		ilock(inode);
		if (type == FT_FILE &&
		    (inode->type == FT_FILE || inode->type == FT_DEVICE))
			return inode;
		iunlock(inode);
		iput(inode);
		return NULL;
	}

	inode = ialloc(parent->dev, type);
	if (!inode) {
		iunlock(parent);
		iput(parent);
		return NULL;
	}

	ilock(inode);
	inode->major = major;
	inode->minor = minor;
	inode->nlink = 1;
	iupdate(inode);

	if (type == FT_DIR) {
		if (dir_link(inode, ".", inode->ino) != 0 ||
		    dir_link(inode, "..", parent->ino) != 0)
			goto fail;
	}

	if (dir_link(parent, name, inode->ino) != 0)
		goto fail;

	if (type == FT_DIR) {
		inode->nlink++;
		iupdate(inode);
	}

	iunlock(parent);
	iput(parent);
	return inode;

fail:
	inode->nlink = 0;
	iupdate(inode);
	iunlock(inode);
	iput(inode);
	iunlock(parent);
	iput(parent);
	return NULL;
}

uint64_t sys_open(void)
{
	char path[MAX_PATH];
	int fd, omode;
	struct file *f;
	struct m_inode *inode;
	off_t off;

	if (fetch_str(ARG(0, uint64_t), path, sizeof(path)))
		return -1;

	omode = ARG(1, int);

	begin_op();

	if (omode & O_CREAT) {
		inode = create(path, FT_FILE, 0, 0);
		if (!inode) {
			end_op();
			return -1;
		}
	} else {
		inode = namei(path);
		if (!inode) {
			end_op();
			return -1;
		}
		ilock(inode);
		if (inode->type == FT_DIR && omode != O_RDONLY) {
			iunlock(inode);
			iput(inode);
			end_op();
			return -1;
		}
	}

	if (inode->type == FT_DEVICE &&
	    (inode->major < 0 || inode->major >= N_DEV)) {
		iunlock(inode);
		iput(inode);
		end_op();
		return -1;
	}

	f = file_alloc();
	if (!f) {
		iunlock(inode);
		iput(inode);
		end_op();
		return -1;
	}

	fd = fd_alloc(f);
	if (fd < 0) {
		file_close(f);
		iunlock(inode);
		iput(inode);
		end_op();
		return -1;
	}

	if (inode->type == FT_DEVICE) {
		f->type = FD_DEVICE;
		f->major = inode->major;
	} else {
		f->type = FD_INODE;
		f->off = 0;
	}
	f->inode = inode;
	f->readable = !(omode & O_WRONLY);
	f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

	if ((omode & O_TRUNC) && inode->type == FT_FILE)
		itrunc(inode);

	if ((omode & O_APPEND)) {
		off = inode->size;
		if (lseeki(inode, off) != off) {
			file_close(f);
			running_proc()->ofile[fd] = NULL;
			iunlock(inode);
			iput(inode);
			end_op();
			return -1;
		}
		f->off = off;
	}

	iunlock(inode);
	end_op();

	return fd;
}

uint64_t sys_close(void)
{
	int fd;
	struct process *p;

	fd = arg_fd(0);
	if (fd < 0)
		return -1;
	p = running_proc();
	file_close(p->ofile[fd]);
	p->ofile[fd] = NULL;
	return 0;
}

uint64_t sys_mkdir(void)
{
	char path[MAX_PATH];
	struct m_inode *inode;

	if (fetch_str(ARG(0, uint64_t), path, sizeof(path)))
		return -1;
	begin_op();
	inode = create(path, FT_DIR, 0, 0);
	if (!inode)
		return -1;
	iunlock(inode);
	iput(inode);
	end_op();
	return 0;
}

uint64_t sys_mknod(void)
{
	char path[MAX_PATH];
	struct m_inode *inode;
	uint16_t major, minor;

	if (fetch_str(ARG(0, uint64_t), path, sizeof(path)))
		return -1;
	major = ARG(1, uint16_t);
	minor = ARG(2, uint16_t);
	begin_op();
	inode = create(path, FT_DEVICE, major, minor);
	if (!inode) {
		end_op();
		return -1;
	}
	iunlock(inode);
	iput(inode);
	end_op();
	return 0;
}

uint64_t sys_fstat(void)
{
	int fd;
	uint64_t pstat;

	fd = arg_fd(0);
	if (fd < 0)
		return -1;
	pstat = ARG(1, uint64_t);
	return file_stat(running_proc()->ofile[fd], pstat);
}

uint64_t sys_execve(void)
{
	char path[MAX_PATH];
	char *argv[MAX_ARGS];
	char *env[MAX_ENVS];
	int i, ret;
	uint64_t str, uargv, uenv;

	if (fetch_str(ARG(0, uint64_t), path, MAX_PATH))
		return -1;

	uargv = ARG(1, uint64_t);
	memset(argv, 0, sizeof(argv));
	for (i = 0;; i++) {
		if (i >= LEN(argv))
			goto bad;
		if (fetch_addr(uargv + sizeof(uint64_t) * i, &str))
			goto bad;
		if (!str) {
			argv[i] = NULL;
			break;
		}
		argv[i] = pm_alloc();
		if (!argv[i])
			goto bad;
		if (fetch_str(str, argv[i], PAGE_SIZE))
			goto bad;
	}

	uenv = ARG(2, uint64_t);
	memset(env, 0, sizeof(env));
	for (i = 0;; i++) {
		if (i >= LEN(env))
			goto bad;
		if (fetch_addr(uenv + sizeof(uint64_t) * i, &str))
			goto bad;
		if (!str) {
			env[i] = NULL;
			break;
		}
		env[i] = pm_alloc();
		if (!env[i])
			goto bad;
		if (fetch_str(str, env[i], PAGE_SIZE))
			goto bad;
	}

	ret = do_execve(path, argv, env);

	for (i = 0; i < LEN(argv) && argv[i] != 0; i++)
		pm_free(argv[i]);
	return ret;

bad:
	for (i = 0; i < LEN(argv) && argv[i] != 0; i++)
		pm_free(argv[i]);
	return -1;
}

uint64_t sys_chdir(void)
{
	char path[MAX_PATH];
	struct m_inode *inode;
	struct process *p;

	if (fetch_str(ARG(0, uint64_t), path, sizeof(path)))
		return -1;

	begin_op();
	inode = namei(path);
	if (!inode) {
		end_op();
		return -1;
	}

	ilock(inode);
	if (inode->type != FT_DIR) {
		iunlock(inode);
		iput(inode);
		end_op();
		return -1;
	}
	iunlock(inode);
	p = running_proc();
	iput(p->cwd);
	end_op();
	p->cwd = inode;
	return 0;
}

uint64_t sys_dup(void)
{
	int fd;
	struct file *f;

	fd = arg_fd(0);
	if (fd < 0)
		return -1;
	f = running_proc()->ofile[fd];
	fd = fd_alloc(f);
	if (fd < 0)
		return -1;
	file_dup(f);
	return 0;
}

uint64_t sys_link(void)
{
	char name[DIR_SIZE], new[MAX_PATH], old[MAX_PATH];
	struct m_inode *parent, *inode;

	if (fetch_str(ARG(0, uint64_t), old, sizeof(old)))
		return -1;

	if (fetch_str(ARG(1, uint64_t), new, sizeof(new)))
		return -1;

	begin_op();

	inode = namei(old);
	if (!inode) {
		end_op();
		return -1;
	}

	ilock(inode);

	if (inode->type == FT_DIR) {
		iunlock(inode);
		iput(inode);
		end_op();
		return -1;
	}

	inode->nlink++;
	iupdate(inode);
	iunlock(inode);

	parent = parenti(new, name);
	if (!parent)
		goto bad;
	ilock(parent);
	if (dir_link(parent, name, inode->ino)) {
		iunlock(parent);
		iput(parent);
		goto bad;
	}
	iunlock(parent);
	iput(parent);
	iput(inode);

	end_op();

	return 0;

bad:
	ilock(inode);
	inode->nlink--;
	iupdate(inode);
	iunlock(inode);
	iput(inode);
	end_op();
	return -1;
}

static bool is_dir_empty(struct m_inode *dir)
{
	off_t off;
	struct dir_entry de;

	for (off = 2 * sizeof(de); off < dir->size; off += sizeof(de)) {
		if (readi(dir, 0, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
			panic("inode read error");
		if (de.ino != 0)
			return false;
	}
	return true;
}

uint64_t sys_unlink(void)
{
	struct m_inode *inode, *parent;
	struct dir_entry de;
	char name[DIR_SIZE], path[MAX_PATH];
	size_t off;

	if (fetch_str(ARG(0, uint64_t), path, sizeof(path)))
		return -1;

	begin_op();

	parent = parenti(path, name);
	if (!parent) {
		end_op();
		return -1;
	}

	ilock(parent);
	if (!strncmp(name, ".", DIR_SIZE) || !strncmp(name, "..", DIR_SIZE))
		goto bad;

	inode = dir_lookup(parent, name, &off);
	if (!inode)
		goto bad;

	ilock(inode);
	if (inode->nlink < 1)
		panic("unlink an invalid inode");
	if (inode->type == FT_DIR && !is_dir_empty(inode)) {
		iunlock(inode);
		iput(inode);
		goto bad;
	}

	memset(&de, 0, sizeof(de));
	if (writei(parent, false, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
		panic("inode write error");
	if (inode->type == FT_DIR) {
		/* for '..' */
		parent->nlink--;
		iupdate(parent);
	}
	iunlock(parent);
	iput(parent);

	inode->nlink--;
	iupdate(inode);
	iunlock(inode);
	iput(inode);

	end_op();

	return 0;

bad:
	iunlock(parent);
	iput(parent);
	end_op();
	return -1;
}

uint64_t sys_pipe(void)
{
	uint64_t fd_array;
	struct file *rfile, *wfile;
	int fd0 = -1;
	int fd1 = -1;
	struct process *p = running_proc();

	fd_array = ARG(0, uint64_t);
	if (pipe_alloc(&rfile, &wfile))
		return -1;

	fd0 = fd_alloc(rfile);
	fd1 = fd_alloc(wfile);
	if (fd0 < 0 || fd1 < 0)
		goto bad;

	if (copy_out(p->page_table, fd_array, &fd0, sizeof(fd0)) ||
	    copy_out(p->page_table, fd_array + sizeof(fd0), &fd1, sizeof(fd1)))
		goto bad;

	return 0;

bad:
	if (fd0 >= 0)
		p->ofile[fd0] = NULL;
	if (fd1 >= 0)
		p->ofile[fd1] = NULL;
	file_close(rfile);
	file_close(wfile);
	return -1;
}

uint64_t sys_lseek(void)
{
	int fd;
	off_t offset;
	int whence;

	fd = arg_fd(0);
	if (fd < 0)
		return -1;
	offset = ARG(1, off_t);
	whence = ARG(2, int);
	return file_lseek(running_proc()->ofile[fd], offset, whence);
}

uint64_t sys_dup2(void)
{
	int oldfd, newfd;
	struct process *p;

	oldfd = arg_fd(0);
	if (oldfd < 0)
		return -1;

	p = running_proc();
	newfd = arg_fd(1);
	if (newfd < 0) {
		p->ofile[newfd] = file_dup(p->ofile[oldfd]);
		return newfd;
	} else {
		if (p->ofile[oldfd] == p->ofile[newfd]) {
			return newfd;
		} else {
			file_close(p->ofile[newfd]);
			p->ofile[newfd] = file_dup(p->ofile[oldfd]);
			return newfd;
		}
	}
}
