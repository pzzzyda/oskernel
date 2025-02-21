#include "fs/buf.h"
#include "lib/string.h"
#include "lock.h"
#include "memlayout.h"
#include "printk.h"
#include "riscv.h"
#include "sched/proc.h"

/* From qemu virtio_mmio.h */

/* Magic value ("virt" string) - Read Only */
#define VIRTIO_MMIO_MAGIC_VALUE 0x000
/* Virtio device version - Read Only */
#define VIRTIO_MMIO_VERSION 0x004
/* Virtio device ID - Read Only */
#define VIRTIO_MMIO_DEVICE_ID 0x008
/* Virtio vendor ID - Read Only */
#define VIRTIO_MMIO_VENDOR_ID 0x00c
/* Bitmask of the features supported by the device (host)
 * (32 bits per set) - Read Only */
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
/* Bitmask of features activated by the driver (guest)
 * (32 bits per set) - Write Only */
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
/* Guest's memory page size in bytes - Write Only */
#define VIRTIO_MMIO_GUEST_PAGE_SIZE 0x028
/* Queue selector - Write Only */
#define VIRTIO_MMIO_QUEUE_SEL 0x030
/* Maximum size of the currently selected queue - Read Only */
#define VIRTIO_MMIO_QUEUE_NUM_MAX 0x034
/* Queue size for the currently selected queue - Write Only */
#define VIRTIO_MMIO_QUEUE_NUM 0x038
/* Guest's PFN for the currently selected queue - Read Write */
#define VIRTIO_MMIO_QUEUE_PFN 0x040
/* Ready bit for the currently selected queue - Read Write */
#define VIRTIO_MMIO_QUEUE_READY 0x044
/* Queue notifier - Write Only */
#define VIRTIO_MMIO_QUEUE_NOTIFY 0x050
/* Interrupt status - Read Only */
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
/* Interrupt acknowledge - Write Only */
#define VIRTIO_MMIO_INTERRUPT_ACK 0x064
/* Device status register - Read Write */
#define VIRTIO_MMIO_STATUS 0x070

/* From qemu virtio_config.h */

/* Status byte for guest to report progress, and synchronize features. */
/* We have seen device and processed generic fields (VIRTIO_CONFIG_F_VIRTIO) */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
/* We have found a driver for the device. */
#define VIRTIO_CONFIG_S_DRIVER 2
/* Driver has used its parts of the config, and is happy */
#define VIRTIO_CONFIG_S_DRIVER_OK 4
/* Driver has finished configuring features */
#define VIRTIO_CONFIG_S_FEATURES_OK 8
/* Can the device handle any descriptor layout? */
#define VIRTIO_F_ANY_LAYOUT 27

/* From qemu virtio_blk.h */

/* Feature bits */
#define VIRTIO_BLK_F_RO 5	   /* Disk is read-only */
#define VIRTIO_BLK_F_SCSI 7	   /* Supports scsi command passthru */
#define VIRTIO_BLK_F_CONFIG_WCE 11 /* Writeback mode available in config */
#define VIRTIO_BLK_F_MQ 12	   /* support more than one vq */

/* From qemu virtio_ring.h */

/* We support indirect buffer descriptors */
#define VIRTIO_RING_F_INDIRECT_DESC 28
/* The Guest publishes the used index for which it expects an interrupt
 * at the end of the avail ring. Host should ignore the avail->flags field. */
/* The Host publishes the avail index for which it expects a kick
 * at the end of the used ring. Guest should ignore the used->flags field. */
#define VIRTIO_RING_F_EVENT_IDX 29

/* This many virtio descriptors.
 * Must be a power of two. */
#define NUM 8

struct virtq_desc {
	uint64_t addr;
	uint32_t len;
	uint16_t flags;
	uint16_t next;
};

/* From qemu virtio_ring.h */

/* This marks a buffer as continuing via the next field. */
#define VRING_DESC_F_NEXT 1
/* This marks a buffer as write-only (otherwise read-only). */
#define VRING_DESC_F_WRITE 2

struct virtq_avail {
	uint16_t flags;
	uint16_t idx;
	uint16_t ring[NUM];
};

struct virtq_used_elem {
	uint32_t id;
	uint32_t len;
};

struct virtq_used {
	uint16_t flags;
	uint16_t idx;
	struct virtq_used_elem ring[NUM];
};

/* From qemu virtio_blk.h */

/* These two define direction. */
#define VIRTIO_BLK_T_IN 0  /* read the disk */
#define VIRTIO_BLK_T_OUT 1 /* write the disk */

struct virtio_blk_req {
	uint32_t type;
	uint32_t reserved;
	uint64_t sector;
};

#define REG(r) ((volatile uint32_t *)(VIRTIO0 + (r)))
#define READ_REG(r) (*(REG(r)))
#define WRITE_REG(r, v) (*(REG(r)) = (v))

