#include "manager.h"
#include "main.h"


uint8_t len(char *args[]) {
  uint8_t x = 0;
  while (args[x++] != NULL) {}
  return x;
}

uint8_t mgr_next(uint8_t is_first, char *name) {
  if (!is_first) op_ctr++;
  if (op_ctr >= 1024) {
    fprintf(stderr, "Too many operations to handle! \n");
    return 1;
  }
  operations[op_ctr] = (operation_record*) malloc(sizeof(operation_record));
  operations[op_ctr]->records = (heap_record*) malloc(sizeof(heap_record)*MAXRECORD_STEPSIZE);
  operations[op_ctr]->size = 0;
  operations[op_ctr]->max_size = MAXRECORD_STEPSIZE;
  if (name) {
    strllcpy(operations[op_ctr]->name, name, 127);
    operations[op_ctr]->name[127] = '\0';
  } else {
    operations[op_ctr]->name[0] = '\0';
  }
  return 0;
}

uint8_t mgr_set(char *args[]) {
  if (!args || !args[1] || !args[2]) return EXIT_FAILURE;
  // we assume here that args[0] == "set"
  if (strcmp(args[1], "debug") == 0 || strcmp(args[1], "dbg") == 0) {
    sscanf(args[2], "%hhu", &DBG);
    if (DBG) fprintf(stderr, "Mgr debug: \tDebug mode turned on in mgr.\n");
    return 0;
  }
  if (strcmp(args[1], "outfile") == 0 ||
      strcmp(args[1], "out") == 0) {
    if (!outfile) {
      outfile = (char*) malloc(1024);
    }
    strllcpy(outfile, args[2], 1023);
    outfile[1023] = '\0';
    if (DBG) fprintf(stderr, "Mgr debug: \tOutput file set to %s\n", outfile);
    return 0;
  }
  if (strcmp(args[1], "exec")==0) {
    if (program == NULL) program = (char*)malloc(256);
    strllcpy(program, args[2], 255);
    program[255] = '\0';
    return 0;
  }

  // default setting, add to export
  if (exportsetting_ctr >= 1024) {
    fprintf(stderr, "Critical error: exportsettings full!!\n");
    return EXIT_FAILURE;
  }
  exportsettings[exportsetting_ctr] = (char*)malloc(256);
  snprintf(exportsettings[exportsetting_ctr++], 255, "%s %s", args[1], args[2]);
  if (DBG) {
    fprintf(stderr, "Mgr debug: \texported option %s -> %s\n", args[1], args[2]);
  }
  return EXIT_SUCCESS;
}

uint8_t mgr_export(char *args[]) {
  if (!outfile) {
    fprintf(stderr, "No output file set!\n");
    return 1;
  }
  FILE *fptr = fopen(outfile, "w");
  if (!fptr) {
    fprintf(stderr, "Failed to open export file (%s)\n", outfile);
    return 1;
  }
  if (DBG) fprintf(stderr, "Mgr debug: \tExporting data to %s...\n", outfile);
  uint8_t res = export(fptr);
  if (DBG) fprintf(stderr, "Mgr debug: \tDone writing to file, closing.\n");
  fclose(fptr);
  return res;
  for (size_t i = 0; i < exportsetting_ctr; i++) {
    fprintf(fptr, "SET %s\n", exportsettings[i]);
  }
  if (program != NULL)
    fprintf(fptr, "START %s\n", program);
  else
    fprintf(fptr, "START\n");
  for (size_t i = 0; i <= op_ctr; i++) {
    operation_record *oprec = operations[i];
    fprintf(fptr, "NEXT %s\n", oprec->name);
    for (size_t j = 0; j < oprec->size; j++) {
      heap_record *hrecord = &(oprec->records[j]);
      switch (hrecord->ty) {
        case MALLOCTY:;
          malloc_record *mal = (malloc_record*) hrecord->ptr;
          if (!mal) {
            fprintf(stderr, "panic: Malloc record is null?!\n");
            return 1;
          }
          fprintf(fptr, "\tMALLOC(%lu) @ 0x%lx = 0x%lx\n", mal->size, mal->saved_ip, mal->mem_ptr);
          break;
        case FREETY:;
          free_record *freerec = (free_record*) hrecord->ptr;
          fprintf(fptr, "\tFREE(0x%lx)\n", *freerec);
          break;
        case NEWEXEC:;
          break;
        case CUSTOM:;
          fprintf(fptr, "\t%s\n", (char*)hrecord->ptr);
          break;
        default:
          fprintf(fptr, "\tUNKNOWN OP: %u\n", hrecord->ty);
          fprintf(stderr, "Did not recognise the op: %u\n", hrecord->ty);
          break;
      }
    } // end inner for
  } // end outer for
  fprintf(fptr, "END");
  if (DBG) fprintf(stderr, "Mgr debug: \tDone writing to file, closing.\n");
  fclose(fptr);
  return 0;
}

