#include "fs/inode.h"
#include "fs/buf.h"
#include "fs/file.h"
#include "fs/log.h"
#include "lib/string.h"
#include "mm/mm.h"
#include "param.h"
#include "printk.h"
#include "sched/cpu.h"

struct inode_table {
	struct m_inode inodes[N_INODE];
	struct spin_lock lock;
};

extern struct super_block sb;
static struct inode_table itable;

#define FIRST_INODE (&itable.inodes[0])
#define LAST_INODE (&itable.inodes[N_INODE - 1])

void iinit(void)
{
	struct m_inode *inode;
	spin_lock_init(&itable.lock, "itable");
	for (inode = FIRST_INODE; inode <= LAST_INODE; inode++)
		sleep_lock_init(&inode->lock, "inode");
}

struct m_inode *ialloc(uint32_t dev, uint16_t type)
{
	uint32_t ino, byte, shift;
	struct buffer *bitmap, *iblock;
	struct d_inode *inode;

	ino = 0;
	bitmap = bread(dev, sb.inode_bitmap_start);
	for (byte = 0; byte < BLOCK_SIZE; byte++) {
		for (shift = 0; shift < 8; shift++) {
			if ((bitmap->data[byte] & (1 << shift)) == 0 &&
			    !(byte == 0 && shift == 0)) {
				bitmap->data[byte] |= (1 << shift);
				log_write(bitmap);
				brelse(bitmap);
				ino = byte * 8 + shift;
				iblock = bread(dev, IBLOCK(ino, sb));
				inode = ((struct d_inode *)(iblock->data)) +
					(ino % IPB);
				memset(inode, 0, sizeof(*inode));
				inode->type = type;
				log_write(iblock);
				brelse(iblock);
				return iget(dev, ino);
			}
		}
	}
	brelse(bitmap);
	return NULL;
}

struct m_inode *iget(uint16_t dev, uint16_t ino)
{
	struct m_inode *empty, *inode;

	spin_lock_acquire(&itable.lock);
	empty = NULL;
	for (inode = FIRST_INODE; inode <= LAST_INODE; inode++) {
		if (inode->refcnt > 0 && inode->dev == dev &&
		    inode->ino == ino) {
			inode->refcnt++;
			spin_lock_release(&itable.lock);
			return inode;
		}
		if (empty == NULL && inode->refcnt == 0)
			empty = inode;
	}

	if (empty == NULL)
		panic("no inodes");

	inode = empty;
	inode->refcnt = 1;
	inode->dev = dev;
	inode->ino = ino;
	inode->valid = false;
	spin_lock_release(&itable.lock);
	return inode;
}

struct m_inode *idup(struct m_inode *inode)
{
	spin_lock_acquire(&itable.lock);
	if (inode->refcnt < 1)
		panic("duplicate an invalid inode");
	inode->refcnt++;
	spin_lock_release(&itable.lock);
	return inode;
}

void ilock(struct m_inode *inode)
{
	struct buffer *b;
	struct d_inode *di;

	if (inode->refcnt < 1)
		panic("lock an invalid inode");

	sleep_lock_acquire(&inode->lock);

	if (!inode->valid) {
		b = bread(inode->dev, IBLOCK(inode->ino, sb));
		di = ((struct d_inode *)b->data) + (inode->ino % IPB);
		inode->type = di->type;
		inode->major = di->major;
		inode->minor = di->minor;
		inode->nlink = di->nlink;
		inode->size = di->size;
		memmove(inode->addrs, di->addrs, sizeof(di->addrs));
		brelse(b);
		inode->valid = true;
		if (inode->type == 0)
			panic("inode has no type");
	}
}

void iunlock(struct m_inode *inode)
{
	if (inode->refcnt < 1)
		panic("unlock an invalid inode");
	sleep_lock_release(&inode->lock);
}

void iput(struct m_inode *inode)
{
	spin_lock_acquire(&itable.lock);
	if (inode->refcnt == 1 && inode->valid && inode->nlink == 0) {
		sleep_lock_acquire(&inode->lock);
		spin_lock_release(&itable.lock);
		itrunc(inode);
		inode->type = 0;
		iupdate(inode);
		inode->valid = false;
		sleep_lock_release(&inode->lock);
		spin_lock_acquire(&itable.lock);
	}
	inode->refcnt--;
	spin_lock_release(&itable.lock);
}