struct virtio_disk {
	/* memory for virtio descriptors &c for queue 0.
	 * this is a global instead of allocated because it must
	 * be multiple contiguous pages, which pm_alloc()
	 * doesn't support, and page aligned. */
	uint8_t virtq_mem[2 * PAGE_SIZE];

	/* a set (not a ring) of DMA descriptors, with which the
	 * driver tells the device where to read and write individual
	 * disk operations. there are NUM descriptors.
	 * most commands consist of a "chain" (a linked list) of a couple of
	 * these descriptors. */
	struct virtq_desc *desc;

	/* a ring in which the driver writes descriptor numbers
	 * that the driver would like the device to process.  it only
	 * includes the head descriptor of each chain. the ring has
	 * NUM elements. */
	struct virtq_avail *avail;

	/* a ring in which the device writes descriptor numbers that
	 * the device has finished processing (just the head of each chain).
	 * there are NUM used ring entries. */
	struct virtq_used *used;

	bool free[NUM];	   /* is descriptor free? */
	uint16_t used_idx; /* we've looked this far in used[2..NUM]. */

	/* track info about in-flight operations,
	 * for use when completion interrupt arrives.
	 * indexed by first descriptor index of chain. */
	struct virtio_disk_track_info {
		struct buffer *b;
		char status;
	} info[NUM];

	/* disk command headers.
	 * one-for-one with descriptors, for convenience. */
	struct virtio_blk_req ops[NUM];

	struct spin_lock lock;
} __attribute__((aligned(PAGE_SIZE)));

static struct virtio_disk disk;

