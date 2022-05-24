#include "main.h"



/******************
** Usable functions in target application:
** The next functions can be applied to the target binary right _before_ an alloc
** to add additional info. Note that linking is required apart from preloading,
** in order not to create linking issues when 'compiling'.
******************/

void hth_mark_next_bugged() {
  char *buf = "CUSTOM BUGGED N";
  send_msg(buf, strlen(buf));
}


void hth_mark_next_target() {
  char *buf = "CUSTOM TARGET N";
  send_msg(buf, strlen(buf));
}

void hth_name_next_alloc(const char *name) {
  char buf[MESSAGE_MAXSIZE];
  strcpy(buf, "CUSTOM NAME ");
  strncat(buf, name, MESSAGE_MAXSIZE-13);
  send_msg(buf, strlen(buf));
}
void hth_send_message(const char *msg) {
  send_msg(msg, strlen(msg));
}
/******************
** End usable functions in target application
******************/




int is_new(char *dyn_lib) {
  int i;
  for (i = 1; i < 255 && dyn_libs[i] != NULL; i++) {
    if (strcmp(dyn_lib, dyn_libs[i])==0) return 0;
  }
  return i;
}

void preload_init() {
  if (initialised) {
    #if DBG
      fprintf(stderr, "Preload init called after initialised?!");
    #endif
    return;
  }
  dyn_libs[0] = NULL;
  dyn_libs[1] = NULL;
  preload_antirecursive = 1;
  orig_malloc = dlsym(RTLD_NEXT, "malloc");
  #if DBG
    if (main)
      printf("Main found at: %p\n", main);
  #endif
  orig_free = dlsym(RTLD_NEXT, "free");
  orig_fork = dlsym(RTLD_NEXT, "fork");
  orig_calloc = dlsym(RTLD_NEXT, "calloc");
  orig_realloc = dlsym(RTLD_NEXT, "realloc");
  orig_reallocarray = dlsym(RTLD_NEXT, "reallocarray");
  orig_posix_memalign = dlsym(RTLD_NEXT, "posix_memalign");
  orig_memalign = dlsym(RTLD_NEXT, "memalign");
  orig_aligned_alloc = dlsym(RTLD_NEXT, "aligned_alloc");
  orig_pvalloc = dlsym(RTLD_NEXT, "pvalloc");
  orig_valloc = dlsym(RTLD_NEXT, "valloc");
  #if DBG
    if (!orig_malloc)
      fprintf(stderr, "No malloc found :/\n");
    if (!orig_free)
      fprintf(stderr, "No free found :/\n");
    if (!orig_calloc)
      fprintf(stderr, "No calloc found :/\n");
    if (!orig_realloc)
      fprintf(stderr, "No realloc found :/\n");
    if (!orig_malloc || !orig_free || !orig_calloc || !orig_realloc)
      fprintf(stderr, "%s :/\n", dlerror());
  #endif
  if (main) {
    char buf[128];
    sprintf(buf, "NEWEXEC 0x%lx", (uintptr_t)main);
    send_msg(buf, 128);
  } else {
    send_msg("NEWEXEC", strlen("NEWEXEC"));
  }
  initialised = 1;
  preload_antirecursive = 0;
}

char dummymalloc_buf[1024*512];
size_t dummymalloc_ptr = 0;
void *dummy_malloc(size_t s) {
  if (dummymalloc_ptr + s > 1024*512) {
    fprintf(stderr, "CRITICAL! dummymalloc is full!");
    return 0;
  }
  char *ptr = dummymalloc_buf;
  ptr += dummymalloc_ptr;
  dummymalloc_ptr += s;
  return (void*) ptr;
}

void dummy_free(void *ptr){return;}

void init_msq() {
  char *env_var = "HTH_MSQ";
  if (getenv(env_var)) {
    int status = sscanf(getenv(env_var), "%d", &msqid);
    if (DBG && status != 1) {
      fprintf(stderr, "Reading the msqid failed.\n");
    }
  } else {
    // No MSQID is set, don't set msqid to make sure no message is sent
    // msgsnd is blocking.
    //fprintf(stderr, "Not implemented yet, please set the env var for the time being. \n");
  }
}