void iupdate(struct m_inode *inode)
{
	struct buffer *b;
	struct d_inode *di;

	b = bread(inode->dev, IBLOCK(inode->ino, sb));
	di = ((struct d_inode *)b->data) + (inode->ino % IPB);
	di->type = inode->type;
	di->major = inode->major;
	di->minor = inode->minor;
	di->nlink = inode->nlink;
	di->size = inode->size;
	memmove(di->addrs, inode->addrs, sizeof(inode->addrs));
	log_write(b);
	brelse(b);
}

static void bzero(uint32_t dev, uint32_t bno)
{
	struct buffer *b = bread(dev, bno);
	memset(b->data, 0, sizeof(b->data));
	log_write(b);
	brelse(b);
}

static uint32_t balloc(uint32_t dev)
{
	uint32_t bno, byte, shift;
	struct buffer *bitmap;

	bno = 0;
	bitmap = bread(dev, sb.data_bitmap_start);
	for (byte = 0; byte < BLOCK_SIZE; byte++) {
		for (shift = 0; shift < 8; shift++) {
			if ((bitmap->data[byte] & (1 << shift)) == 0) {
				bitmap->data[byte] |= (1 << shift);
				log_write(bitmap);
				brelse(bitmap);
				bno = byte * 8 + shift + sb.data_start;
				bzero(dev, bno);
				return bno;
			}
		}
	}
	brelse(bitmap);
	return 0;
}

static void bfree(uint32_t dev, uint32_t bno)
{
	struct buffer *b;
	uint32_t byte, shift;

	b = bread(dev, sb.data_bitmap_start);
	byte = (bno - sb.data_start) / 8;
	shift = (bno - sb.data_start) % 8;
	b->data[byte] &= (~(1 << shift));
	log_write(b);
	brelse(b);
}

static uint32_t bmap(struct m_inode *inode, uint32_t nth)
{
	struct buffer *b;
	uint32_t addr, *addrs;

	addrs = inode->addrs;
	if (nth < N_DIRECT) {
		if (addrs[nth] == 0)
			addrs[nth] = balloc(inode->dev);
		return addrs[nth];
	}

	nth -= N_DIRECT;
	addrs += N_DIRECT;
	if (nth < N_INDIRECT * APB) {
		if (addrs[nth / APB] == 0) {
			addrs[nth / APB] = balloc(inode->dev);
			if (addrs[nth / APB] == 0)
				return 0;
		}
		b = bread(inode->dev, addrs[nth / APB]);
		addrs = (uint32_t *)b->data;
		if (addrs[nth % APB] == 0) {
			addrs[nth % APB] = balloc(inode->dev);
			if (addrs[nth % APB] == 0) {
				brelse(b);
				return 0;
			}
			log_write(b);
		}
		addr = addrs[nth % APB];
		brelse(b);
		return addr;
	}

	panic("the maximum number of blocks for a single file");
}

void itrunc(struct m_inode *inode)
{
	size_t i, j;
	struct buffer *b;
	uint32_t *addrs;

	for (i = 0; i < N_DIRECT; i++) {
		if (inode->addrs[i]) {
			bfree(inode->dev, inode->addrs[i]);
			inode->addrs[i] = 0;
		}
	}

	for (; i < N_DIRECT + N_INDIRECT; i++) {
		if (inode->addrs[i]) {
			b = bread(inode->dev, inode->addrs[i]);
			addrs = (uint32_t *)(b->data);
			for (j = 0; j < APB; j++) {
				if (addrs[j])
					bfree(inode->dev, addrs[j]);
			}
			brelse(b);
			bfree(inode->dev, inode->addrs[i]);
			inode->addrs[i] = 0;
		}
	}

	inode->size = 0;
	iupdate(inode);
}

ssize_t readi(struct m_inode *inode, bool to_user, uint64_t dst, off_t off,
	      size_t n)
{
	ssize_t target, len;
	struct buffer *b;
	uint32_t addr;

	if (off > inode->size)
		return -1;

	if (off + n > inode->size)
		n = inode->size - off;

	target = n;
	while (n > 0) {
		addr = bmap(inode, off / BLOCK_SIZE);
		if (!addr)
			break;
		b = bread(inode->dev, addr);
		len = BLOCK_SIZE - off % BLOCK_SIZE;
		if (len > n)
			len = n;
		if (either_copy_out(to_user, dst, b->data + off % BLOCK_SIZE,
				    len)) {
			brelse(b);
			break;
		}
		brelse(b);
		dst += len;
		off += len;
		n -= len;
	}
	return target - n;
}

