#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>  // ssize_t
#include <sys/socket.h> // send(),recv()
#include <netdb.h>      // gethostbyname()

/**
* This program will act as the client to an encryption server where
* it will send plain text to the specified server for it to then
* perform encryption on. Then, it will receive the encrypted text back
* and write it to stdout
*/

/* Error function used for reporting issues */
void error(const char *msg) { 
  perror(msg); 
  exit(0); 
} 

/* Set up the address struct */
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber, 
                        char* hostname){
 
  /* Clear out the address struct */
  memset((char*) address, '\0', sizeof(*address)); 

  /* The address should be network capable */
  address->sin_family = AF_INET;
  /* Store the port number */
  address->sin_port = htons(portNumber);

  /* Get the DNS entry for this host name */
  struct hostent* hostInfo = gethostbyname(hostname); 
  if (hostInfo == NULL) { 
    fprintf(stderr, "CLIENT: ERROR, no such host\n"); 
    exit(0); 
  }
  /* Copy the first IP address from the DNS entry to sin_addr.s_addr */
  memcpy((char*) &address->sin_addr.s_addr, 
        hostInfo->h_addr_list[0],
        hostInfo->h_length);
}

/**
 * This function will loop multiple times if
 * the data it needs to send to the server has 
 * not been sent in full and will return -1 if it 
 * fails and 0 on success
 */
int sendall(int sockDesc, char *buf, int *len)
{
  int total = 0;        // Num bytes actually sent
  int bytesLeft = *len; //Bytes left to send
  int n;

  while(total < *len)
  {
    n = send(sockDesc, buf+total, bytesLeft, 0);
    if(n == -1) { break; }
    total += n;
    bytesLeft -= n;
  }

  *len = total; // Number actually sent updated to the total

  return n==-1?-1:0; //Return -1 on failure, 0 on success
}

int main(int argc, char *argv[]) {
  int socketFD, portNumber, charsWritten, charsRead, bufLen;
  int plainChar, keyChar;
  FILE *plainFile, *keyFile;
  struct sockaddr_in serverAddress;
  char buffer[4];

  /* Check usage & args */
  if (argc < 4) { 
    fprintf(stderr,"USAGE: %s plaintext key port\n", argv[0]); 
    exit(0); 
  } 
  
  /* Open our plainFile and if it is null, exit with the appropriate error message and status */
  plainFile = fopen(argv[1], "r");
  if(plainFile == NULL)
  {
    fprintf(stderr, "Plaintext file could not be opened\n");
    exit(1);
  }

  /* Open our keyFile and if it is null, exit with the appropriate error message and status */
  keyFile = fopen(argv[2], "r");
  if(keyFile == NULL)
  {
    fprintf(stderr, "Keyfile could not be opened\n");
    exit(1);
  }

  /* Set the file pointers to the end of the file to check if our plainFile is longer than keyFile */
  fseek(plainFile, 0, SEEK_END);
  fseek(keyFile, 0, SEEK_END);

  if(ftell(plainFile) > ftell(keyFile))
  {
    fprintf(stderr, "Our plain file is longer than our key, exiting\n");
    exit(1);
  }

  /* Afterwards, reset the file pointers to the beginning to check for bar chars */
  fseek(plainFile, 0, SEEK_SET);
  fseek(keyFile, 0, SEEK_SET);

  /* As long as both have not reached EOF, continue looping through the files to check for bad chars */
  while((plainChar = fgetc(plainFile)) != EOF && (keyChar = fgetc(keyFile)) != EOF)
  {
    /* If our char is not a space, within the range, or a newline, it is a bad char, close and exit */
    if(plainChar != 32 && (plainChar < 65 || plainChar > 90) && plainChar != 10)
    {
      fprintf(stderr,"Bad char detected in %s, exiting\n", argv[1]);
      fclose(keyFile);
      fclose(plainFile);
      exit(1);
    }
    if(keyChar != 32 && (keyChar < 65 || keyChar > 90) && keyChar != 10)
    {
      fprintf(stderr, "Bad char detected in %s, exiting\n", argv[2]);
      fclose(keyFile);
      fclose(plainFile);
      exit(1);
    }
  }

  /* Reset the file pointers again for sending them to the server */
  fseek(plainFile, 0, SEEK_SET);
  fseek(keyFile, 0, SEEK_SET);

  /* Create a socket */
  socketFD = socket(AF_INET, SOCK_STREAM, 0); 
  if (socketFD < 0){ error("CLIENT: ERROR opening socket"); }

  /* Set up the server address struct */
  setupAddressStruct(&serverAddress, atoi(argv[3]), "localhost");

  /* Connect to server  and print the appropriate error message if it fails */
  if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0)
  {
    fprintf(stderr, "CLIENT: ERROR connecting to port %d\n", atoi(argv[3]));
    exit(2);
  }

  /* Before we enter the loop, null terminate the array */
  buffer[3] = '\0';

  /* As long as neither have reached EOF, continue grabbing chars from both files to send to the server */
  while((plainChar = fgetc(plainFile)) != EOF && (keyChar = fgetc(keyFile)) != EOF)
  {
    /* reset our identifying index and clear out the first two elements of the buffer array */
    memset(buffer, '\0', sizeof(buffer)-2);
    buffer[2] = 'e';
    if(plainChar == 10 || keyChar == 10)
    {
      buffer[0] = '@';
      buffer[1] = '@';
    }
    else
    {
      buffer[0] = plainChar;
      buffer[1] = keyChar;
    }

    bufLen = strlen(buffer);
    /* After putting our characters into the buffer, send them all to the server and error check appropriately */
    charsWritten = sendall(socketFD, buffer, &bufLen); 
    if (charsWritten < 0){
      error("CLIENT: ERROR writing to socket");
    }
    
    /* Null out the buffer and then read whatever the server sends back into our buffer for identifier checking */
    memset(buffer, '\0', sizeof(buffer));
    charsRead = recv(socketFD, buffer, sizeof(buffer)-1, 0); 
    if (charsRead < 0){ error("CLIENT: ERROR reading from socket"); }

    /* Check for the identifier from the server indicating it connected to the wrong server  and exit */
    if(buffer[2] == 'w')
    {
      fprintf(stderr, "Connected to the wrong server! Attempted port: %d\n", atoi(argv[3]));
      exit(2);
    }

    /* Check for the idenitifer from the server indicating it has sent something back */
    if(buffer[2] == 't')
    {
      fprintf(stdout, "\n");
      break;
    }

    /* Check for the identifier from the server identifying it has a valid character to read into stdout */
    if(buffer[2] == 'c')
    {
      buffer[2] = '\0';
      putc(buffer[0], stdout);
    }
  }
  /* Close the socket and files */
  close(socketFD);
  fclose(keyFile);
  fclose(plainFile);
  return 0;
}
