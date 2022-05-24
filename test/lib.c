#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"


int askname() {
  //setvbuf(stdout, NULL, _IONBF, 0);
  char *name = (char*) malloc(128);
  printf("Hello, I am computer, what's your name?\n");
  scanf("%s", name);
  printf("Hello, %s!\n", name);
  free(name);
  char* unname = (char*) malloc(64);
  return 0;
}
