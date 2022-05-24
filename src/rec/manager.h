#ifndef HTH_MANAGER_H_
#define HTH_MANAGER_H_

#include "main.h"

#define MAXRECORD_STEPSIZE 16384

typedef struct {
  /* uint64_t id; */
  size_t size;
  uintptr_t mem_ptr;
  uintptr_t saved_ip;
} malloc_record;

typedef struct {
  /* uint64_t id; */
  size_t size;
  size_t nmemb;
  uintptr_t mem_ptr;
  uintptr_t saved_ip;
} calloc_record;

typedef struct {
  /* uint64_t id; */
  size_t size;
  uintptr_t old_ptr;
  uintptr_t mem_ptr;
  uintptr_t saved_ip;
} realloc_record;

typedef struct {
  /* uint64_t id; */
  size_t size;
  size_t alignment;
  uintptr_t mem_ptr;
  uintptr_t saved_ip;
} memalign_record;

typedef struct {
  /* uint64_t id; */
  int64_t param;
  int64_t value;
  int64_t retval;
} mallopt_record;


typedef uintptr_t free_record;

typedef struct {
  uintptr_t main_fn;
  char *libname[128];
  uintptr_t base_addr[128];
} newexec_record;

typedef struct {
  pid_t parent_pid;
  pid_t child_pid;
} fork_record;

newexec_record *current_exec;

enum record_ty {
  NEWEXEC = 0,
  CUSTOM,
  MALLOCTY,
  FREETY,
  CALLOCTY,
  REALLOCTY,
  MEMALIGNTY,
  FORK,
  MALLOPTTY,
};

typedef struct {
  enum record_ty ty;
  void *ptr;
  pid_t pid;
} heap_record;


typedef struct {
  size_t size;
  heap_record *records;
  char name[128];
  size_t max_size;
} operation_record;

operation_record *operations[1024];
uint64_t op_ctr = 0;

char *outfile = NULL;
char *exportsettings[1024];
uint64_t exportsetting_ctr = 0;

extern uint8_t export(FILE *fptr);


uint8_t mgr_next(uint8_t is_first, char *name);
uint8_t mgr_set(char *args[]);
uint8_t mgr_malloc(char *args[]);
uint8_t mgr_free(char *args[]);
uint8_t mgr_newexec(char *args[]);

uint8_t mgr_export(char *args[]);
uint8_t mgr_custom(char *args[], char *text);
uint8_t mgr_add_fork(char *args[]);

uint8_t mgr_handle_message(message *msg);
void start_listener();




#endif