void send_msg(const char *msg_contents, uint16_t size) {
  if (!msqid) init_msq();
  if (!msqid) return;
  #if DBG
    fprintf(stderr, "Sending message to %d: %s\n", msqid, msg_contents);
  #endif
  message msg;
  msg.mtype = 23; // >?
  snprintf(msg.mtext, MESSAGE_MAXSIZE, "%s :%u", msg_contents, getpid());
  fprintf(stderr, "New message: %s\n", msg.mtext);
  if (msgsnd(msqid, &msg, strlen(msg.mtext), 0) == -1 && DBG) {
    perror("msgsnd");
    exit(EXIT_FAILURE);
  }
}

void check_for_new_ext_lib(void* saved_ip) {
  Dl_info info;
  struct link_map *extra_info;
  int success = dladdr1(saved_ip, &info, (void**)&extra_info, RTLD_DL_LINKMAP);
  if (!success)
    return;
  // ignore if it's the local file
  if (strlen(extra_info->l_name)<1)
    return;
  int res = is_new(extra_info->l_name);
  if (res == 0 || res==255) return;
  dyn_libs[res] = extra_info->l_name;
  dyn_libs[res+1] = NULL;
  char msg[MESSAGE_MAXSIZE];
  char buf[128];
  strcpy(msg, "EXTERNAL ADD ");
  strcat(msg, extra_info->l_name);
  strcat(msg, " @ ");
  sprintf(buf, "0x%lx", (uintptr_t)info.dli_fbase);
  strcat(msg, buf);
  send_msg(msg, strlen(msg));
}

void send_msg_malloc(size_t s, void** p) {
  if ((!orig_malloc) && (!preload_antirecursive)) {
      preload_init();
  }
  char msg[MESSAGE_MAXSIZE];
  char buf[128];
  strcpy(msg, "MALLOC ");
  sprintf(buf, "%lu", s);
  strcat(msg, buf);
  strcat(msg, " ");
  sprintf(buf, "0x%lx", (uintptr_t)*p);
  strcat(msg, buf);
  // we assume a 64-bit arch 
  // and system-V calling conventions
  intptr_t *saved_ip =(intptr_t*)( (intptr_t)p + 5*sizeof(p) );
  check_for_new_ext_lib((void*)*saved_ip);
  if (DBG) fprintf(stderr, "Saved ip at %p? 0x%lx\n", saved_ip, *saved_ip);
  strcat(msg, " ");
  sprintf(buf, "0x%lx", *saved_ip);
  strcat(msg, buf);
  send_msg(msg, strlen(msg));
}

void send_msg_free(void *p) {
  if ((!orig_malloc) && (!preload_antirecursive)) {
      preload_init();
  }
  char msg[MESSAGE_MAXSIZE];
  char buf[128];
  strcpy(msg, "FREE ");
  sprintf(buf, "0x%lx", (uintptr_t)p);
  strcat(msg, buf);
  send_msg(msg, strlen(msg));
}


void send_msg_calloc(size_t nmemb, size_t size, void** p) {
  if ((!orig_malloc) && (!preload_antirecursive)) {
      preload_init();
  }
  char msg[MESSAGE_MAXSIZE];
  char buf[128];
  strcpy(msg, "CALLOC ");
  sprintf(buf, "%lu ", nmemb);
  strcat(msg, buf);
  sprintf(buf, "%lu", size);
  strcat(msg, buf);
  strcat(msg, " ");
  sprintf(buf, "0x%lx", (uintptr_t)*p);
  strcat(msg, buf);
  // we assume a 64-bit arch
  // and system-V calling conventions
  intptr_t *saved_ip =(intptr_t*)( (intptr_t)p + 5*sizeof(p) );
  check_for_new_ext_lib((void*)*saved_ip);
  if (DBG) fprintf(stderr, "Saved ip at 0x%lx? 0x%lx\n", (uintptr_t)saved_ip, *saved_ip);
  strcat(msg, " ");
  sprintf(buf, "0x%lx", *saved_ip);
  strcat(msg, buf);
  send_msg(msg, strlen(msg));
}