ssize_t writei(struct m_inode *inode, bool from_user, uint64_t src, off_t off,
	       size_t n)
{
	ssize_t target, len;
	struct buffer *b;
	uint32_t addr;

	if (off > inode->size)
		return -1;

	target = n;
	while (n > 0) {
		addr = bmap(inode, off / BLOCK_SIZE);
		if (!addr)
			break;
		b = bread(inode->dev, addr);
		len = BLOCK_SIZE - off % BLOCK_SIZE;
		if (len > n)
			len = n;
		if (either_copy_in(from_user, b->data + off % BLOCK_SIZE, src,
				   len)) {
			brelse(b);
			break;
		}
		log_write(b);
		brelse(b);
		off += len;
		src += len;
		n -= len;
	}

	if (off > inode->size)
		inode->size = off;
	iupdate(inode);
	return target - n;
}

void stati(struct m_inode *inode, struct stat *st)
{
	st->ino = inode->ino;
	st->type = inode->type;
	st->nlink = inode->nlink;
	st->size = inode->size;
}

off_t lseeki(struct m_inode *inode, off_t offset)
{
	if (inode->type != FT_FILE)
		return -1;
	if (bmap(inode, offset / BLOCK_SIZE) != 0)
		return offset;
	else
		return -1;
}

static char *skip_elem(char *path, char *name)
{
	char *s;
	size_t len;

	while (*path == '/')
		path++;

	if (*path == 0)
		return 0;

	s = path;
	while (*path != '/' && *path != 0)
		path++;
	len = path - s;

	if (len >= DIR_SIZE)
		memmove(name, s, DIR_SIZE);
	else {
		memmove(name, s, len);
		name[len] = 0;
	}

	while (*path == '/')
		path++;

	return path;
}

static struct m_inode *lookup(char *path, bool parent, char *name)
{
	struct m_inode *inode, *next;

	if (*path == '/')
		inode = iget(ROOT_DEV, ROOT_INO);
	else
		inode = idup(running_proc()->cwd);

	while ((path = skip_elem(path, name)) != NULL) {
		ilock(inode);
		if (inode->type != FT_DIR) {
			iunlock(inode);
			iput(inode);
			return NULL;
		}
		if (parent && *path == '\0') {
			iunlock(inode);
			return inode;
		}
		if (!(next = dir_lookup(inode, name, 0))) {
			iunlock(inode);
			iput(inode);
			return NULL;
		}
		iunlock(inode);
		iput(inode);
		inode = next;
	}
	if (parent) {
		iput(inode);
		return NULL;
	}
	return inode;
}

struct m_inode *namei(char *path)
{
	char name[DIR_SIZE];
	return lookup(path, false, name);
}

struct m_inode *parenti(char *path, char *name)
{
	return lookup(path, true, name);
}

struct m_inode *dir_lookup(struct m_inode *dir, char *name, size_t *poff)
{
	size_t off;
	struct dir_entry de;

	if (dir->type != FT_DIR)
		panic("lookup in non-directory files");

	for (off = 0; off < dir->size; off += sizeof(de)) {
		if (readi(dir, false, (uint64_t)&de, off, sizeof(de)) !=
		    sizeof(de))
			panic("inode read error");
		if (!de.ino)
			continue;
		if (strncmp(de.name, name, DIR_SIZE) == 0) {
			if (poff)
				*poff = off;
			return iget(dir->dev, de.ino);
		}
	}
	return NULL;
}

int dir_link(struct m_inode *dir, char *name, uint32_t ino)
{
	size_t off;
	struct dir_entry de;
	struct m_inode *inode;

	inode = dir_lookup(dir, name, NULL);
	if (inode) {
		iput(inode);
		return -1;
	}

	for (off = 0; off < dir->size; off += sizeof(de)) {
		if (readi(dir, false, (uint64_t)&de, off, sizeof(de)) !=
		    sizeof(de))
			panic("inode read error");
		if (!dir->ino)
			break;
	}

	strncpy(de.name, name, DIR_SIZE);
	de.ino = ino;
	if (writei(dir, false, (uint64_t)&de, off, sizeof(de)) != sizeof(de))
		return -1;
	else
		return 0;
}
