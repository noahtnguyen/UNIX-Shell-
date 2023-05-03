// ----------------------------------------------------------------------------
// Project 1—UNIX Shell
// Author: Noah Nguyen
// Created: April 11, 2023
// ----------------------------------------------------------------------------
// This project consists of designing a C or C++ program
// to serve as a shell interface that accepts user commands
// and then executes each command in a separate process.
// ----------------------------------------------------------------------------
// Creating the child process and executing the command in the child
// Providing a history feature
// Adding support of input and output redirection
// Allowing the parent and child processes to communicate via a pipe
// ----------------------------------------------------------------------------
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* ------------------------- The maximum length command ---------------------*/
#define MAX_LINE 80
// command line input
char command[MAX_LINE];

/* -------------------------- List of all functions -------------------------*/
void processing_input(char command[MAX_LINE], char *args[],
                      char history_container[MAX_LINE]);
void pipe_communication(char *args[], char *pidpipe[], bool ampersand);
void execution(char *args[], int argc);
bool redirection(char *args[], int argc);
int pipe_check(char *args[], int argc);

/* ------------------------------ main driver -------------------------------*/
int main() {
  char command[MAX_LINE];
  char *args[MAX_LINE / 2 + 1];
  bool ampersand;
  char history_container[MAX_LINE];
  int should_run = 1;

  // intialize the history_container and command before operating
  memset(args, 0, sizeof(args));
  memset(command, 0, MAX_LINE);
  memset(history_container, 0, MAX_LINE);
  // printf("Checkpoint: %s", history_container);

  while (should_run) {

    printf("osh>");
    fflush(stdout);
    // reading user input
    processing_input(command, args, history_container);

    if (args[0] != NULL) {
      // get the number of arguments
      int argc = 0;
      while (args[argc] != NULL) {
        argc++;
      }
      // exit the loop if the user enters exit
      if (strcmp(args[0], "exit") == 0) {
        should_run = 0;
        break;
      }

      // execute the command

      execution(args, argc);

      // wait for the child process to finish, unless the command included &
      ampersand = false; // initialize ampersand variable
      if (!ampersand) {
        wait(NULL);
      }
    }
  }
  return 0;
}


// ----------------------------------------------------------------------------
// processing_input():
//  - reads what the user enters on the command line
//  - based on the standard API:
//      ssize_t getline(char **lineptr, size_t *n, FILE *stream)
//  - arguments:
//      char command[]: an array to store the characters in the command
//                      entered by the user
//      char *args[]: an arra of character pointers that will hold the
//                    argument of the command
//      char *history_container: stores the most recent command entered.
//      int argc: number of arguments in char* args[]
// ----------------------------------------------------------------------------
void processing_input(char command[MAX_LINE], char *args[],
                      char history_container[MAX_LINE]) {

  // Read the user command into char command[]
  // ssize_t getline(char **lineptr, size_t *n, FILE *stream):
  //  - lineptr: where the line is store
  char *line = NULL;
  // size_t *n: size of the lineptr buffer
  size_t bufsize = 0;
  //  - FILE *stream: file descriptor to read input from the user (stdin)
  //  - command characters are stored in char command[]
  //  - the command_size is at max 80, which is MAX_LINE defined above
  // command_size: actual command line length
  ssize_t command_size;

  // Read user input using getline()
  command_size = getline(&line, &bufsize, stdin);
  // printf("Command size: %d", command_size)

  // If an error occurs, getline() returns -1
  //  - exit(-1) to terminate the program due to an error
  if (command_size < 0) {
    perror("Error getline()");
    exit(EXIT_FAILURE);
  }

  // Copy user input to char command[]
  strcpy(command, line);

  // Free memory allocated by getline()
  free(line);

  // Set command_size to number of bytes read
  command_size = strlen(command);

  // Remove trailing newline character from the command
  command[strcspn(command, "\n")] = '\0';

  // If the user enters !!, execute the most recent command
  // in the history_container
  if (strcmp(command, "!!") == 0) {
    // check if there is any command in the history
    // if there is none, print statement to user screen
    // and wait for new command
    if (strlen(history_container) == 0) {
      printf("No commands in history.\n");
      return;
    } else {
      printf("%s\n", history_container);
      strcpy(command, history_container);
    }
  } else {
    // Copy user input to char history_container[]
    strcpy(history_container, command);
    // Parse what the user command by tokens from char command[]
    // using strtok to get the first token with separator is a whitespace
    char *token = strtok(command, " \n");
    // Store the tokens in an array of character strings (args)
    // Example: if the user enters the command ps -ael at the osh> prompt,
    //          the values stored in the args array are:
    //     args[0] = "ps"
    //     args[1] = "-ael"
    //     args[2] = NULL
    // counter i
    int i = 0;
    // check the token until it is null
    while (token != NULL) {
      args[i] = token;
      i++;
      token = strtok(NULL, " ");
    }
    args[i] = token;
  }
}

