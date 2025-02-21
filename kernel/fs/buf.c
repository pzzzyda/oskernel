#include "fs/buf.h"
#include "dev/virtio_disk.h"
#include "lock.h"
#include "param.h"
#include "printk.h"

struct buffer_cache {
	struct spin_lock lock;
	struct buffer buf[N_BUF];

	/*
	 * Linked list of all buffers, through prev/next.
	 * Sorted by how recently the buffer was used.
	 * head.next is most recent, head.prev is least.
	 */
	struct buffer head;
};

static struct buffer_cache bcache;

#define FIRST_BUF (&bcache.buf[0])
#define LAST_BUF (&bcache.buf[N_BUF - 1])

void binit(void)
{
	struct buffer *b;

	spin_lock_init(&bcache.lock, "bcache");
	/* Create linked list of buffers. */
	bcache.head.prev = &bcache.head;
	bcache.head.next = &bcache.head;
	for (b = FIRST_BUF; b <= LAST_BUF; b++) {
		sleep_lock_init(&b->lock, "buf");
		b->refcnt = 0;
		b->valid = false;
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}
}

/* Return a locked buffer with the contents of the indicated block. */
struct buffer *bread(uint32_t dev, uint32_t bno)
{
	struct buffer *b;

	spin_lock_acquire(&bcache.lock);

	/* Is the block already cached? */
	for (b = bcache.head.next; b != &bcache.head; b = b->next) {
		if (b->dev == dev && b->bno == bno) {
			b->refcnt++;
			spin_lock_release(&bcache.lock);
			sleep_lock_acquire(&b->lock);
			return b;
		}
	}

	/*
	 * Not cached.
	 * Recycle the least recently used (LRU) unused buffer.
	 */
	for (b = bcache.head.prev; b != &bcache.head; b = b->prev) {
		if (b->refcnt == 0) {
			b->dev = dev;
			b->bno = bno;
			b->valid = false;
			b->refcnt = 1;
			spin_lock_release(&bcache.lock);
			sleep_lock_acquire(&b->lock);
			virtio_disk_read(b);
			b->valid = true;
			return b;
		}
	}

	panic("no buffers");
}

/*
 * Write buffer's contents to disk.
 * Must be locked.
 */
void bwrite(struct buffer *b)
{
	if (!sleep_lock_holding(&b->lock))
		panic("write an unlocked buffer");
	virtio_disk_write(b);
}

/*
 * Release a locked buffer.
 * Move to the head of the most-recently-used list.
 */
void brelse(struct buffer *b)
{
	sleep_lock_release(&b->lock);
	spin_lock_acquire(&bcache.lock);
	b->refcnt--;
	if (b->refcnt == 0) {
		/* No one is waiting for it. */
		b->next->prev = b->prev;
		b->prev->next = b->next;
		b->next = bcache.head.next;
		b->prev = &bcache.head;
		bcache.head.next->prev = b;
		bcache.head.next = b;
	}
	spin_lock_release(&bcache.lock);
}

void bpin(struct buffer *b)
{
	spin_lock_acquire(&bcache.lock);
	if (b->refcnt < 1)
		panic("pin an invalid buffer");
	b->refcnt++;
	spin_lock_release(&bcache.lock);
}

void bunpin(struct buffer *b)
{
	spin_lock_acquire(&bcache.lock);
	if (b->refcnt < 1)
		panic("unpin an invalid buffer");
	b->refcnt--;
	spin_lock_release(&bcache.lock);
}
