#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"


int main() {
  //setvbuf(stdout, NULL, _IONBF, 0);
  char *name = (char*) malloc(128);
  printf("Hello, I am computer, what's your name?\n");
  scanf("%s", name);
  printf("Hello, %s!\n", name);
  return 0;
}
