
#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <string.h>
#include <ctype.h>
#include <err.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "fifo.h"

#define arrlen(arr) (sizeof(arr) / sizeof *(arr))


struct thread_args 
{
  struct fifo *in, *out;
};


void *input_thread(void *_targs)
{
  struct thread_args *targs = _targs;
  for (;;) {
        char *inStr = NULL;
        size_t length = 0;
        int numRead = getline(&inStr, &length, stdin); // Get the next line of text from stdin and use its return value in the next fifo_write
        if (numRead == -1) break;
        if (strcmp(inStr, "STOP\n") == 0) 
        {
          break;
        }
        if (fifo_write(targs->out, inStr, numRead) == -1) err(1, "fifo_read");
        free(inStr);
  }
  fifo_close_read(targs->in);
  fifo_close_write(targs->out);
  return targs;
}


void *line_separator_thread(void *_targs)
{
  struct thread_args *targs = _targs;
  for (;;) {
        char c;
        ssize_t r = fifo_read(targs->in, &c, 1);
        if (r < 0) err(1, "fifo_read");
        if (r == 0) break;
        if (c == '\n') c = ' ';
        if (fifo_write(targs->out, &c, r) == -1) err(1, "fifo_write");
  }
  fifo_close_read(targs->in);
  fifo_close_write(targs->out);
  return targs;
}


void *replace_thread(void *_targs)
{
  struct thread_args *targs = _targs;
  for (;;) 
  {
        char c;
        ssize_t r = fifo_read(targs->in, &c,  1);
        if (r < 0) err(1, "fifo_read");
        if (r == 0) break;
        if (c == '+') // If we find an instance of a +, grab the next character for comparison
        {
          char compChar;
          ssize_t compR = fifo_read(targs->in, &compChar, 1);
          if(compR < 0) err(1, "comp fifo_read");
          if(compR == 0) // Nothing read, write the previously grabbed character to the next buffer
          {
            if(fifo_write(targs->out, &c, r) == -1) err(1, "comp fifo_write");
            break;
          }
          if(compChar == '+') // + found, replace the compChar with a carat and write it to the next buffer
          {
            compChar = '^';
            if(fifo_write(targs->out, &compChar, compR) == -1) err(1, "carrat fifo_write");
          }
          else // No additional + found, write both to the next buffer
          {
            if (fifo_write(targs->out, &c, r) == -1) err(1, "fifo_write");
            if (fifo_write(targs->out, &compChar, compR) == -1) err(1, "fifo_write");
          }
        }
        else // Regular character, write to the next buffer
        {
          if (fifo_write(targs->out, &c, r) == -1) err(1, "fifo_write");
        }
  }
  fifo_close_read(targs->in);
  fifo_close_write(targs->out);
  return targs;
}


void *output_thread(void *_targs)
{
  struct thread_args *targs = _targs;
  for (;;) 
  {
    char *toPrint = malloc(81); 
    ssize_t r = fifo_read(targs->in, toPrint, 80); 
    if (r < 0) err(1, "fifo_read");
    if (r == 0) break;
    if(r == 80) //If we read 80 characters, null terminate the string, print it, then free it
    {
      toPrint[strlen(toPrint)] = '\0';
      printf("%s\n", toPrint);
      free(toPrint);
      fflush(stdout);
    }
  }
  fifo_close_read(targs->in);
  fifo_close_write(targs->out);
  return targs; 
}




int
main(int argc, char *argv[])
{
  struct fifo *fifos[3];
  for (size_t i = 0; i < arrlen(fifos); ++i) {
        fifo_create(&fifos[i], 1024);
  }


  pthread_t threads[4];
  pthread_create(&threads[0], NULL, input_thread, &(struct thread_args) {.in=NULL, .out=fifos[0]});
  pthread_create(&threads[1], NULL, line_separator_thread, &(struct thread_args) {.in=fifos[0], .out=fifos[1]});
  pthread_create(&threads[2], NULL, replace_thread, &(struct thread_args) {.in=fifos[1], .out=fifos[2]});
  pthread_create(&threads[3], NULL, output_thread, &(struct thread_args) {.in=fifos[2], .out=NULL});
 
  for (size_t i = 0; i < arrlen(threads); ++i) {
        pthread_join(threads[i], NULL);
  }
  for (size_t i = 0; i < arrlen(fifos); ++i) {
        fifo_destroy(fifos[i]);
        fifos[i] = NULL;
  }
}
