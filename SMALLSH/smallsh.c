/********************************************** 
 * AUTHOR: COMINGUPWITHNAMES
 * LAST MODIFIED: 2/26/2023
 * COURSE NUMBER: CS344
 * DESCRIPTION: This program is used to create
 * a shell much like bash. It will utilize
 * built in commands like cd and exit. 
 * If a command is not already built in,
 * then the command will be passed to the 
 * UNIX shell with the exec() function. 
 **********************************************/

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <err.h>
#include <errno.h>

#define MAX_CHAR    2048
#define MAX_COMMAND 512

/* Function initialization */
void handleSIGINT(int signo);
void handleSpecialHandle();
void checkRunningProcess();
int userInput();
char *expansion(char *restrict *restrict input, char const *restrict toFind, char const *restrict sub);
void builtInCommands();
int parseInput();
void forkProcess();

/* Global variable initialization */
int childExitStatus;                  // Exit status for any children
pid_t childPId;                         // Process Id for any children
int backgroundFlag = 0;               // Int to indicate if a process will run in the background or not 
int inFileDesc;
int outFileDesc;
char *lineToRead = NULL;       
size_t numRead = 0;
char *input_buffer[MAX_COMMAND] = {0};
pid_t processId;                      // Used for the expansion of $$
pid_t foregroundProcessId = 0;        // Used to indicate the exit status of the last foreground command 0 if unset
pid_t backgroundProcessId = -1;       // Used to indicate the processId of the most recent background process -1 if unset
char *delimChars;                     // Used to store the delimit chars used for seperating tokens
char *homeDir;                        // Used to store the home directory of the HOME env var 
char *processIdString;                // Used to store the string representation of the processId from getpid() 
char *foregroundExitString;           // Used to store the string representation of childExitStatus; 
char *backgroundIdString;             // Used to store the string represntation of the most recent background process 
char *inFileName = NULL;
char *outFileName = NULL;
struct sigaction SIGINT_action = {0};
struct sigaction SIGTSTP_action = {0};
struct sigaction oldAction = {0};


void handleSIGINT(int signo)
{

}

int main()
{

  SIGINT_action.sa_handler = SIG_IGN;
  sigfillset(&SIGINT_action.sa_mask);
  sigaction(SIGINT, &SIGINT_action, &oldAction);

  SIGTSTP_action.sa_handler = SIG_IGN;
  sigfillset(&SIGTSTP_action.sa_mask);
  sigaction(SIGTSTP, &SIGTSTP_action, NULL);
  

  processId = getpid();

  /* Check the HOME env variable and if it is null, set the homeDir global var to "" */
  if(getenv("HOME") == NULL) { homeDir = ""; }
  else { homeDir = getenv("HOME"); }

  /* Check the $? env var and if it is null, set the exit status of the last foreground command which by default is (0) */
  if(getenv("$?") == NULL) 
  { 
    childExitStatus = 0;
    foregroundExitString = malloc(10);
    sprintf(foregroundExitString, "%d", childExitStatus); 
  }
  else 
  { 
    childExitStatus = atoi(getenv("$?"));
    foregroundExitString = getenv("$?"); 
  }

    backgroundIdString = malloc(10);

  /* As a final check, check to see if the env var $$ exists and if not, set the processId to the curr process and malloc enough room to fit the Id */
  if(getenv("$$") == NULL) 
  { 
    processIdString = malloc(10);
    sprintf(processIdString, "%jd", (intmax_t) processId); 
  }
  else 
  {
    processId = atoi(getenv("$$"));
    processIdString = getenv("$$");
  }

/**************************************************************************** 
 * Start of the infinite loop that will:
 *
 *  Check for any un-waited-for background processes in the same group Id.
 *
 *  Print an interactive prompt to start reading input 
 *
 *  Split the words up according to the IFS env variable and put them in the
 *  input_buffer array of strings.
 *
 *  As they are being put in, they will be checked for any occurence of 
 *  "~/,$$,$?,?!" and will be replaced with the appropriate replacements.
 *
 *  Afterwards, the input will be parsed to determine what command to run and
 *  whar args it will be passed with.
 *
 *  Then, the appropriate command/builtIn will be executed where builtIn 
 *  commands skip the next step.
 *
 *  If it is not a builtIn command, the program will then move onto waiting
 *  for that process to finish execution before looping back to checking for
 *  processes.
 *
 *****************************************************************************/
  while(1)
  {
startLoop:
    /* Check the IFS env variable and if it is null, set the delimChars global var to a default value ("") */
    if(getenv("IFS") == NULL) { delimChars = " \t\n";}
    else { delimChars = getenv("IFS"); }
    checkRunningProcess();
    if(userInput() == 1) { goto startLoop; }
    builtInCommands();
    outFileName = NULL;
    inFileName = NULL;
    backgroundFlag = 0;
  }
  return EXIT_SUCCESS;
}

