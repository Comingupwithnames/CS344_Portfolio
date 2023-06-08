#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>


struct fifo {
  unsigned char *start, *end, *read, *write;
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  int read_open : 1, write_open : 1;
};


struct fifo *
fifo_create(struct fifo **fifo, size_t size)
{
  int save_errno = errno;
  struct fifo *_fifo = malloc(sizeof *_fifo);
  if (!_fifo) goto end;
  _fifo->start = malloc(sizeof *_fifo->start * size);
  if (!_fifo->start) goto err_1;
  if ((errno = pthread_mutex_init(&_fifo->mutex, NULL)) ||
          (errno = pthread_cond_init(&_fifo->cond, NULL))) {
        err(1, "fifo_create"); // Abort, unrecoverable error
  }
  _fifo->end = _fifo->start + size;
  _fifo->read = _fifo->start;
  _fifo->write = _fifo->start;
  _fifo->read_open = 1;
  _fifo->write_open = 1;
  goto end;
err_1:
  free(_fifo);
  _fifo = NULL;
end:
  if (_fifo) *fifo = _fifo;
  errno = save_errno;
  return _fifo;
}


void
fifo_destroy(struct fifo *fifo)
{
  int save_errno = errno;
  if (!fifo) return;
  free(fifo->start);
  if ((errno = pthread_mutex_destroy(&fifo->mutex))) err(1, "pthread_mutex_destroy");
  if ((errno = pthread_cond_destroy(&fifo->cond))) err(1, "pthread_cond_destroy");
  free(fifo);
  errno = save_errno;
}


ssize_t
fifo_write(struct fifo *fifo, void const *buf, size_t n)
{
  int save_errno = errno;
  ssize_t i = 0;
  if (!fifo || !buf) {
        i = -1;
        errno = EINVAL;
        goto end;
  }
  if (n == 0) goto end;
  unsigned char *write, *read;
  write = fifo->write;
  read = write + 1;


  for (;;) {
        /* Loop freely until reaching read position (don't write into read-only region) */
        while (i < n && write + 1 != read && (read != fifo->start || write + 1 != fifo->end)) {
          *(write++) = ((unsigned char const *)buf)[i++];
          if (write == fifo->end) write = fifo->start;
        }
        if ((errno = pthread_mutex_lock(&fifo->mutex))) err(1, "pthread_mutex_lock");
        /* Update the shared write position and signal condition waiters */
        fifo->write = write;
        if ((errno = pthread_cond_signal(&fifo->cond))) err(1, "pthread_cond_signal");


        /* if all bytes have been written, we are done */
        if (i == n) {
          if ((errno = pthread_mutex_unlock(&fifo->mutex))) err(1, "pthread_mutex_unlock");
          break;
        }
        /* Otherwise, continuously (re-)check the read-only position until there is more room to write */
        for (;;) {
          read = fifo->read;
          if (write + 1 != read && (read != fifo->start || write + 1 != fifo->end)) break;
          if (!fifo->read_open) {
            save_errno = EPIPE;
            i = -1;
            if ((errno = pthread_mutex_unlock(&fifo->mutex))) err(1, "pthread_mutex_unlock");
            goto end;
          }
          if ((errno = pthread_cond_wait(&fifo->cond, &fifo->mutex))) err(1, "pthread_cond_wait");
        }
        if ((errno = pthread_mutex_unlock(&fifo->mutex))) err(1, "pthread_mutex_unlock");
  }
end:
  errno = save_errno;
  return i;
}


ssize_t
fifo_read(struct fifo *fifo, void *buf, size_t n)
{
  int save_errno = errno;
  ssize_t i = 0;
  if (!fifo || !buf) {
        i = -1;
        errno = EINVAL;
        goto end;
  }
  if (n == 0) goto end;
  unsigned char *write, *read;
  read = fifo->read;
  write = read;


  int eof = 0;
  while (!eof) {
        /* Loop freely until reaching write position (don't read from write-only region) */
        while (i < n && read != write) {
          ((unsigned char *)buf)[i++] = *(read++);
          if (read == fifo->end) read = fifo->start;
        }
        if ((errno = pthread_mutex_lock(&fifo->mutex))) err(1, "pthread_mutex_lock");
        /* Update the shared read position and signal condition waiters */
        fifo->read = read;
        if ((errno = pthread_cond_signal(&fifo->cond))) err(1, "pthread_cond_signal");
        /* if all bytes have been read, we are done */
        if (i == n) {
          if ((errno = pthread_mutex_unlock(&fifo->mutex))) err(1, "pthread_mutex_unlock");
          break;
        }
        for (;;) {
          write = fifo->write;
          if (read != write) break; /* More data to read */
          if (!fifo->write_open) {
            eof = 1;
            break;
          }
          if ((errno = pthread_cond_wait(&fifo->cond, &fifo->mutex))) err(1, "pthread_cond_wait");
        }
        if ((errno = pthread_mutex_unlock(&fifo->mutex))) err(1, "pthread_mutex_unlock");
  }
end:
  errno = save_errno;
  return i;
}

void
fifo_close_read(struct fifo *fifo)
{
  int save_errno = errno;
  if (!fifo) return;
  if ((errno = pthread_mutex_lock(&fifo->mutex))) err(1, "pthread_mutex_lock");
  fifo->read_open = 0;
  if ((errno = pthread_cond_signal(&fifo->cond))) err(1, "pthread_cond_signal");
  if ((errno = pthread_mutex_unlock(&fifo->mutex))) err(1, "pthread_mutex_unlock");
  errno = save_errno;
}

void
fifo_close_write(struct fifo *fifo)
{
  int save_errno = errno;
  if (!fifo) return;
  if ((errno = pthread_mutex_lock(&fifo->mutex))) err(1, "pthread_mutex_lock");
  fifo->write_open = 0;
  if ((errno = pthread_cond_signal(&fifo->cond))) err(1, "pthread_cond_signal");
  if ((errno = pthread_mutex_unlock(&fifo->mutex))) err(1, "pthread_mutex_unlock");
  errno = save_errno;
}