void send_msg_realloc(size_t size, void** argp, void **retptr) {
  if ((!orig_malloc) && (!preload_antirecursive)) {
      preload_init();
  }
  char msg[MESSAGE_MAXSIZE];
  char buf[128];
  strcpy(msg, "REALLOC ");
  sprintf(buf, "0x%lx %lu 0x%lx", ((uintptr_t)*argp), size, (uintptr_t)*retptr);
  strcat(msg, buf);
  intptr_t *saved_ip =(intptr_t*)( (intptr_t)retptr + 5*sizeof(retptr) );
  check_for_new_ext_lib((void*)*saved_ip);
  if (DBG) fprintf(stderr, "Saved ip at 0x%lx? 0x%lx\n", (uintptr_t)saved_ip, *saved_ip);
  strcat(msg, " ");
  sprintf(buf, "0x%lx", *saved_ip);
  strcat(msg, buf);
  send_msg(msg, strlen(msg));
}

void send_msg_memalign(size_t size, size_t alignment, void **retptr) {
  if ((!orig_malloc) && (!preload_antirecursive)) {
      preload_init();
  }
  char msg[MESSAGE_MAXSIZE];
  char buf[128];
  strcpy(msg, "MEMALIGN ");
  sprintf(buf, "%lu, %lu 0x%lx", size, alignment, (uintptr_t)*retptr);
  strcat(msg, buf);
  intptr_t *saved_ip =(intptr_t*)( (intptr_t)retptr + 5*sizeof(retptr) );
  check_for_new_ext_lib((void*)*saved_ip);
  if (DBG) fprintf(stderr, "Saved ip at 0x%lx? 0x%lx\n", (uintptr_t)saved_ip, *saved_ip);
  strcat(msg, " ");
  sprintf(buf, "0x%lx", *saved_ip);
  strcat(msg, buf);
  send_msg(msg, strlen(msg));
}

int mallopt(int param, int value) {
  if (!orig_mallopt) {
    if (!preload_antirecursive) {
      preload_init();
    }
  }
  int x = orig_mallopt(param, value);
  char buf[128];
  sprintf(buf, "MALLOPT %d %d %d", param, value, x);
  send_msg(buf, strlen(buf));
  return x;
}

//void *malloc(size_t s) {
void *MALMACRO(size_t s) {
  if (!orig_malloc) {
    if (!preload_antirecursive) {
      preload_init();
    } else {
      return dummy_malloc(s);
    }
  }
  #if NOCORRUPTION
    size_t ds = 3*s;
    if (ds < s) ds = 2*s;
    if (ds < s) ds = s;
    void *ptr = orig_malloc(ds);
  #else
    void *ptr = orig_malloc(s);
  #endif
  if (!preload_antirecursive) {
    preload_antirecursive = 1;
    send_msg_malloc(s, &ptr);
    preload_antirecursive = 0;
  }
  return ptr;
}

//void free(void *p) {
void FREEMACRO(void *p) {
  if (!orig_free) {
    if (!preload_antirecursive) preload_init();
    else dummy_free(p);
    return;
  }
  orig_free(p);
  if (!preload_antirecursive) {
    preload_antirecursive = 1;
    send_msg_free(p);
    preload_antirecursive = 0;
  }
  return;
}


void *CALLOCMACRO(size_t nmemb, size_t size) {
  if (!orig_calloc) {
    if (!preload_antirecursive) preload_init();
    else {
      void *x = dummy_malloc(nmemb*size);
      memset(x, 0, nmemb*size);
      return x;
    }
  }
  #if NOCORRUPTION
    size_t ds = 3*size;
    if (ds < size) ds = 2*size;
    if (ds < size) ds = size;
    void *retptr = orig_calloc(nmemb, ds);
  #else
    void *retptr = orig_calloc(nmemb, size);
  #endif
  if (!preload_antirecursive) {
    preload_antirecursive = 1;
    send_msg_calloc(nmemb, size, &retptr);
    preload_antirecursive = 0;
  }
  return retptr;
}

void *REALLOCMACRO(void *ptr, size_t size) {
  if (!orig_calloc) {
    if (!preload_antirecursive) preload_init();
    else {

      void *ptr2 = dummy_malloc(size);
      memcpy(ptr2, ptr, size);
      return ptr2;
    }
  }
  #if NOCORRUPTION
    size_t ds = 3*size;
    if (ds < size) ds = 2*size;
    if (ds < size) ds = size;
    void *retptr = orig_realloc(ptr, ds);
  #else
    void *retptr = orig_realloc(ptr, size);
  #endif
  if (!preload_antirecursive) {
    preload_antirecursive = 1;
    send_msg_realloc(size, &ptr, &retptr);
    preload_antirecursive = 0;
  }
  return retptr;
}


