#ifndef _PIPE_H
#define _PIPE_H

#include "fs/file.h"

int pipe_alloc(struct file **rfile, struct file **wfile);
void pipe_close(struct pipe *pi, bool writable);
ssize_t pipe_read(struct pipe *pi, uint64_t dst, size_t n);
ssize_t pipe_write(struct pipe *pi, uint64_t src, size_t n);

#endif
