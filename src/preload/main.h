#ifndef _H_PRELOAD_MAIN
#define _H_PRELOAD_MAIN

// for RTLD_NEXT, but already defined in the makefile
//#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include <link.h>
#include <unistd.h>

#ifndef MALMACRO
#define MALMACRO malloc
#endif
#ifndef FREEMACRO
#define FREEMACRO free
#endif
#ifndef CALLOCMACRO
#define CALLOCMACRO calloc
#endif
#ifndef REALLOCMACRO
#define REALLOCMACRO realloc
#endif

#ifndef NOCORRUPTION
#define NOCORRUPTION 0
#endif
#ifndef DBG
#define DBG 0
#endif

#define MESSAGE_MAXSIZE 8192


uint8_t preload_antirecursive = 0;
uint8_t initialised = 0;

// thx 2 Dennis Andriesse and his book "Practical Binary Analysis"
// although apparently dlym calls malloc which creates a recursive dependency
// so we needed a recursion check with a temp buffer 
// Note that POSIX has malloc hooks to that can alternatively be used.
void* (*orig_malloc)(size_t);
void (*orig_free)(void*);
pid_t (*orig_fork)(void);
void* (*orig_calloc)(size_t, size_t);
void* (*orig_realloc)(void*, size_t);
int (*orig_mallopt)(int, int);
// Additionals?
void *(*orig_reallocarray)(void*, size_t, size_t);
int (*orig_posix_memalign)(void **, size_t, size_t);
void *(*orig_memalign)(size_t, size_t);
void *(*orig_aligned_alloc)(size_t, size_t);
void *(*orig_pvalloc)(size_t);
void *(*orig_valloc)(size_t);


extern int main(int argc, char *argv[]) __attribute__((weak));

char *dyn_libs[256];

int msqid = 0;
/* message structure */
typedef struct {
    long mtype;
    char mtext[MESSAGE_MAXSIZE];
} message;


/*** Prototypes ***/
// To use by the user:

void hth_mark_next_bugged();
void hth_mark_next_target();
void hth_name_next_alloc(const char *name);
void hth_send_message(const char *msg);

// Internal:
void send_msg(const char *msg_contents, uint16_t size);
int is_new(char *dyn_lib);
void preload_init();
void *dummy_malloc(size_t s);
void dummy_free(void *ptr);
void init_msq();
void send_msg(const char *msg_contents, uint16_t size);
void check_for_new_ext_lib(void *saved_ip);
void send_msg_malloc(size_t s, void **p);
void send_msg_free(void *p);
void send_msg_calloc(size_t nmemb, size_t size, void **p);
void send_msg_realloc(size_t size, void **argp, void **retptr);
void send_msg_memalign(size_t size, size_t alignment, void **retptr);
// preload overwrites:
int mallopt(int param, int value);
void *MALMACRO(size_t s);
void FREEMACRO(void *p);
void *CALLOCMACRO(size_t nmemb, size_t size);
void *REALLOCMACRO(void *ptr, size_t size);
pid_t fork(void);
void *reallocarray(void *ptr, size_t nmemb, size_t size);
int posix_memalign(void **memptr, size_t alignment, size_t size);
void *memalign(size_t alignment, size_t size);
void *aligned_alloc(size_t alignment, size_t size);
void *valloc(size_t size);
void *pvalloc(size_t size);



#endif