// ----------------------------------------------------------------------------
// pipe_communication():
//  - allow the output of one command to serve as input to another using a pipe
//  - assume that commands will contain only one pipe character
//    and will not be combined with any redirection operators.
// ----------------------------------------------------------------------------
void pipe_communication(char *args[], char *pidpipe[], bool ampersand) {
  // write end and read end of the pipe
  enum { READ, WRITE };
  // pipeFD[0] = READ
  // pipeFD[1] = WRITE
  int pipeFD[2];

  // if there is error exit with code -1
  if (pipe(pipeFD) < 0) {
    perror("Error in creating pipe");
    exit(EXIT_FAILURE);
  }

  // create child process
  pid_t pid_pipe;
  pid_pipe = fork();
  // if fork has error occured exit with code -1
  if (pid_pipe < 0) {
    perror("Error forking");
    exit(EXIT_FAILURE);
  }
  // otherwise work on the child process to execute command
  else if (pid_pipe == 0) {
    // close the READ end of the pipe
    close(pipeFD[0]);
    // stdout write to WRITE
    dup2(pipeFD[1], STDOUT_FILENO);
    // close the WRITE end of the pipe
    close(pipeFD[1]);
    // Execute the first command
    // if there is error exit with -1
    if (execvp(args[0], args) < 0) {
      perror("Error during exec");
      exit(EXIT_FAILURE);
    }
    // exit succesfully
    exit(EXIT_SUCCESS);
  }
  // if in the parent process, move to the second child process
  else {
    // fork the second child process
    pid_pipe = fork();
    // if fork has error occured exit with code -1
    if (pid_pipe < 0) {
      perror("Error forking");
      exit(EXIT_FAILURE);
    }
    // child
    else if (pid_pipe == 0) {
      // close the WRITE end of pipe
      close(pipeFD[1]);
      // stdin to read
      dup2(pipeFD[0], STDIN_FILENO);
      // close the READ end
      close(pipeFD[0]);
      // execute commands
      if (execvp(pidpipe[0], pidpipe) < 0) {
        perror("Error during exec");
        exit(EXIT_FAILURE);
      }
      // exit succesfully
      exit(EXIT_SUCCESS);
    }
    // parent
    else if (pid_pipe > 0) {
      int status;
      // close both ends
      close(pipeFD[0]);
      close(pipeFD[1]);
      // if there's ampersand then parent and
      // child processes will run concurrently.
      if (!ampersand) {
        waitpid(pid_pipe, &status, 0);
      }
      // otherwise return
      return;
    }
  }
}

// ----------------------------------------------------------------------------
// redirection():
//  - support the ‘>’ and ‘<’ redirection operators
//      '>' redirects the output of a command to a file
//      '<' redirects the input to a command from a file
//  - check for '<' and '>' in the command entered
//  - return true if it is in the command, false otherwise
// ----------------------------------------------------------------------------
bool redirection(char *args[], int argc) {
  for (int i = 0; i < argc; i++) {
    if ((strcmp(args[i], "<") == 0) || (strcmp(args[i], ">") == 0)) {
      return true;
    }
  }
  return false;
}

// ----------------------------------------------------------------------------
// pipe_check():
//  - check to see if it requires pipe communication
//  - return index of | if '|' is found in the command entered
//    otherwise return -1
// ----------------------------------------------------------------------------
int pipe_check(char *args[], int argc) {

  if (args == NULL || argc <= 0) {
    return -1;
  }
  for (int i = 0; i < argc; i++) {
    if (args[i] != NULL && strcmp(args[i], "|") == 0) {
      return i;
    }
  }
  return -1;
}

