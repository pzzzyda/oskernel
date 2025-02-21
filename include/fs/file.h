#ifndef _FILE_H
#define _FILE_H

#include "types.h"

#define SEEK_SET 0 /* Seek from beginning of file.  */
#define SEEK_CUR 1 /* Seek from current position.  */
#define SEEK_END 2 /* Seek from end of file.  */

struct m_inode;
struct pipe;

#define FD_NONE 0
#define FD_INODE 1
#define FD_DEVICE 2
#define FD_PIPE 3

struct file {
	uint16_t type;
	bool readable;
	bool writable;
	uint32_t refcnt;
	struct m_inode *inode; /* for FD_INODE or FD_DEVICE */
	struct pipe *pi;       /* for FD_PIPE */
	uint32_t off;	       /* for FD_INODE */
	uint16_t major;	       /* for FD_DEVICE */
};

struct device {
	ssize_t (*read)(bool to_user, uint64_t dst, size_t n);
	ssize_t (*write)(bool from_user, uint64_t src, size_t n);
};

#define CONSOLE 1

void file_init(void);
struct file *file_alloc(void);
void file_close(struct file *f);
struct file *file_dup(struct file *f);
int file_stat(struct file *f, uint64_t pstat);
off_t file_lseek(struct file *f, off_t offset, int whence);
ssize_t file_read(struct file *f, uint64_t dst, size_t n);
ssize_t file_write(struct file *f, uint64_t src, size_t n);

#endif