/*********************************************************************************
 *
 * checkRunningProcess:
 *
 * This function will be ran at the start of each loop where it will check for any
 * un-waited-for children and if there are, it will check the exit status/signal 
 * of that child to print the appropriate exit/signal/continue message.
 *
 ************************************************************************************/
void checkRunningProcess()
{
  while (waitpid(0, &childExitStatus, WNOHANG | WUNTRACED) > 0)
  {
    if(WIFEXITED(childExitStatus))
    {
      fprintf(stderr, "Child process %jd done. Exit Status %d.\n", (intmax_t)backgroundProcessId, WEXITSTATUS(childExitStatus));
      childPId = 0;
    }
    if(WIFSIGNALED(childExitStatus))
    {
      fprintf(stderr, "Child process %jd done. Signaled %d.\n", (intmax_t)backgroundProcessId, WTERMSIG(childExitStatus));
      childPId = 0;
    }
    if(WIFSTOPPED(childExitStatus))
    {
      kill(childPId, SIGCONT);
      fprintf(stderr, "Child process %jd stopped. Continuing. \n", (intmax_t)backgroundProcessId);
      backgroundProcessId = childPId;
      sprintf(backgroundIdString, "%jd", (intmax_t) backgroundProcessId);
    }
  }
}


/*************************************************************
 *
 * userInput:
 * 
 * This function will prompt the user by printing an interactive
 * prompt to the terminal and read a line of input given to it
 * by the user.
 *
 * Once done with reading the input it will then expand any
 * special params using expansion().
 *
 *************************************************************/
int userInput()
{
  /* Print the prompt of the command line */
  char *PromptString = getenv("PS1");
  if(PromptString == NULL)
  {
    PromptString = "";
  }
  fprintf(stderr, "%s", PromptString);

  /* Get the line of text the user enters into the terminal */
  ssize_t line_length;
  SIGINT_action.sa_handler = handleSIGINT;
  if((line_length = getline(&lineToRead, &numRead, stdin)) == -1)
  {
    if(feof(stdin)) 
    {
     fflush(stdin);
     fprintf(stderr, "\nexit\n");
     kill(0, SIGINT);
     exit(foregroundProcessId);
    }
    else
    {
      errno = 0;
      SIGINT_action.sa_handler = SIG_IGN;
      return 1;
    }
  }
  SIGINT_action.sa_handler = SIG_IGN;

  char* firstToken;
  char* dupToken;

  if((firstToken = strtok(lineToRead, delimChars)) == NULL) { return 1; }
  dupToken = strdup(firstToken);
  input_buffer[0] = strdup(dupToken);
  free(dupToken);

  int count = 0;
  while(input_buffer[count] != NULL)
  {
    count++;
    input_buffer[count] = strtok(NULL, delimChars);
  }

  count = 0;

  while (input_buffer[count] != NULL)
  {
    char * toPass = strdup(input_buffer[count]);
    if(strncmp(input_buffer[count], "~/", 2) == 0) 
    { 
      toPass = strcpy(toPass, expansion(&toPass, "~", homeDir));
      input_buffer[count] = strdup(toPass);
    }
    toPass = strcpy(toPass, expansion(&toPass, "$$", processIdString)); 
    toPass = strcpy(toPass, expansion(&toPass, "$?", foregroundExitString));
    toPass = strcpy(toPass, expansion(&toPass, "$!", backgroundIdString));
    input_buffer[count] = strdup(toPass);
    count++;
    free(toPass);
  }

return 0;
}