uint8_t mgr_malloc(char *args[]) {
  // 1 = size (in bytes ofc, %lu), 2 = resulting pointer (%p), 3 = saved ip (0x%lx)
  malloc_record *mr = (malloc_record*) malloc(sizeof(malloc_record));
  if (!mr) {
    fprintf(stderr, "Critical error: malloc failed upon saving a malloc record.\n");
    return 1;
  }
  sscanf(args[1], "%lu", &(mr->size));
  sscanf(args[2], "0x%lx", &(mr->mem_ptr));
  sscanf(args[3], "0x%lx", &(mr->saved_ip));
  if (DBG) fprintf(stderr, "Mgr debug: \tSaving new malloc(%lu) -> 0x%lx, 0x%lx @ %p in (%lu,%lu).\n",
              mr->size, mr->mem_ptr, mr->saved_ip, mr, op_ctr, operations[op_ctr]->size);
  size_t index = operations[op_ctr]->size;
  operations[op_ctr]->records[index].ptr = (void*) mr;
  operations[op_ctr]->records[index].ty = MALLOCTY;
  if (args[4] != NULL  && strlen(args[2])>1)
    sscanf(args[4], ":%d", &(operations[op_ctr]->records[index].pid));
  else
    if (DBG) fprintf(stderr, "Mgr debug: \tMalloc has no associated pid :(\n");
  operations[op_ctr]->size++;
  return 0;
}

uint8_t mgr_calloc(char *args[]) {
  // CALLOC nmemb size retptr saved_ip
  // 1 = nmemb, 2 = size, 3 = retptr, 4 = savedip, 5 = [pid]
  calloc_record *cr = (calloc_record*) malloc(sizeof(calloc_record));
  if (!cr) {
    fprintf(stderr, "Critical error: malloc failed upon saving a calloc record.\n");
    return 1;
  }
  sscanf(args[1], "%lu", &(cr->nmemb));
  sscanf(args[2], "%lu", &(cr->size));
  sscanf(args[3], "0x%lx", &(cr->mem_ptr));
  sscanf(args[4], "0x%lx", &(cr->saved_ip));
  size_t index = operations[op_ctr]->size;
  operations[op_ctr]->records[index].ptr = (void*) cr;
  operations[op_ctr]->records[index].ty = CALLOCTY;
  if (args[5] != NULL  && strlen(args[5])>1) {
    sscanf(args[5], ":%d", &(operations[op_ctr]->records[index].pid));
  } else {
    if (DBG) fprintf(stderr, "Mgr debug: \tCalloc has no associated pid :(  ):\n");
  }
  operations[op_ctr]->size++;
  return 0;
}

