#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define arrlen(x) (sizeof(x) / sizeof *(x))

int main(int argc, char *argv[])
{
  char alphabet[27] = {"ABCDEFGHIJKLMNOPQRSTUVWXYZ "};
  if(argc < 2 || argc > 2)
  {
    fprintf(stderr, "USAGE: %s keyLength\n", argv[0]);
    exit(0);
  }

  int keyLength = atoi(argv[1]);
  int indexNum;
  char toPrint;
  srand(time(NULL)); //Initialize our random generator
  
  for(int i = 0; i<keyLength; i++)
  {
    indexNum = rand() % arrlen(alphabet);
    toPrint = alphabet[indexNum];
    fprintf(stdout, "%c", toPrint);
  }
  putc(10, stdout);
}