/*****************************************
 *
 * expansion:
 *
 * This function will replace the toFind
 * text with the text specified by the
 * string sub and return the new string.
 *
 *****************************************/
char *expansion(char *restrict *restrict input, char const *restrict toFind, char const *restrict sub)
{
  /* Dereference the input and set the appropriate lengths of each string */
  char *deRef = *input;
  size_t input_len = strlen(deRef);
  size_t const toFind_len =  strlen(toFind),
               sub_len = strlen(sub);

  /* Start of the find loop that will loop over the string and replace each instance of what it is finding */
  for (; (deRef = strstr(deRef, toFind));)
  {
    ptrdiff_t offset = deRef - *input; // Set offset equal to the difference between the two pointers

    if(sub_len > toFind_len) //If our sub length is larger than the string to find, realloc the string to fit a larger one
    {
      deRef = realloc(*input, sizeof **input * (input_len + sub_len - toFind_len + 1));
      if(!deRef) { goto exit; }
      *input = deRef;
      deRef = *input + offset;
    }

    memmove(deRef + sub_len, deRef + toFind_len, input_len + 1 - offset - toFind_len);
    memcpy(deRef, sub, sub_len);

    input_len = input_len + sub_len - toFind_len; //update input length

    if(strncmp("~", toFind, 1) == 0) { break; }

    deRef += sub_len; // move the pointer for deRef to continue looking for more occurences
  }
  deRef = *input;
    if(sub_len < toFind_len) //If our sub length is less than toFind, realloc the string to be smaller
    {
      deRef = realloc(*input, sizeof **input * (input_len + 1));
      if(!deRef) { goto exit; }
      *input = deRef;
    }
exit:    
  return deRef;
}


/*************************************************************
 *
 * builtInCommands:
 *
 * This function implements the running of builtIn commands
 * by comparing the first element of input_buffer to both
 * "cd" and "exit" and executing the appropriate commands.
 *
 * If it is not a builtIn command, it will pass on to
 * forkProcess to execute the command there.
 *
 *************************************************************/
void builtInCommands()
{
  if(parseInput() == 1) { goto exitLabel; }

  if(strcmp(input_buffer[0], "cd") == 0)
  {
     if(input_buffer[2] != NULL)
     {
       errx(-1, "Too many arguments for cd, try again.");
       errno = 0;
       goto exitLabel;
     }
     char currWorkingDirectory[MAX_CHAR]; //Create char array to hold the directory
     char *newDirPath;
     getcwd(currWorkingDirectory, sizeof(currWorkingDirectory));
      
     newDirPath = input_buffer[1];
     if(newDirPath)
     {
       strcat(currWorkingDirectory, "/");
       strcat(currWorkingDirectory, newDirPath);
       if(chdir(currWorkingDirectory) == -1)
       {
         errx(errno, "Directory specified is not a file or directory");
         errno = 0;
       }
     }
     else
     {
       if(chdir(homeDir) == -1)
       {
         errx(errno, "Change home directory failed");
         errno = 0;
       }
     }
  }
   else if(strcmp(input_buffer[0], "exit") == 0)
   {
     int exitCode;
     if(input_buffer[1] != NULL)
     {
      exitCode = atoi(input_buffer[1]);
     }
     else 
     {
      exitCode = childExitStatus;
     }
     fprintf(stderr, "\nexit\n");
     kill(0, SIGINT);
     exit(exitCode);
   }
   else
   {
    forkProcess();
   }
   exitLabel:
      fflush(stdout);
}

/************************************************************
 *
 * parseInput:
 * This function will operate on input_buffer and look through
 * it and handle any special words it comes across.
 *
 *************************************************************/