// ----------------------------------------------------------------------------
// execution()
//  - fork a child process
//  - execute command specified by the user
//  - uses pipe communication,  or redirection if needed
// agruments:
//      char *args[]: an arra of character pointers that will hold the
//                    argument of the command
//      int argc: number of arguments in char* args[]
// ----------------------------------------------------------------------------
void execution(char *args[], int argc) {
  // if the user does not enter anything, then just return
  if (args[0] == NULL) {
    return;
  }

  // bool variable to keep track of '&' that allows
  // the child process to run in background
  // intialized false
  bool ampersand = false;
  int pipe_char = pipe_check(args, argc);

  // pipe communication required
  if (pipe_char != -1) {
    // command: first_child | second_child
    //          args        | second_child

    // initializing second child
    int counter;
    char *second_child[MAX_LINE / 2 + 1];
    for (counter = 0; counter < argc - pipe_char - 1; counter++) {
      second_child[counter] = strdup(args[pipe_char + counter + 1]);
    }
    // get rid of eol
    second_child[counter] = NULL;

    // check for & at the end
    // allow the child process to run in the background
    if (strcmp(second_child[counter - 1], "&") == 0) {
      ampersand = true;
      // get rid of & in args so there's only command left
      args[counter - 1] = NULL;
    }

    // get rid of | in args so there's only commands left
    args[pipe_char] = NULL;

    // call pipe_communication
    pipe_communication(args, second_child, ampersand);
    return;
  }

  // no pipe communication
  else {
    // check for &
    if ((strcmp(args[argc - 1], "&") == 0)) {
      ampersand = true;
      // get rid of & in args
      args[argc - 1] = NULL;
      argc--;
    }

    // create child process
    pid_t pid;
    pid = fork();

    // check for error
    if (pid < 0) {
      perror("Error during fork");
      exit(EXIT_FAILURE);

    }
    //   // child process
    else if (pid == 0) {
      // check to see if redirection needed
      bool redirect = redirection(args, argc);

      // if redirection is needed, call redirection()
      if (redirect) {
        // output
        if (strcmp(args[argc - 2], ">") == 0) {
          // remove >
          args[argc - 2] = NULL;
          // open to read or write
          int output_fd = -1;
          // open InputFile with the following flags/ features:
          // - O_WRONLY: open for writing only
          // - O_CREAT: create new file if it's not there
          // - S_IRUSR: read permission for the file owner.
          // - S_IRGRP: read permission for members of the file's group.
          // - S_IWGRP: write permission for members of the file's group.
          // - S_IWUSR: write permission for the file owner.
          output_fd = open(args[argc - 1], O_WRONLY | O_CREAT,
                           S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
          // if output_fd is still -1, eror occured or inputFile is nullptr
          if (output_fd == -1) {
            // exit with -1
            perror("Failed to open file");
            exit(EXIT_FAILURE);
          }
          // otherwise, redirect output command to outputFile
          // using dup2 to duplicate output_fd to STDOUT_FILENO (stdout)
          dup2(output_fd, STDOUT_FILENO);
          // closefile descriptors to save them
          close(output_fd);
        }
        // input
        else if (strcmp(args[argc - 2], "<") == 0) {
          // remove <
          args[argc - 2] = NULL;
          // open inputFile with read-only
          int input_fd = -1;
          //    int file_descriptor = open("filename", O_RDONLY);
          input_fd = open(args[argc - 1], O_RDONLY);
          // if input_fd is still -1, error occured or inputFile is null
          if (input_fd == -1) {
            // exit with -1
            perror("Failed to open file");
            exit(EXIT_FAILURE);
          }
          // otherwise, redirect input command to inputFile
          // using dup2 to duplicate input_fd to STDIN_FILENO (stdin)
          dup2(input_fd, STDIN_FILENO);
          // close file descriptors to save them
          close(input_fd);
        }
      }
      // otherwise just execute using execvp
      if (execvp(args[0], args) < 0) {
        perror("Error during exec");
        exit(EXIT_FAILURE);
      }
      // exit successfully
      exit(EXIT_SUCCESS);
    }
    // parent process
    else {
      // if there is &
      // parent and child processes will run concurrently.
      if (ampersand) {
        wait(NULL);
      }
    }
  }
}