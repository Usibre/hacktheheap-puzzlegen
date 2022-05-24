
#include "stdio.h" 
#include "stdlib.h"
#include "string.h"


int main(int argc, char *argv[]) { 
  int i = atoi(argv[1]); 
  char *buf = (char*)malloc(i);
  strncpy(buf, argv[2], i);
  printf("%s\n", buf);
  return 0;
}
