#include "fs/log.h"
#include "fs/buf.h"
#include "fs/fs.h"
#include "lib/string.h"
#include "lock.h"
#include "param.h"
#include "printk.h"
#include "sched/proc.h"

struct log_header {
	uint32_t n;
	uint32_t blocks[LOG_SIZE];
};

struct log {
	struct spin_lock lock;
	uint32_t start;
	uint32_t size;
	int outstanding;
	bool committing;
	uint32_t dev;
	struct log_header lh;
};

static struct log lg;

static void recover_from_log(void);
static void commit(void);

void log_init(uint32_t dev, struct super_block *sb)
{
	if (sizeof(struct log_header) > BLOCK_SIZE)
		panic("too big log header");

	spin_lock_init(&lg.lock, "log");
	lg.start = sb->log_start;
	lg.size = sb->n_log_blks;
	lg.dev = dev;
	recover_from_log();
}

static void install_trans(bool recovering)
{
	uint32_t i;
	struct buffer *from, *to;

	for (i = 0; i < lg.lh.n; i++) {
		from = bread(lg.dev, lg.start + 1 + i);
		to = bread(lg.dev, lg.lh.blocks[i]);
		memmove(to->data, from->data, BLOCK_SIZE);
		bwrite(to);
		if (!recovering)
			bunpin(to);
		brelse(from);
		brelse(to);
	}
}

static void read_log_header(void)
{
	uint32_t i;
	struct buffer *b;
	struct log_header *lh;

	b = bread(lg.dev, lg.start);
	lh = (struct log_header *)(b->data);
	lg.lh.n = lh->n;
	for (i = 0; i < lh->n; i++)
		lg.lh.blocks[i] = lh->blocks[i];
	brelse(b);
}

static void write_log_header(void)
{
	uint32_t i;
	struct buffer *b;
	struct log_header *lh;

	b = bread(lg.dev, lg.start);
	lh = (struct log_header *)(b->data);
	lh->n = lg.lh.n;
	for (i = 0; i < lg.lh.n; i++)
		lh->blocks[i] = lg.lh.blocks[i];
	bwrite(b);
	brelse(b);
}

static void recover_from_log(void)
{
	read_log_header();
	install_trans(true);
	lg.lh.n = 0;
	write_log_header();
}

void begin_op(void)
{
	spin_lock_acquire(&lg.lock);
	while (true) {
		if (lg.committing) {
			sleep_on(&lg, &lg.lock);
		} else if (lg.lh.n + (lg.outstanding + 1) * MAX_OP_BLKS >
			   LOG_SIZE) {
			sleep_on(&lg, &lg.lock);
		} else {
			lg.outstanding += 1;
			spin_lock_release(&lg.lock);
			break;
		}
	}
}

void end_op(void)
{
	bool do_committing = false;

	spin_lock_acquire(&lg.lock);
	lg.outstanding -= 1;
	if (lg.committing)
		panic("end the operation when the log is committing");
	if (lg.outstanding == 0) {
		do_committing = true;
		lg.committing = true;
	} else {
		wake_up(&lg);
	}
	spin_lock_release(&lg.lock);

	if (do_committing) {
		commit();
		spin_lock_acquire(&lg.lock);
		lg.committing = false;
		wake_up(&lg);
		spin_lock_release(&lg.lock);
	}
}

static void write_to_log(void)
{
	uint32_t i;
	struct buffer *from, *to;

	for (i = 0; i < lg.lh.n; i++) {
		from = bread(lg.dev, lg.lh.blocks[i]);
		to = bread(lg.dev, lg.start + 1 + i);
		memmove(to->data, from->data, BLOCK_SIZE);
		bwrite(to);
		brelse(from);
		brelse(to);
	}
}

static void commit(void)
{
	if (lg.lh.n > 0) {
		write_to_log();
		write_log_header();
		install_trans(false);
		lg.lh.n = 0;
		write_log_header();
	}
}

void log_write(struct buffer *b)
{
	uint32_t i;

	spin_lock_acquire(&lg.lock);
	if (lg.lh.n >= lg.size - 1)
		panic("too big a transaction");
	if (lg.outstanding < 1)
		panic("outside of transaction");

	for (i = 0; i < lg.lh.n; i++) {
		if (lg.lh.blocks[i] == b->bno)
			break;
	}

	lg.lh.blocks[i] = b->bno;
	if (i == lg.lh.n) {
		bpin(b);
		lg.lh.n++;
	}
	spin_lock_release(&lg.lock);
}
