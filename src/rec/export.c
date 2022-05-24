#include "export.h"



uint8_t export_malloc(FILE *fptr, malloc_record *mal) {
  if (!fptr || !mal) {
    fprintf(stderr, "Failure exporting a malloc. \n");
    return EXIT_FAILURE;
  }
  fprintf(fptr, "\tMALLOC(%lu) @ 0x%lx = 0x%lx",
      mal->size, mal->saved_ip, mal->mem_ptr);
  return EXIT_SUCCESS;
}

uint8_t export_free(FILE *fptr, free_record *freerec) {
  if (!fptr || !freerec) {
    fprintf(stderr, "Failure exporting a free. \n");
    return EXIT_FAILURE;
  }
  fprintf(fptr, "\tFREE(0x%lx)", *freerec);
  return EXIT_SUCCESS;
}

uint8_t export_calloc(FILE *fptr, calloc_record *cal) {
  if (!fptr || !cal) {
    fprintf(stderr, "Failure exporting a calloc. \n");
    return EXIT_FAILURE;
  }
  if (CALLOC_AS_MALLOC) {
    malloc_record mal;
    mal.size = (cal->size*cal->nmemb);
    mal.mem_ptr = cal->mem_ptr;
    mal.saved_ip = cal->saved_ip;
    return export_malloc(fptr, &mal);
  }
  fprintf(fptr, "\tCALLOC(%lu, %lu) @ 0x%lx = 0x%lx",
      cal->nmemb, cal->size, cal->saved_ip, cal->mem_ptr);
  return EXIT_SUCCESS;
}

uint8_t export_realloc(FILE *fptr, realloc_record *real, pid_t pid) {
  if (!fptr || !real) {
    fprintf(stderr, "Failure exporting a realloc. \n");
    return EXIT_FAILURE;
  }
  if (REALLOC_AS_FREEMALLOC) {
    uint8_t res = export_free(fptr, (free_record*)&(real->old_ptr));
    if (res != EXIT_SUCCESS) return res;
    fprintf(fptr, " @ %d\n", pid);
    malloc_record mal;
    mal.size = real->size;
    mal.mem_ptr = real->mem_ptr;
    mal.saved_ip = real->saved_ip;
    return export_malloc(fptr, &mal);
  }
  fprintf(fptr, "\tREALLOC(0x%lx, %lu) @ 0x%lx = 0x%lx",
      real->old_ptr, real->size, real->saved_ip, real->mem_ptr);
  return EXIT_SUCCESS;
}

uint8_t export_memalign(FILE *fptr, memalign_record *memal, pid_t pid) {
  if (!fptr || !memal) {
    fprintf(stderr, "Failure exporting a memalign. \n");
    return EXIT_FAILURE;
  }
  fprintf(fptr, "\tMEMALIGN(%lu, %lu) @ 0x%lx = 0x%lx",
      memal->alignment, memal->size, memal->saved_ip, memal->mem_ptr);
  return EXIT_SUCCESS;
}


uint8_t export_mallopt(FILE *fptr, mallopt_record *mallopt, pid_t pid) {
  if (!fptr || !mallopt) {
    fprintf(stderr, "Failure exporting a mallopt. \n");
    return EXIT_FAILURE;
  }
  fprintf(fptr, "\tMALLOPT(%ld, %ld) = %ld",
      mallopt->param, mallopt->value, mallopt->retval);
  return EXIT_SUCCESS;
}

// export_mallopt
uint8_t export_newexec(FILE *fptr, newexec_record *exec_rec) {
  if (!fptr || !exec_rec) {
    fprintf(stderr, "Failure exporting a new exec. \n");
    return EXIT_FAILURE;
  }
  if (exec_rec->main_fn) {
    fprintf(fptr, "PROCESS_START 0x%lx", exec_rec->main_fn);
  } else {
    fprintf(fptr, "PROCESS_START");
  }
  // dynlibsss
  for (size_t i = 0; i < 128 && exec_rec->libname[i] != NULL; i++) {
    fprintf(fptr, "\n\tDYNAMIC %s @ 0x%lx",
        exec_rec->libname[i], exec_rec->base_addr[i]);
  }
  return EXIT_SUCCESS;
}

