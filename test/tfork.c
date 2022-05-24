#include "stdlib.h"
#include "unistd.h"
#include "sys/types.h"
#include "stdio.h"



int main() {
  char *x = (char*)malloc(128);
  pid_t pid = fork();

  if (pid < 0) {
    printf("Fork failure.\n");
  } else if (pid == 0) {
    printf("Fork child, allocing 2\n");
    x = (char*)malloc(64);
    x = (char*)malloc(64);
  } else {
    printf("Fork parent, child @ %d. Alloc 4\n", pid);
    x = (char*)malloc(32);
    x = (char*)malloc(32);
    x = (char*)malloc(32);
    x = (char*)malloc(32);
  }
  return 0;
}
