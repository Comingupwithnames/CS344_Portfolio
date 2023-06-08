#ifndef FIFO_H__
#define FIFO_H__

#include <stdlib.h>
#include <unistd.h>

struct fifo;

struct fifo *fifo_create(struct fifo **fifo, size_t size);

void fifo_destroy(struct fifo *fifo);

ssize_t fifo_write(struct fifo *fifo, void const *buf, size_t n);

ssize_t fifo_read(struct fifo *fifo, void *buf, size_t n);

void fifo_close_read(struct fifo *fifo);

void fifo_close_write(struct fifo *fifo);

#endif  //FIFO_H__
