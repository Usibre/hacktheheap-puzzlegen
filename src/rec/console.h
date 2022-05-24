#ifndef HTH_CONSOLE_H_
#define HTH_CONSOLE_H_

#include "main.h"


#define CONSOLE_BUFSIZE 1024
#define CONSOLE_ARGSIZE 16



#define READ_END 0
#define WRITE_END 1



// for a environment loop
extern char **environ;

pid_t program_pid = 0;
char *envs[64];
char last_cmd[CONSOLE_BUFSIZE];
int execve_pipe[2];
unsigned int next_sleeptime = 0;
char **args = NULL;

void send_msg(char *msg_contents, uint16_t size);
int read_config_file(char *filename);
int _is_running();


int next_operation(char **args);
int save_operation(char **args);
int nop_operation(char **args);
int sleep_operation(char **args);
int exit_operation(char **args);
int show_operation(char **args);
int set_operation(char **args);
int start_operation(char **args);
int interact_operation(char **args);
int stop_operation(char **args);
int restart_operation(char **args);

int copy_env();
int copy_operation(char **args);

int exec_operation(char **args);

int add_env_var(char **args);
int help_operation(char **args);
int next_operation(char **args);

int send_direct(char **args);
int console_exec(char **args);

int console_handle_line(char *line);
char *console_read_line(char *buffer);
void start_console(char **configfiles);


#define CONSOLE_COMMANDS 18
char *console_cmd[] = {
  "help",
  "h",
  "exit",
  "quit",
  "q",
  "show",
  "s",
  "set",
  "start",
  "stop",
  "restart",
  "interact",
  "i",
  "sleep",
  "n",
  "next",
  "copy",
  "exec",
  "",
  /*"start",
  "restart"*/
  /* more */
};

int (*console_fcn[]) (char**) = {
  &help_operation,
  &help_operation,
  &exit_operation,
  &exit_operation,
  &exit_operation,
  &show_operation,
  &set_operation,
  &set_operation,
  &start_operation,
  &stop_operation,
  &restart_operation,
  &interact_operation,
  &interact_operation,
  &sleep_operation,
  &next_operation,
  &next_operation,
  &copy_operation,
  &exec_operation,
  &nop_operation,
  &nop_operation,
  &nop_operation,
  &nop_operation,
  &nop_operation,
  &nop_operation,
  &nop_operation,
  &nop_operation,
  &nop_operation
};





#endif
