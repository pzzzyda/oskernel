#include "fs/fs.h"
#include "fs/buf.h"
#include "fs/log.h"
#include "lib/string.h"
#include "printk.h"

struct super_block sb;

void fs_init(uint32_t dev)
{
	struct buffer *b = bread(dev, 1);
	memmove(&sb, b->data, sizeof(sb));
	brelse(b);
	log_init(dev, &sb);
}
