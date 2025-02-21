#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fs/fs.h"
#include "param.h"

#define FT_NONE 0
#define FT_DIR 1
#define FT_FILE 2
#define FT_DEVICE 3

/*
 * layout:
 * [boot block | super block | log | inode bitmap | inode | data bitmap | data]
 */

static const uint32_t n_log_blks = LOG_SIZE;
static const uint32_t n_inode_blks = (BLOCK_SIZE * 8) / IPB;
static const uint32_t n_data_blks = BLOCK_SIZE * 8;

static const uint32_t n_blks =
	1 + 1 + n_log_blks + 1 + n_inode_blks + 1 + n_data_blks;

static const uint32_t log_start = 1 + 1;
static const uint32_t inode_bitmap_start = log_start + n_log_blks;
static const uint32_t inode_start = inode_bitmap_start + 1;
static const uint32_t data_bitmap_start = inode_start + n_inode_blks;
static const uint32_t data_start = data_bitmap_start + 1;

static int fs_fd;
static struct super_block sb;
static struct d_inode root_inode;
static uint16_t root_ino;
static uint32_t root_block;

static uint16_t mkfs_xuint16(uint16_t x)
{
	uint16_t y;
	uint8_t *a = (uint8_t *)&y;
	a[0] = x;
	a[1] = x >> 8;
	return y;
}

static uint32_t mkfs_xuint32(uint32_t x)
{
	uint32_t y;
	uint8_t *a = (uint8_t *)&y;
	a[0] = x;
	a[1] = x >> 8;
	a[2] = x >> 16;
	a[3] = x >> 24;
	return y;
}