pid_t fork(void) {
  if (!orig_fork) {
    if (!preload_antirecursive) {
      preload_init();
    }
  }
  usleep(20000); // 1000000 == 1 second, so this is 0.02 seconds
  pid_t result = orig_fork();
  if (result < 0) {
    // failure
    send_msg("FORK FAILURE", strlen("FORK FAILURE"));
  } else if (result == 0) {
    //send_msg("FORK child", strlen("FORK child"));
  } else {
    char buf[MESSAGE_MAXSIZE];
    snprintf(buf, MESSAGE_MAXSIZE, "FORK INTO %d FROM", result);
    send_msg(buf, strlen(buf));
  }
  // sleep a bit more before returning to register the fork at the right time?
  usleep(20000); // 1000000 == 1 second, so this is 0.02 seconds
  return result;
}
void *reallocarray(void *ptr, size_t nmemb, size_t size) {
  if (!orig_reallocarray) {
    if (!preload_antirecursive) {
      preload_init();
    }
  }
  #if NOCORRUPTION
    size_t ds = 3*size;
    if (ds < size) ds = 2*size;
    if (ds < size) ds = size;
    void *x = orig_reallocarray(ptr, nmemb, ds);
  #else
    void *x = orig_reallocarray(ptr, nmemb, size);
  #endif
  send_msg_realloc(size*nmemb, &ptr, &x);
  return x;
}
int posix_memalign(void **memptr, size_t alignment, size_t size) {
  if (!orig_posix_memalign) {
    if (!preload_antirecursive) {
      preload_init();
    }
  }
  #if NOCORRUPTION
    size_t ds = 3*size;
    if (ds < size) ds = 2*size;
    if (ds < size) ds = size;
    int x = orig_posix_memalign(memptr, alignment, ds);
  #else
    int x = orig_posix_memalign(memptr, alignment, size);
  #endif
  send_msg_memalign(size, alignment, memptr);
  return x;
}
void *memalign(size_t alignment, size_t size) {
  if (!orig_memalign) {
    if (!preload_antirecursive) {
      preload_init();
    }
  }
  #if NOCORRUPTION
    size_t ds = 3*size;
    if (ds < size) ds = 2*size;
    if (ds < size) ds = size;
    void *x = orig_memalign(alignment, ds);
  #else
    void *x = orig_memalign(alignment, size);
  #endif
  send_msg_memalign(size, alignment, &x);
  return x;
}
void *aligned_alloc(size_t alignment, size_t size) {
  if (!orig_aligned_alloc) {
    if (!preload_antirecursive) {
      preload_init();
    }
  }
  #if NOCORRUPTION
    size_t ds = 3*size;
    if (ds < size) ds = 2*size;
    if (ds < size) ds = size;
    void *x = orig_aligned_alloc(alignment, ds);
  #else
    void *x = orig_aligned_alloc(alignment, size);
  #endif
  send_msg_memalign(size, alignment, &x);
  return x;
}
void *valloc(size_t size) {
  if (!orig_aligned_alloc) {
    if (!preload_antirecursive) {
      preload_init();
    }
  }
  #if NOCORRUPTION
    size_t ds = 3*size;
    if (ds < size) ds = 2*size;
    if (ds < size) ds = size;
    void *x = orig_valloc(ds);
  #else
    void *x = orig_valloc(size);
  #endif
  send_msg_memalign(size, _SC_PAGESIZE, &x);
  return x;
}
void *pvalloc(size_t size) {
  if (!orig_aligned_alloc) {
    if (!preload_antirecursive) {
      preload_init();
    }
  }
  #if NOCORRUPTION
    size_t ds = 3*size;
    if (ds < size) ds = 2*size;
    if (ds < size) ds = size;
    void *x = orig_pvalloc(ds);
  #else
    void *x = orig_pvalloc(size);
  #endif
  send_msg_memalign(size, _SC_PAGESIZE, &x);
  return x;
}
