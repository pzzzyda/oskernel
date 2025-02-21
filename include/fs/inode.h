#ifndef _INODE_H
#define _INODE_H

#include "fs/fs.h"
#include "fs/stat.h"
#include "lock.h"

struct m_inode {
	uint32_t dev;	 /* Device number */
	uint16_t ino;	 /* Inode number */
	uint32_t refcnt; /* Reference count */

	struct sleep_lock lock;

	bool valid; /* inode has been read from disk? */

	/* on disk */
	uint16_t type;		 /* File type */
	uint16_t major;		 /* Major device number (FT_DEVICE only) */
	uint16_t minor;		 /* Minor device number (FT_DEVICE only) */
	uint16_t nlink;		 /* Number of links to inode in file system */
	uint32_t size;		 /* Size of file (bytes) */
	uint32_t addrs[N_ADDRS]; /* Data block addresses */
};

void iinit(void);
struct m_inode *ialloc(uint32_t dev, uint16_t type);
struct m_inode *iget(uint16_t dev, uint16_t ino);
struct m_inode *idup(struct m_inode *inode);
void ilock(struct m_inode *inode);
void iunlock(struct m_inode *inode);
void iput(struct m_inode *inode);
void iupdate(struct m_inode *inode);
void itrunc(struct m_inode *inode);
ssize_t readi(struct m_inode *inode, bool to_user, uint64_t dst, off_t off,
	      size_t n);
ssize_t writei(struct m_inode *inode, bool from_user, uint64_t src, off_t off,
	       size_t n);
void stati(struct m_inode *inode, struct stat *st);
off_t lseeki(struct m_inode *inode, off_t offset);
struct m_inode *namei(char *path);
struct m_inode *parenti(char *path, char *name);
struct m_inode *dir_lookup(struct m_inode *dir, char *name, size_t *poff);
int dir_link(struct m_inode *dir, char *name, uint32_t ino);

#endif
