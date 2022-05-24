#include "stdlib.h" 



int main() { 
  char *x[5]; 
  for (int i = 0; i < 5; i++) { 
    x[i] = (char*)malloc(128);
  }
  free(x[3]);
  for (int i = 0; i < 5; i++) { 
    x[i] = (char*)malloc(128);
  }
  free(x[3]);
  return 0;
} 
