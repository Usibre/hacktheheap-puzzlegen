#ifndef HTH_MAIN_H_
#define HTH_MAIN_H_


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <string.h>
#include <stdint.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>

#define debug_print(fmt, ...) \
        do { fprintf(stderr, "%s:%d:%s(): \t" fmt, __FILE__, \
                                __LINE__, __func__, __VA_ARGS__); } while (0)

#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))

#define MESSAGE_MAXSIZE 8192
#define CONSOLE_BUFSIZE 1024
#define CONSOLE_ARGSIZE 16

#ifndef COMMENT_DEL
#define COMMENT_DEL '#'
#endif

uint8_t DBG = 0;
int msqid;
pid_t childpid;
char *program = NULL;

/* message structure */
typedef struct {
    long mtype;
    char mtext[MESSAGE_MAXSIZE];
} message;


//#ifndef HAVE_STRLCPY
/*
 * '_cups_strlcpy()' - Safely copy two strings.
 */
size_t                  /* O - Length of string */
strllcpy(char       *dst,        /* O - Destination string */
              const char *src,      /* I - Source string */
          size_t      size)     /* I - Size of destination string buffer */
{
  size_t    srclen;         /* Length of source string */
 /*
  * Figure out how much room is needed...
  */
  size --;
  srclen = strlen(src);
 /*
  * Copy the appropriate amount...
  */
  if (srclen > size)
    srclen = size;
  memcpy(dst, src, srclen);
  dst[srclen] = '\0';

  return (srclen);
}
//#endif /* !HAVE_STRLCPY */

char **str_split(char **pointers, char* line);
int main(int argc, char* argv[]);



#endif // HTH_MAIN_H_