uint8_t export_custom(FILE *fptr, char *custom) {
  if (!fptr || !custom) {
    fprintf(stderr, "Failure exporting a custom. \n");
    return EXIT_FAILURE;
  }
  fprintf(fptr, "\t%s", custom);
  return EXIT_SUCCESS;
}

uint8_t export_settings(FILE *fptr) {
  if (!fptr) return EXIT_FAILURE;
  for (size_t i = 0; i < exportsetting_ctr; i++) {
    fprintf(fptr, "SET %s\n", exportsettings[i]);
  }
  if (program != NULL)
    fprintf(fptr, "START %s\n", program);
  else
    fprintf(fptr, "START\n");
  return EXIT_SUCCESS;
}

uint8_t export_fork(FILE *fptr, fork_record *rec) {
  if (!fptr || !rec) {
    fprintf(stderr, "Failure exporting a fork. \n");
    return EXIT_FAILURE;
  }
  fprintf(fptr, "\tFORK %d -> %d", rec->parent_pid, rec->child_pid);
  return EXIT_SUCCESS;
}

uint8_t export(FILE *fptr) {
  if (export_settings(fptr) != EXIT_SUCCESS)
    return EXIT_FAILURE;
  for (size_t i = 0; i <= op_ctr; i++) {
    operation_record *oprec = operations[i];
    fprintf(fptr, "NEXT %s\n", oprec->name);
    for (size_t j = 0; j < oprec->size; j++) {
      heap_record *hrecord = &(oprec->records[j]);
      switch (hrecord->ty) {
        case MALLOCTY:;
          if (export_malloc(fptr, (malloc_record*)hrecord->ptr) != EXIT_SUCCESS)
            return EXIT_FAILURE;
          break;
        case FREETY:;
          if (export_free(fptr, (free_record*)hrecord->ptr) != EXIT_SUCCESS)
            return EXIT_FAILURE;
          break;
        case CALLOCTY:;
          if (export_calloc(fptr, (calloc_record*)hrecord->ptr) != EXIT_SUCCESS)
            return EXIT_FAILURE;
          break;
        case REALLOCTY:;
          if (export_realloc(fptr, (realloc_record*)hrecord->ptr, hrecord->pid) != EXIT_SUCCESS)
            return EXIT_FAILURE;
          break;
        case MEMALIGNTY:;
          if (export_memalign(fptr, (memalign_record*)hrecord->ptr, hrecord->pid) != EXIT_SUCCESS)
            return EXIT_FAILURE;
          break;
        case MALLOPTTY:;
          if (export_mallopt(fptr, (mallopt_record*)hrecord->ptr, hrecord->pid) != EXIT_SUCCESS)
            return EXIT_FAILURE;
          break;
        case NEWEXEC:;
          if (export_newexec(fptr, (newexec_record*)hrecord->ptr) != EXIT_SUCCESS)
            return EXIT_FAILURE;
          break;
        case CUSTOM:;
          if (export_custom(fptr, (char*)hrecord->ptr) != EXIT_SUCCESS)
            return EXIT_FAILURE;
          break;
        case FORK:;
          if (export_fork(fptr, (fork_record*)hrecord->ptr) != EXIT_SUCCESS)
            return EXIT_FAILURE;
          break;
        default:
            fprintf(fptr, "\tUNKNOWN OP: %u\n", hrecord->ty);
            fprintf(stderr, "Did not recognise the op: %u\n", hrecord->ty);
            break;
      } // end switch
      fprintf(fptr, " @ %d\n", hrecord->pid);
    } // end inner for
  } // end outer for
  fprintf(fptr, "END");
  return EXIT_SUCCESS;
} // end fn