uint8_t mgr_realloc(char *args[]) {
  // REALLOC oldptr size retptr saved_ip
  realloc_record *rr = (realloc_record*) malloc(sizeof(realloc_record));
  if (!rr) {
    fprintf(stderr, "Critical error: malloc failed upon saving a realloc record.\n");
    return 1;
  }
  sscanf(args[1], "0x%lx", &(rr->old_ptr));
  sscanf(args[2], "%lu", &(rr->size));
  sscanf(args[3], "0x%lx", &(rr->mem_ptr));
  sscanf(args[4], "0x%lx", &(rr->saved_ip));
  size_t index = operations[op_ctr]->size;
  operations[op_ctr]->records[index].ptr = (void*) rr;
  operations[op_ctr]->records[index].ty = REALLOCTY;
  if (args[5] != NULL  && strlen(args[2])>1)
    sscanf(args[5], ":%d", &(operations[op_ctr]->records[index].pid));
  else
    if (DBG) fprintf(stderr, "Mgr debug: \tRealloc has no associated pid :(\n");
  operations[op_ctr]->size++;
  return 0;
}

uint8_t mgr_free(char *args[]) {
  // 1 = pointer to be freed, that's it for now iirc
  // The malloc itself with its return pointer should provide enough
  // information to determine the name of the malloc itself and stuff :)
  free_record *freeptr = (free_record*) malloc(sizeof(free_record));
  if (!freeptr) {
    fprintf(stderr, "Critical error: malloc failed upon saving a free record.\n");
    return 1;
  }
  sscanf(args[1], "0x%lx", freeptr);
  // no need to save if it's a free(0)
  if (*freeptr == 0x0) {
    if (DBG) fprintf(stderr, "Mgr debug: \tSkipping new free(0x0) @ %p\n", freeptr);
    return 0;
  }
  if (DBG) fprintf(stderr, "Mgr debug: \tSaving new free(0x%lx)\n", *freeptr);
  size_t index = operations[op_ctr]->size;
  operations[op_ctr]->records[index].ptr = (void*) freeptr;
  operations[op_ctr]->records[index].ty = FREETY;
  if (args[2] != NULL  && strlen(args[2])>1)
    sscanf(args[2], ":%d", &(operations[op_ctr]->records[index].pid));
  operations[op_ctr]->size++;
  return 0;
}

uint8_t mgr_memalign(char *args[]) {
  // REALLOC oldptr size retptr saved_ip
  memalign_record *mr = (memalign_record*) malloc(sizeof(memalign_record));
  if (!mr) {
    fprintf(stderr, "Critical error: malloc failed upon saving a realloc record.\n");
    return 1;
  }
  sscanf(args[1], "%lu", &(mr->size));
  sscanf(args[2], "%lu", &(mr->alignment));
  sscanf(args[3], "0x%lx", &(mr->mem_ptr));
  sscanf(args[4], "0x%lx", &(mr->saved_ip));
  size_t index = operations[op_ctr]->size;
  operations[op_ctr]->records[index].ptr = (void*) mr;
  operations[op_ctr]->records[index].ty = MEMALIGNTY;
  if (args[5] != NULL  && strlen(args[2])>1)
    sscanf(args[5], ":%d", &(operations[op_ctr]->records[index].pid));
  else
    if (DBG) fprintf(stderr, "Mgr debug: \tRealloc has no associated pid :(\n");
  operations[op_ctr]->size++;
  return 0;
}

uint8_t mgr_mallopt(char *args[]) {
  mallopt_record *mr = (mallopt_record*) malloc(sizeof(mallopt_record));
  sscanf(args[1], "%ld", &(mr->param));
  sscanf(args[2], "%ld", &(mr->value));
  sscanf(args[3], "%ld", &(mr->retval));
  size_t index = operations[op_ctr]->size;
  operations[op_ctr]->records[index].ptr = (void*) mr;
  operations[op_ctr]->records[index].ty = MALLOPTTY;
  if (args[4] != NULL  && strlen(args[4])>1)
    sscanf(args[4], ":%d", &(operations[op_ctr]->records[index].pid));
  else
    if (DBG) fprintf(stderr, "Mgr debug: \tmalloopt has no associated pid :(\n");
  operations[op_ctr]->size++;
  return 0;
}

