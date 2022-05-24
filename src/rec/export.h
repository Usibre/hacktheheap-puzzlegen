#ifndef HTH_EXPORT_H_
#define HTH_EXPORT_H_

#include "manager.h"
#include "main.h"

#define MALLOC_FREE_ONLY 0

#if MALLOC_FREE_ONLY
  #define REALLOC_AS_FREEMALLOC 1
  #define CALLOC_AS_MALLOC 1
#else
  #define REALLOC_AS_FREEMALLOC 0
  #define CALLOC_AS_MALLOC 0
#endif

uint8_t export_malloc(FILE *fptr, malloc_record *mal);
uint8_t export_free(FILE *fptr, free_record *freerec);
uint8_t export_calloc(FILE *fptr, calloc_record *cal);
uint8_t export_realloc(FILE *fptr, realloc_record *real, pid_t pid);
uint8_t export_newexec(FILE *fptr, newexec_record *exec_rec);
uint8_t export_custom(FILE *fptr, char *custom);
uint8_t export_settings(FILE *fptr);
uint8_t export_fork(FILE *fptr, fork_record *rec);
uint8_t export(FILE *fptr);

#endif
