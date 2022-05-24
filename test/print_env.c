#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 


int main() { 
  char *x = (char*) malloc(139);
  char *fynnuname = (char*) malloc(128);
  free(fynnuname);
  strcpy(x, "Hellow mister :)\n");
  printf("%s", x);
  if (getenv("HTH_MSQ"))
    printf("Env MSQID: %s\n", getenv("HTH_MSQ"));
  else 
    printf("No HTH_MSQ Found!\n");
  if (getenv("LD_PRELOAD"))
    printf("preload: %s\n", getenv("LD_PRELOAD"));
  else 
    printf("No preload Found!\n");
  return 0;
}