uint8_t mgr_newexec(char *args[]) {
  size_t index = operations[op_ctr]->size;
  operations[op_ctr]->records[index].ty = NEWEXEC;
  newexec_record *exec_ptr = (newexec_record*) malloc(sizeof(newexec_record));
  exec_ptr->libname[0] = (char*)0;
  if (args[1]) {
    sscanf(args[1], "0x%lx",&(exec_ptr->main_fn));
  } else {
    exec_ptr->main_fn = 0;
  }
  operations[op_ctr]->records[index].ptr = (void*) exec_ptr;
  if (args[2] != NULL && strlen(args[2])>1)
    sscanf(args[2], ":%d", &(operations[op_ctr]->records[index].pid));
  operations[op_ctr]->size++;
  current_exec = exec_ptr;
  return 0;
}

uint8_t mgr_custom(char *args[], char *text) {
  if (!args[1]) {
    fprintf(stderr, "No custom data specified.\n");
    return 1;
  }
  if (DBG) fprintf(stderr, "Mgr debug: \tSaving custom message: '%s'\n", text+strlen(args[0])+1);
  char *custom_text = (char*) malloc(1024);
  strllcpy(custom_text, text+strlen(args[0])+1, 1023);
  custom_text[1023] = '\0';
  size_t index = operations[op_ctr]->size;
  operations[op_ctr]->records[index].ty = CUSTOM;
  operations[op_ctr]->records[index].ptr = (void*)custom_text;
  operations[op_ctr]->records[index].pid = 0;
  for (int i = 1; i < 4; i++) {
    if (args[i] == NULL)
      break;
    if (strlen(args[i])>1 && args[i][0]==':') {
      sscanf(args[i], ":%d", &(operations[op_ctr]->records[index].pid));
      size_t total_length = 0;
      for (int j = 1; j < i; j++) {
        total_length += strlen(args[j])+1;
      }
      ((char*)(operations[op_ctr]->records[index].ptr))[total_length] = '\0';
      break;
    } else {
    }
  }
  operations[op_ctr]->size++;
  return 0;
}

uint8_t mgr_add_fork(char *args[]) {
  // ARGS contains 'FORK INTO [pid] FROM :[parent_pid]'
  fork_record *rec = (fork_record*)malloc(sizeof(fork_record));
  sscanf(args[2], "%d", &(rec->child_pid));
  sscanf(args[4], ":%d", &(rec->parent_pid));
  // and send it
  size_t index = operations[op_ctr]->size;
  operations[op_ctr]->records[index].ty = FORK;
  operations[op_ctr]->records[index].ptr = (void*)rec;
  operations[op_ctr]->records[index].pid = rec->parent_pid;
  operations[op_ctr]->size++;
  return 0;
}

uint8_t mgr_add_external(char *args[]) {
  /*
  Mgr debug: 	received: 'EXTERNAL ADD /lib/x86_64-linux-gnu/libc.so.6 @ 0x7ffff580b000'
  */
  size_t i;
  for (i = 0; i < 128 && current_exec->libname[i] != 0 ; i++) {
    // current exec already saved this library
    if (DBG) {
      printf("Comparing against %p...\n", current_exec->libname[i]);
      printf("Comparing against %s...\n", current_exec->libname[i]);
    }
    if (strcmp(args[2], current_exec->libname[i])==0) {
      return 0;
    }
  }
  if (i == 128) {
    fprintf(stderr, "External library space full :(\n");
    return EXIT_FAILURE;
  }
  char *ext_lib = malloc(128);
  if (ext_lib == NULL) {
    fprintf(stderr, "Malloc space full :(\n");
    return EXIT_FAILURE;
  }
  strllcpy(ext_lib, args[2], 127);
  ext_lib[127] = '\0';
  intptr_t base_addr;
  sscanf(args[4], "0x%lx", &base_addr);
  current_exec->libname[i] = ext_lib;
  current_exec->base_addr[i] = base_addr;
  if (i+1 < 128)
    current_exec->libname[i+1] = (char*)0;
  return 0;
}

