#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <err.h>

#include <stdint.h>
// Check to see if uint8_t exists, throw error message otherwise
#ifndef UINT8_MAX
#error "No support for uint8_t"
#endif

#define arrlen(x) (sizeof (x) / sizeof *(x))

#ifndef WRAPNUM
#define WRAPNUM 76
#endif

/* AUTHOR: COMINGUPWITHNAMES 
 * LAST MODIFIED: 1/27/2023
 * COURSE NUMBER: CS 344
 * DESCRIPTION: This program will read input from either a file indicated in additional arguments or
 *              from the keyboard from the command line and encode them in BASE64 and print them to stdout
 */

static char const alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                               "abcdefghijklmnopqrstuvwxyz"  
                               "0123456789+/=";

int main(int argc, char *argv[])
{
  int count = 0; // Counter to print \n every 76 characters 
  FILE *fileHandle = stdin; // Given file opened for reading
  if(argc == 1){ } // No file specified, do nothing
  else if (argc == 2) // File name included, check for the - character
  {
    if(strcmp(argv[1], "-")) // If no - char read, open the file
    {
      fileHandle = fopen(argv[1], "r");
      if(fileHandle == NULL) { err(errno, "fopen()"); }
    }
  }
  else { err(errno=EINVAL, "More than one argument received"); }

  for(;;)
  {
    uint8_t in[3]; 
    size_t numRead = fread(in, sizeof *in, arrlen(in), fileHandle); // Read three characters into the in array
    if(numRead < arrlen(in) && ferror(fileHandle)) 
    {
      // If num characters read less than three due to a file reading error, exit with error
      err(errno, "fread() within loop"); 
    } 
    if(numRead == 0 && feof(fileHandle)) { break; } // End of file reached, break out of the loop

    // Process our input using bit manipulations -- Reference: RFC 4648
    uint8_t out_idx[4];
    //Grab upper six bits in byte 0
    out_idx[0] = in[0] >> 2;
    //Grab lower four bits of byte 0 and upper four of byte 1 and mask them against 0x3Fu
    out_idx[1] = (in[0] << 4 | in[1] >> 4) & 0x3Fu; // 0x3Fu = 00111111
    // Grab lower 6 bits of byte 1 and upper 2 bits of byte 2 and mask them again
    out_idx[2] = (in[1] << 2 | in[2] >> 6) & 0x3Fu;
    //Grab lower 6 bits of byte 2 by masking them against 0x3Fu
    out_idx[3] = in[2] & 0x3Fu; 

    if(numRead < 3) //If we read less than three, pad with an = sign and garbage check the third position
    { 
      out_idx[3] = 64;
      out_idx[2] &= 0x3Cu; // 0x3Cu = 00111100
    }
    if(numRead < 2) // If we read less than two, pad with another = sign and garbage check the second position
    {
      out_idx[2] = 64;
      out_idx[1] &= 0x30u; // 0x30u = 00110000
    }
    
    for(size_t i = 0; i<arrlen(out_idx); ++i)
    {
      if(putchar(alphabet[out_idx[i]]) == EOF) { err(errno, "Initial putchar()"); } 
      if(++count == WRAPNUM)
        {
          if(putchar('\n') == EOF) { err(errno, "putchar() wrapping loop"); }
          count = 0;
        }
    }
    if(numRead < arrlen(in) && feof(fileHandle)) { break; }
  }

  if(count != 0) { if(putchar('\n') == EOF) { err(errno, "putchar() at the end of the code"); } }

  if(fflush(stdout) == EOF) { err(errno, "fflush()"); }

  if(fileHandle != stdin && fclose(fileHandle) == EOF) { err(errno, "fclose()"); }
  return EXIT_SUCCESS;
}