static void mkfs_read_block(uint32_t bno, void *buf)
{
	if (lseek(fs_fd, BLOCK_SIZE * bno, SEEK_SET) != BLOCK_SIZE * bno) {
		perror("lseek");
		exit(1);
	}
	if (read(fs_fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
		perror("read");
		exit(1);
	}
}

static void mkfs_write_block(uint32_t bno, void *buf)
{
	if (lseek(fs_fd, BLOCK_SIZE * bno, SEEK_SET) != BLOCK_SIZE * bno) {
		perror("lseek");
		exit(1);
	}
	if (write(fs_fd, buf, BLOCK_SIZE) != BLOCK_SIZE) {
		perror("write");
		exit(1);
	}
}

static uint32_t mkfs_block_alloc(void)
{
	uint8_t buf[BLOCK_SIZE];
	uint32_t byte, shift;

	mkfs_read_block(sb.data_bitmap_start, buf);
	for (byte = 0; byte < BLOCK_SIZE; byte++) {
		for (shift = 0; shift <= 7; shift++) {
			if (!(buf[byte] & (1 << shift))) {
				buf[byte] |= (1 << shift);
				mkfs_write_block(sb.data_bitmap_start, buf);
				return byte * 8 + shift + sb.data_start;
			}
		}
	}
	puts("no free block");
	exit(1);
}

static uint16_t mkfs_inode_alloc(void)
{
	uint8_t buf[BLOCK_SIZE];
	uint32_t byte, shift;

	mkfs_read_block(sb.inode_bitmap_start, buf);
	for (byte = 0; byte < BLOCK_SIZE; byte++) {
		for (shift = 0; shift <= 7; shift++) {
			if ((byte || shift) && !(buf[byte] & (1 << shift))) {
				buf[byte] |= (1 << shift);
				mkfs_write_block(sb.inode_bitmap_start, buf);
				return byte * 8 + shift;
			}
		}
	}
	puts("no free inode");
	exit(1);
}

static void mkfs_write_inode(uint16_t ino, struct d_inode *inode)
{
	uint8_t buf[BLOCK_SIZE];
	mkfs_read_block(IBLOCK(ino, sb), buf);
	memmove(((struct d_inode *)buf) + (ino % IPB), inode, sizeof(*inode));
	mkfs_write_block(IBLOCK(ino, sb), buf);
}

static void mkfs_update_inode(uint16_t ino, struct d_inode *inode)
{
	int i;
	inode->size = mkfs_xuint32(inode->size);
	for (i = 0; i < N_ADDRS; i++)
		inode->addrs[i] = mkfs_xuint32(inode->addrs[i]);
	mkfs_write_inode(ino, inode);
}

static void mkfs_create_inode(struct d_inode *inode, uint16_t ino,
			      uint16_t type)
{
	int i;
	inode->type = mkfs_xuint16(type);
	inode->major = mkfs_xuint16(0);
	inode->minor = mkfs_xuint16(0);
	inode->nlink = mkfs_xuint16(1);
	inode->size = mkfs_xuint32(0);
	for (i = 0; i < N_ADDRS; i++)
		inode->addrs[i] = mkfs_xuint32(0);
	mkfs_write_inode(ino, inode);
}

static uint32_t mkfs_block_map(struct d_inode *inode, uint32_t n)
{
	uint8_t buf[BLOCK_SIZE];
	uint32_t *addrs;

	addrs = inode->addrs;
	if (n < N_DIRECT) {
		if (addrs[n] == 0)
			addrs[n] = mkfs_block_alloc();
		return addrs[n];
	}

	n -= N_DIRECT;
	addrs += N_DIRECT;

	if (n < N_INDIRECT * APB) {
		if (addrs[n / APB] == 0)
			addrs[n / APB] = mkfs_block_alloc();
		mkfs_read_block(addrs[n / APB], buf);
		if (((uint32_t *)buf)[n % APB] == 0) {
			((uint32_t *)buf)[n % APB] = mkfs_block_alloc();
			mkfs_write_block(addrs[n / APB], buf);
		}
		return ((uint32_t *)buf)[n % APB];
	}

	puts("block overflow");
	exit(1);

	return 0;
}

static void mkfs_init_sb(void)
{
	sb.block_size = mkfs_xuint32(BLOCK_SIZE);
	sb.n_log_blks = mkfs_xuint32(n_log_blks);
	sb.n_inode_blks = mkfs_xuint32(n_inode_blks);
	sb.n_data_blks = mkfs_xuint32(n_data_blks);
	sb.log_start = mkfs_xuint32(log_start);
	sb.inode_bitmap_start = mkfs_xuint32(inode_bitmap_start);
	sb.inode_start = mkfs_xuint32(inode_start);
	sb.data_bitmap_start = mkfs_xuint32(data_bitmap_start);
	sb.data_start = mkfs_xuint32(data_start);
}

static uint32_t mkfs_create_dir_entry(uint32_t off, char *name, uint16_t ino)
{
	uint8_t buf[BLOCK_SIZE];
	struct dir_entry de;

	strncpy(de.name, name, DIR_SIZE);
	de.ino = mkfs_xuint16(ino);
	mkfs_read_block(root_block, buf);
	memmove(buf + off, &de, sizeof(de));
	mkfs_write_block(root_block, buf);
	return off + sizeof(de);
}

int main(int argc, char **argv)
{
	int fd, i;
	char *short_name;
	struct d_inode inode;
	uint16_t ino;
	ssize_t len;
	uint8_t buf[BLOCK_SIZE];
	uint32_t off, entry_count, nth, bno;

	if (argc < 2) {
		fprintf(stderr, "usage: %s [image name] [files...]\n", argv[0]);
		exit(1);
	}

	fs_fd = open(argv[1], O_RDWR | O_CREAT | O_TRUNC, 0666);
	if (fs_fd < 0) {
		perror("open");
		exit(1);
	}

	mkfs_init_sb();

	memset(buf, 0, sizeof(buf));
	for (i = 0; i < n_blks; i++)
		mkfs_write_block(i, buf);

	memmove(buf, &sb, sizeof(sb));
	mkfs_write_block(1, buf);

	root_ino = mkfs_inode_alloc();
	assert(root_ino == ROOT_INO);
	root_block = mkfs_block_alloc();
	mkfs_create_inode(&root_inode, root_ino, FT_DIR);

	off = 0;
	entry_count = 0;
	off = mkfs_create_dir_entry(off, ".", ROOT_INO);
	off = mkfs_create_dir_entry(off, "..", ROOT_INO);
	entry_count += 2;
	for (i = 2; i < argc; i++) {
		if (off >= BLOCK_SIZE) {
			puts("only support one block");
			break;
		}
		short_name = strrchr(argv[i], '/');
		if (!short_name)
			short_name = argv[i];
		else
			short_name++;
		while (*short_name == '_')
			short_name++;
		fprintf(stdout, "copy %s to /%s\n", argv[i], short_name);
		ino = mkfs_inode_alloc();
		mkfs_create_inode(&inode, ino, FT_FILE);
		off = mkfs_create_dir_entry(off, short_name, ino);
		fd = open(argv[i], O_RDONLY);
		if (fd < 0) {
			perror("open");
			exit(1);
		}
		nth = 0;
		while (1) {
			memset(buf, 0, BLOCK_SIZE);
			len = read(fd, buf, BLOCK_SIZE);
			bno = mkfs_block_map(&inode, nth++);
			mkfs_write_block(bno, buf);
			inode.size += len;
			if (len < BLOCK_SIZE)
				break;
		}
		close(fd);
		mkfs_update_inode(ino, &inode);
		entry_count++;
	}
	root_inode.addrs[0] = root_block;
	root_inode.size = entry_count * sizeof(struct dir_entry);
	mkfs_update_inode(root_ino, &root_inode);
	close(fs_fd);
	return 0;
}