void virtio_disk_init(void)
{
	uint32_t status, max_q_size;
	uint64_t features;

	spin_lock_init(&disk.lock, "virtio_disk");

	if (READ_REG(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
	    READ_REG(VIRTIO_MMIO_VERSION) != 1 ||
	    READ_REG(VIRTIO_MMIO_DEVICE_ID) != 2 ||
	    READ_REG(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551)
		panic("could not find virtio disk");

	/* reset device */
	status = 0;
	WRITE_REG(VIRTIO_MMIO_STATUS, status);

	/* set ACKNOWLEDGE status bit */
	status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
	WRITE_REG(VIRTIO_MMIO_STATUS, status);

	/* set DRIVER status bit */
	status |= VIRTIO_CONFIG_S_DRIVER;
	WRITE_REG(VIRTIO_MMIO_STATUS, status);

	/* negotiate features */
	features = READ_REG(VIRTIO_MMIO_DEVICE_FEATURES);
	features &= ~(1 << VIRTIO_BLK_F_RO);
	features &= ~(1 << VIRTIO_BLK_F_SCSI);
	features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
	features &= ~(1 << VIRTIO_BLK_F_MQ);
	features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
	features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
	features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
	WRITE_REG(VIRTIO_MMIO_DRIVER_FEATURES, features);

	/* tell device that features negotiate is complate */
	status |= VIRTIO_CONFIG_S_FEATURES_OK;
	WRITE_REG(VIRTIO_MMIO_STATUS, status);

	/* ensure FEATURES_OK is set */
	status = READ_REG(VIRTIO_MMIO_STATUS);
	if ((status & VIRTIO_CONFIG_S_FEATURES_OK) == 0)
		panic("virtio disk FEATURES_OK unset");

	/* initialize queue 0 */
	WRITE_REG(VIRTIO_MMIO_QUEUE_SEL, 0);

	/* ensure queue 0 is not in use */
	if (READ_REG(VIRTIO_MMIO_QUEUE_READY) != 0)
		panic("virtio disk should not be ready");

	WRITE_REG(VIRTIO_MMIO_GUEST_PAGE_SIZE, PAGE_SIZE);

	/* check maximum queue size */
	max_q_size = READ_REG(VIRTIO_MMIO_QUEUE_NUM_MAX);
	if (max_q_size == 0)
		panic("virtio disk has no queue 0");
	if (max_q_size < NUM)
		panic("virtio disk max queue too short");

	/* allocate and zero queue memory. */
	disk.desc = (struct virtq_desc *)(disk.virtq_mem);
	disk.avail = (struct virtq_avail *)(disk.virtq_mem +
					    NUM * sizeof(struct virtq_desc));
	disk.used = (struct virtq_used *)(disk.virtq_mem + PAGE_SIZE);
	memset(disk.virtq_mem, 0, sizeof(disk.virtq_mem));

	/* set queue size */
	WRITE_REG(VIRTIO_MMIO_QUEUE_NUM, NUM);

	/* write physical addresses */
	WRITE_REG(VIRTIO_MMIO_QUEUE_PFN, ((uint64_t)disk.virtq_mem) >> 12);

	/* queue is ready */
	WRITE_REG(VIRTIO_MMIO_QUEUE_READY, 0x1);

	/* all NUM descriptors start out unused */
	memset(disk.free, true, sizeof(disk.free));

	/* tell device we're completely ready */
	status |= VIRTIO_CONFIG_S_DRIVER_OK;
	WRITE_REG(VIRTIO_MMIO_STATUS, status);
}

/* find a free descriptor, mark it non-free, return its index. */
static int alloc_desc(void)
{
	int i;
	for (i = 0; i < NUM; i++) {
		if (disk.free[i]) {
			disk.free[i] = false;
			return i;
		}
	}
	return -1;
}

/* mark a descriptor as free. */
static void free_desc(int i)
{
	if (i < 0 || i >= NUM)
		panic("free an invalid descriptor");
	if (disk.free[i])
		panic("the descriptor is already free");
	disk.desc[i].addr = 0;
	disk.desc[i].len = 0;
	disk.desc[i].flags = 0;
	disk.desc[i].next = 0;
	disk.free[i] = true;
	wake_up(&disk.free[0]);
}

/* free a chain of descriptors. */
static void free_chain(int i)
{
	uint16_t flag, next;
	while (true) {
		flag = disk.desc[i].flags;
		next = disk.desc[i].next;
		free_desc(i);
		if (flag & VRING_DESC_F_NEXT)
			i = next;
		else
			break;
	}
}

/*
 * allocate three descriptors (they need not be contiguous).
 * disk transfers always use three descriptors.
 */
static int alloc3_desc(int *idx)
{
	int i, j;
	for (i = 0; i < 3; i++) {
		idx[i] = alloc_desc();
		if (idx[i] == -1) {
			for (j = 0; j < i; j++)
				free_desc(idx[j]);
			return -1;
		}
	}
	return 0;
}

static void virtio_disk_rw(struct buffer *b, bool is_write)
{
	int idx[3];
	uint64_t sector;
	struct virtio_blk_req *req;

	spin_lock_acquire(&disk.lock);

	sector = b->bno * (BLOCK_SIZE / 512);

	/*
	 * the spec's Section 5.2 says that legacy block operations use
	 * three descriptors: one for type/reserved/sector, one for the
	 * data, one for a 1-byte status result.
	 */
	while (true) {
		if (alloc3_desc(idx) == 0)
			break;
		sleep_on(&disk.free[0], &disk.lock);
	}

	req = &disk.ops[idx[0]];

	if (is_write)
		req->type = VIRTIO_BLK_T_OUT;
	else
		req->type = VIRTIO_BLK_T_IN;
	req->reserved = 0;
	req->sector = sector;

	disk.desc[idx[0]].addr = (uint64_t)req;
	disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
	disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
	disk.desc[idx[0]].next = idx[1];

	disk.desc[idx[1]].addr = (uint64_t)b->data;
	disk.desc[idx[1]].len = BLOCK_SIZE;
	if (is_write)
		disk.desc[idx[1]].flags = 0;
	else
		disk.desc[idx[1]].flags = VRING_DESC_F_WRITE;
	disk.desc[idx[1]].flags |= VRING_DESC_F_NEXT;
	disk.desc[idx[1]].next = idx[2];

	disk.info[idx[0]].status = 0xff;
	disk.desc[idx[2]].addr = (uint64_t)(&(disk.info[idx[0]].status));
	disk.desc[idx[2]].len = 1;
	disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
	disk.desc[idx[2]].next = 0;

	b->disk = true;
	disk.info[idx[0]].b = b;

	disk.avail->ring[disk.avail->idx % NUM] = idx[0];

	__sync_synchronize();

	disk.avail->idx += 1;

	__sync_synchronize();

	WRITE_REG(VIRTIO_MMIO_QUEUE_NOTIFY, 0);

	while (b->disk)
		sleep_on(b, &disk.lock);

	disk.info[idx[0]].b = NULL;
	free_chain(idx[0]);

	spin_lock_release(&disk.lock);
}

void virtio_disk_intr(void)
{
	int id;
	struct buffer *b;

	spin_lock_acquire(&disk.lock);

	WRITE_REG(VIRTIO_MMIO_INTERRUPT_ACK,
		  READ_REG(VIRTIO_MMIO_INTERRUPT_STATUS) & 0x3);

	__sync_synchronize();

	while (disk.used_idx != disk.used->idx) {
		__sync_synchronize();
		id = disk.used->ring[disk.used_idx % NUM].id;

		if (disk.info[id].status != 0)
			panic("virtio disk interrupt status");

		b = disk.info[id].b;
		b->disk = false;
		wake_up(b);

		disk.used_idx += 1;
	}

	spin_lock_release(&disk.lock);
}

void virtio_disk_read(struct buffer *b)
{
	virtio_disk_rw(b, false);
}

void virtio_disk_write(struct buffer *b)
{
	virtio_disk_rw(b, true);
}
