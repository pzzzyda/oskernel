#ifndef _FS_H
#define _FS_H

#include "types.h"

#define BLOCK_SIZE 1024

#define ROOT_INO 1

#define N_DIRECT 10
#define N_INDIRECT 3
#define N_ADDRS (N_DIRECT + N_INDIRECT)

#define DIR_SIZE 30

/* Inodes per block */
#define IPB (BLOCK_SIZE / sizeof(struct d_inode))

/* Block containing inode i */
#define IBLOCK(i, sb) ((i) / IPB + sb.inode_start)

/* Addrs per block */
#define APB (BLOCK_SIZE / sizeof(uint32_t))

struct super_block {
	uint32_t block_size;
	uint32_t n_log_blks;
	uint32_t n_inode_blks;
	uint32_t n_data_blks;
	uint32_t log_start;
	uint32_t inode_bitmap_start;
	uint32_t inode_start;
	uint32_t data_bitmap_start;
	uint32_t data_start;
};

struct d_inode {
	uint16_t type;		 /* File type */
	uint16_t major;		 /* Major device number (FT_DEVICE only) */
	uint16_t minor;		 /* Minor device number (FT_DEVICE only) */
	uint16_t nlink;		 /* Number of links to inode in file system */
	uint32_t size;		 /* Size of file (bytes) */
	uint32_t addrs[N_ADDRS]; /* Data block addresses */
};

struct dir_entry {
	uint16_t ino; /* Inode number */
	char name[DIR_SIZE];
};

void fs_init(uint32_t dev);

#endif
