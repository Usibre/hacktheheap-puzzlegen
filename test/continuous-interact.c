#include "stdio.h"
#include "stdlib.h"
#include "unistd.h"


int main() {
  char *val;
  printf("Hello, I am a computer and I'll duplicate everything you do.\n");
  while (1) {
    val = (char*) malloc(128);
    scanf("%s", val);
    printf("%s\n", val);
    free(val);
  }
  return 0;
}
