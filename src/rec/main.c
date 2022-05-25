#include "main.h"
#include "manager.h"
#include "console.h"




char **str_split(char **pointers, char* line) {
  size_t ctr = 0;
  memset(pointers, 0, CONSOLE_ARGSIZE*(sizeof(char**)));
  pointers[ctr++] = line;
  char *token = strchr(line, ' ');
  while (token != NULL) {
    *token = '\0'; // replace the space with a null byte
    pointers[ctr++] = ++token;
    token = strchr(token, ' ');
  }
  return pointers;
}


int main(int argc, char* argv[]) {
  /* create message queue */
  if (strcmp(argv[1], "-h")==0 || strcmp(argv[1], "--help")==0) { 
    printf("Hack the Heap\nHeap Recorder utility Console\nVersion: %s\n"
      "Start the console without arguments (or using a script as argument) to use the console.\n", RECVERSION);
  }
  msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0600);
  if (msqid == -1) {
    perror("msgget");
    return EXIT_FAILURE;
  }


  pid_t child_pid = fork();
  if (child_pid == 0) {
    // child
    start_listener();
  } else {
    // parent
    childpid = child_pid;
    if (argc > 1) {
      start_console(++argv);
    } else {
      start_console(NULL);
    }
  }
  return 0;
}
