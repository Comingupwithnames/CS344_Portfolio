#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>

/* Global variable for our alphabet to encode off of */
char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ ";

/* Error function used for reporting issues */
void error(const char *msg) {
  perror(msg);
  exit(1);
} 

/* Set up the address struct for the server socket */
void setupAddressStruct(struct sockaddr_in* address, 
                        int portNumber){
 
  /* Clear out the address struct */
  memset((char*) address, '\0', sizeof(*address)); 

  /* The address should be network capable */
  address->sin_family = AF_INET;
  /* Store the port number */
  address->sin_port = htons(portNumber);
  /* Allow a client at any address to connect to this server */
  address->sin_addr.s_addr = INADDR_ANY;
}

/**
 * This function will act as a recv loop to receive all the
 * data coming across a socket connection on the network
 * and will return -1 if it fails, 0 if successful
 */
int recvall(int sockDesc, char *buf, int *len)
{
  int total = 0;
  int bytesLeft = *len;
  int n;

  while(total < *len)
  {
    n = recv(sockDesc, buf+total, bytesLeft, 0);
    if(n == -1) { break; }
    total += n;
    bytesLeft -= n;
  }

  *len = total;

  return n==-1?-1:0;
}

int main(int argc, char *argv[]){
  /* Set an initial alarm of 3 minutes to make sure the port is not always in use after execution */
  alarm(180);
  int connectionSocket, charsRead, charsWritten;
  pid_t spawnid = -5;
  int childStatus, childPid;
  int plainIndex, keyIndex, cipherIndex;
  int numChildren = 0;
  int bufLength = 0;
  char buffer[4];
  struct sockaddr_in serverAddress, clientAddress;
  socklen_t sizeOfClientInfo = sizeof(clientAddress);

  /* Check usage & args */
  if (argc < 2) { 
    fprintf(stderr,"USAGE: %s port\n", argv[0]); 
    exit(1);
  } 
  
  /* Create the socket that will listen for connections */
  int listenSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (listenSocket < 0) {
    error("ERROR opening socket");
  }

  /* Set up the address struct for the server socket */
  setupAddressStruct(&serverAddress, atoi(argv[1]));

  /* Associate the socket to the port */
  if (bind(listenSocket, 
          (struct sockaddr *)&serverAddress, 
          sizeof(serverAddress)) < 0){
    error("ERROR on binding");
  }

  /* Start listening for connetions. Allow up to 5 connections to queue up */
  listen(listenSocket, 5); 
  
  while(1){
    /* Perform a blocking wait if the number of child processes is 5 */
    while(numChildren == 5)
    {
      if(waitpid(-1, NULL, 0) > 0){ numChildren--; }
    }

    /* Accept the connection request which creates a connection socket */
    connectionSocket = accept(listenSocket, 
                (struct sockaddr *)&clientAddress, 
                &sizeOfClientInfo); 
    if (connectionSocket < 0){ error("ERROR on accept"); }
    spawnid = fork();
    switch(spawnid)
    {
      case -1:
        fprintf(stderr, "fork() failed!");
        break;
      case 0:
        /* Set an alarm again since the child does not inherit an alarm as well */
        alarm(180);

        while(1)
        {
          /* Set our buffer to "garbage" values so it correctly calculates length */
          memset(buffer, 'a', 3);

          /* Set our buffer length and call our recvall function to read in everything into the buffer */
          bufLength = strlen(buffer);
          charsRead = recvall(connectionSocket, buffer, &bufLength); 
          if (charsRead < 0){ error("ERROR reading from socket"); }

          /* Check the identifier index to make sure we are dealing with the right client */
          if(buffer[2] != 'e' && buffer[2] != 'a')
          {
            fprintf(stderr, "Wrong client connected. Attempted port: %d\n", ntohs(clientAddress.sin_port));
            /* Send a notification back to the client telling it that it connected to the wrong server */
            buffer[2] = 'w';
            charsWritten = send(connectionSocket, buffer, sizeof(buffer)-1, 0);
            if(charsWritten < 0) { error("ERROR writing to socket"); }
            close(connectionSocket);
            exit(2);
          }

          /* Check to see if we have reached the end of our text to encode then send the termination index back */
          if(buffer[0] == '@' && buffer[1] == '@' && buffer[2] == 'e')
          {
            buffer[2] = 't';
            charsWritten = send(connectionSocket, buffer, sizeof(buffer)-1,0);
            if(charsWritten < 0) { error("ERROR writing to socket"); }
            break;
          }

          /* If we have not reached the end of our text to read, encrypt it and send it back to the client */
          if(buffer[0] != '@' && buffer[1] != '@' && buffer[2] == 'e')
          {
            /* Null our identifier index to set later */
            buffer[2] = '\0';

            /* Loop through the alphabet array to find the indecies of each letter to encode them */ 
            for(int i = 0; i<sizeof(alphabet); i++)
            {
              if(buffer[0] == alphabet[i])
              {
                plainIndex = i;
              }
              if(buffer[1] == alphabet[i])
              {
                keyIndex = i;
              }
            }

            /* Calculate the index of our cipher character and put it in the buffer */
            cipherIndex = (plainIndex + keyIndex) % 27;
            buffer[0] = alphabet[cipherIndex];

            /* Null out the next index of our buffer as it is no longer needed */
            buffer[1] = '\0';

            /* Set the success index identifier and send message back to the client */
            buffer[2] = 'c';
            charsWritten = send(connectionSocket, buffer, sizeof(buffer)-1, 0); 
            if (charsWritten < 0)
            {
              error("ERROR writing to socket");
            }
          }
        }
        /* Close the connection socket for this client and exit */
        close(connectionSocket);
        exit(0);
        break;
      default:
        /* Increment the number of children since we performed a fork earlier and perform a non blocking wait */
        numChildren++;
        childPid = waitpid(childPid, &childStatus, WNOHANG);
        break;
    }       
  }
  /* Close the listening socket */
  close(listenSocket); 
  return 0;
}
