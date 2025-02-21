#ifndef _VIRTIO_DISK_H
#define _VIRTIO_DISK_H

struct buffer;

void virtio_disk_init(void);
void virtio_disk_intr(void);
void virtio_disk_read(struct buffer *b);
void virtio_disk_write(struct buffer *b);

#endif
