#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"


int main() {
  setvbuf(stdout, NULL, _IONBF, 0);
  char *name = (char*) calloc(1, 128);
  printf("Hello, I am computer, what's your name?\n");
  name = realloc(name, 512);
  scanf("%s", name);
  printf("Hello, %s!\n", name);
  return 0;
}