int parseInput()
{
  if(input_buffer[0] == NULL) { return 1; }

  for (int i = 0; i<MAX_COMMAND; i++)
  {
    if(input_buffer[i] == NULL)
    {
      break;
    }
    if(strcmp(input_buffer[i], "&") == 0 && input_buffer[i+1] == NULL)
    {
      backgroundFlag = 1;
      input_buffer[i] = NULL;
      if(i > 1 && strcmp(input_buffer[i-2], ">") == 0)
      {
        outFileName = strdup(input_buffer[i-1]);
        if(i > 3 && strcmp(input_buffer[i-4], "<") == 0)
        {
          inFileName = strdup(input_buffer[i-3]);
          input_buffer[i-4] = NULL;
          input_buffer[i-3] = NULL;
        }
        input_buffer[i-2] = NULL;
        input_buffer[i-1] = NULL;
        break;
      }
      if(i > 1 && strcmp(input_buffer[i-2], "<") == 0)
      {
        inFileName = strdup(input_buffer[i-1]);
        if(i > 3 && strcmp(input_buffer[i-4], ">") == 0)
        {
          outFileName = strdup(input_buffer[i-3]);
          input_buffer[i-4] = NULL;
          input_buffer[i-3] = NULL;
        }
        input_buffer[i-2] = NULL;
        input_buffer[i-1] = NULL;
        break;
      }
      break;
    }
    if(input_buffer[i+1] == NULL && i != 0 && strcmp(input_buffer[i-1], "<") == 0)
    {
      inFileName = strdup(input_buffer[i]);
      if(strcmp(input_buffer[i-3], ">") == 0)
      {
        outFileName = strdup(input_buffer[i-2]);
        input_buffer[i-3] = NULL;
        input_buffer[i-2] = NULL; 
      }
      input_buffer[i-1] = NULL;
      input_buffer[i] = NULL;
      break;
    }
    if(input_buffer[i+1] == NULL && i != 0 && strcmp(input_buffer[i-1], ">") == 0)
    {
      outFileName = strdup(input_buffer[i]);
      if(strcmp(input_buffer[i-3], "<") == 0)
      {
        inFileName = strdup(input_buffer[i-2]);
         input_buffer[i-3] = NULL;
        input_buffer[i-2] = NULL; 
      }
      input_buffer[i-1] = NULL;
      input_buffer[i] = NULL;
      break;
    }
    if(strcmp(input_buffer[i], "#") == 0)
    {
      for (int j = i; j<MAX_COMMAND; j++)
      {
        if(input_buffer[j+1] !=NULL)
        {
          input_buffer[j] = NULL;
          //free(input_buffer[j]);
        }
        else if(input_buffer[j] != NULL)
        {
          input_buffer[j] = NULL;
          //free(input_buffer[j]);
        }
        else
        {
          break;
        }
      }
    }
  }
return 0;
}



void forkProcess()
{ 
  childPId = fork();

  switch(childPId)
  {
  case -1:
          perror("fork()\n");
          exit(1);
          break;
  case 0:
          sigaction(SIGINT, &oldAction, NULL);
          if(inFileName != NULL)
          {
            inFileDesc = open(inFileName, O_RDONLY | O_CREAT, 0444);
            if(inFileDesc == -1)
            {
              printf("open() failed on \"%s\"\n", inFileName);
              perror("IO Error");
              errno = 0;
              exit(1);
            }
            dup2(inFileDesc, 0);
            close(inFileDesc);
          }
          if(outFileName != NULL)
          {
            outFileDesc = open(outFileName, O_RDWR | O_CREAT | O_APPEND, 0777);
            if(outFileDesc == -1)
            {
              printf("open() failed on \"%s\"\n", outFileName);
              perror("IO Error");
              errno = 0;
              exit(2);
            }
            dup2(outFileDesc, 1);
            close(outFileDesc);
          }
          if(strncmp(input_buffer[0], "/", 1) == 0)
          {
            execv(input_buffer[0], input_buffer);
            perror("execv");
            errno = 0;
            exit(3);
          }
          else
          {
            execvp(input_buffer[0], input_buffer);
            perror("execvp");
            errno = 0;
            exit(4);
          }
          break;
         
  default:
          if(backgroundFlag == 1)
          {
            backgroundIdString = realloc(backgroundIdString, 20);
            backgroundProcessId = childPId;
            sprintf(backgroundIdString, "%jd", (intmax_t) childPId);
          }
          else
          {
            childPId = waitpid(childPId, &childExitStatus, WUNTRACED);
            foregroundProcessId = childPId;
            if(WIFEXITED(childExitStatus))
            {
             sprintf(foregroundExitString, "%d", WEXITSTATUS(childExitStatus)); 
            }
          }
          break;
  }
}