uint8_t mgr_handle_message(message* msg) {
  if (DBG) fprintf(stderr, "Mgr debug: \treceived: '%s'\n", msg->mtext);
  if (msg->mtext[0]==COMMENT_DEL) return 0;
  if (operations[op_ctr]->size >= operations[op_ctr]->max_size) {
    operations[op_ctr]->max_size += MAXRECORD_STEPSIZE;
    operations[op_ctr]->records =
        realloc(operations[op_ctr]->records, sizeof(heap_record)*operations[op_ctr]->max_size);
    if (operations[op_ctr] == NULL) {
      fprintf(stderr, "Critical error: Operation record full!!\n");
      return EXIT_FAILURE;
    }
  }
  char *args[CONSOLE_ARGSIZE];
  char buff[MESSAGE_MAXSIZE];
  memcpy(buff, msg->mtext, MESSAGE_MAXSIZE-1);
  buff[MESSAGE_MAXSIZE-1] = '\0';
  str_split(args, buff);
  if (!args[0]) return EXIT_FAILURE;
  if (strcmp(args[0], "set")==0) {
    return mgr_set(args);
  }
  if ((strcmp(args[0], "next")==0)||(strcmp(args[0], "NEXT")==0)) {
    return mgr_next(0, args[1]);
  }
  if (strcmp(args[0], "save")==0 ||
      strcmp(args[0], "export") == 0) {
    return mgr_export(args);
  }
  if (strcmp(args[0], "MALLOC")==0) {
    return mgr_malloc(args);
  }
  if (strcmp(args[0], "CALLOC")==0) {
    return mgr_calloc(args);
  }
  if (strcmp(args[0], "REALLOC")==0) {
    return mgr_realloc(args);
  }
  if (strcmp(args[0], "MEMALIGN")==0) {
    return mgr_memalign(args);
  }
  if (strcmp(args[0], "FREE")==0) {
    return mgr_free(args);
  }
  if (strcmp(args[0], "NEWEXEC")==0) {
    return mgr_newexec(args);
  }
  if (strcmp(args[0], "custom")==0 ||
      strcmp(args[0], "CUSTOM")==0) {
    return mgr_custom(args, msg->mtext);
  }
  if (strcmp(args[0], "EXTERNAL")==0 &&
      strcmp(args[1], "ADD")==0 &&
      strcmp(args[3], "@")==0) {
    return mgr_add_external(args);
  }
  if (strcmp(args[0], "FORK")==0 &&
      strcmp(args[1], "INTO")==0) {
    // successful fork
    return mgr_add_fork(args);
  }
  return EXIT_FAILURE;
}

void start_listener() {
  message msg;
  do {
    char *args[] = {NULL,NULL,NULL};
  } while (0);
  uint8_t status = mgr_next(1, "init");

  while (1) {
    if (msgrcv(msqid, &msg, MESSAGE_MAXSIZE, 0, 0) != -1) {
      status = mgr_handle_message(&msg);
      if (DBG) fprintf(stderr, "Mgr debug: \tStatus: %hhu\n", status);
      if (strncmp("QUIT", msg.mtext, 4)==0)
        exit(0);
      memset(msg.mtext, 0, MESSAGE_MAXSIZE);
    } else {
      // sth went wrong
      fprintf(stderr, "Something went wrong :(\n");
      if (errno == ENOMSG) {
        // should not happen because IPC_NOWAIT is not set
      } else {
        fprintf(stderr, "Something went vwewy wrong :(\n");
        perror("msgrcv");
        exit(EXIT_FAILURE);
      }
    } // else end
  } // while true end
} // fcn end
