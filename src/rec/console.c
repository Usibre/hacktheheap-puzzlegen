#include "console.h"


void send_msg(char *msg_contents, uint16_t size) {
  message msg;
  msg.mtype = 23; // >?
  if (DBG)
    fprintf(stderr, "Console: \tSending message:\t%s \n", msg_contents);
  memcpy(&(msg.mtext), msg_contents, size);
  if (msgsnd(msqid, &msg, size, 0) == -1) {
    perror("msgsnd");
    exit(EXIT_FAILURE);
  }
}

int read_config_file(char *filename) {
  if (DBG) {
    fprintf(stderr, "Console: \tReading config from file: %s\n", filename);
  }
  FILE *f = fopen(filename, "r");
  if (!f) {
    fprintf(stderr, "Failed to open file (%s)\n", filename);
    return 0; // only return non-null on a critical error
  }
  ssize_t characters;
  char *line = NULL;
  size_t len = 0, ctr = 1;
  int status = 0;
  characters = getline(&line, &len, f);
  while (characters >= 0) {
    if (characters < 2) continue;
    line[strlen(line)-1] = '\0'; // strip the newline
    fprintf(stderr, "Console:\tFile: 0x%lx - 0x%lx :) \n", (uintptr_t)f, (uintptr_t)f->_IO_read_ptr);
    fprintf(stderr, "Console:\tFile line (ret=%ld) reads: '%s'\n", characters, line);
    status = console_handle_line(line);
    if (status) {
      debug_print("Error reading line %lu: %s\n", ctr, line);
    }
    ctr++;
    characters = getline(&line, &len, f);
  }
  free(line);
  fclose(f);
  printf("Done processing config file ('%s')\n", filename);
  return 0;
}

int _is_running() {
  if (program_pid > 0 && kill(program_pid, 0)==0) {
    // check if defunct / zombie
    int status = 0;
    int x = waitpid(program_pid, &status, WNOHANG|WUNTRACED|WCONTINUED);
    return x<1;
  }
  return 0;
}

/** Console Ops **/
int save_operation(char **args) {
  send_msg("save", 8);
  return 0;
}

int nop_operation(char **args) {
  return 0;
}

int sleep_operation(char **args) {
  unsigned int seconds;
  if (args == NULL || args[1] == NULL) {
    seconds = 1;
  } else {
    sscanf(args[1], "%u", &seconds);
    if (seconds == 0 || seconds > 60)
      seconds = 1;
  }
  sleep(seconds);
  return 0;
}

void exit_sigcatcher(int sig) {
  exit_operation(NULL);
}

int exit_operation(char **args) {
  if (next_sleeptime)
    sleep(next_sleeptime);
  if (_is_running())
    stop_operation(NULL);
  send_msg("QUIT", 4);
  exit(0);
}

int start_operation(char **args) {
  if (_is_running()) {
    fprintf(stderr, "Target already running!\n");
    return 0;
  }
  if (!program) {
    fprintf(stderr, "No target executable specified!\n");
    return 0;
  }
  // disabled this check bc it could be linked 
  // we don't want to block other potential workflows for no reason 
  if (0){ // !preload) {
    fprintf(stderr, "Preload not specified!\n");
    return 0;
  }
  int res = pipe(execve_pipe);
  if (res != 0 && DBG) {
    fprintf(stderr, "Warning! Pipe failed :( \n");
  }
  int i;
  if (DBG) {
    fprintf(stderr, "Console: \tArgs: \n");
    i = 1;
    while (args[i] != NULL && i < 64) {
      fprintf(stderr, "\t\t%s\n", args[i++]);
    }
  }
  if (DBG) {
    fprintf(stderr, "Console: \tEnv: \n");
    i = 0;
    while (envs[i] != NULL && i < 64) {
      fprintf(stderr, "\t\t%s\n", envs[i++]);
    }
  }
  program_pid = fork();
  if (program_pid == -1) {
    fprintf(stderr, "Forking failed!\n");
    return 1;
  }
  if (program_pid == 0) {
    // child
    char *argv[CONSOLE_ARGSIZE];
    argv[0] = program;
    size_t i = 1;
    while (args[i] && i < CONSOLE_ARGSIZE-1) {
      argv[i] = args[i];
      i++;
    }
    argv[i] = (char*)0;
    // exec
    dup2(execve_pipe[READ_END], STDIN_FILENO);
    close(execve_pipe[WRITE_END]);
    execve(argv[0], argv, envs);
    fprintf(stderr, "Execve on target executable failed!\n");
    fprintf(stderr, "Error code %d\n", errno);
    perror("execve");   /* execve() returns only on error */
    exit(EXIT_FAILURE);
  } else {
    // parent
    close(execve_pipe[READ_END]);
    if (DBG) {
      fprintf(stderr, "Console: \tStarting executable %s on pid %d...\n", program, program_pid);
      fprintf(stderr, "\t\tNote this pid is of the child process, but might not be the pid of interest.\n");
    }
    if (next_sleeptime)
      sleep(next_sleeptime);
    return 0;
  }
  return 0;
}

int interact_operation(char **args) {
  if (!_is_running()) {
    fprintf(stderr, "Program not running!\n");
    return 0;
  }
  size_t length = strlen(args[0])+1;
  char buf[256];
  snprintf(buf, 255, "%s\n", last_cmd+length);
  buf[255] = '\0';
  ssize_t written = write(execve_pipe[WRITE_END], buf, strlen(buf));
  if (DBG) fprintf(stderr, "Console: \tWritten %lu to stdin: '%s'\n", written, last_cmd+length);
  return 0;
}

int stop_operation(char **args) {
  close(execve_pipe[WRITE_END]);
  kill(program_pid, SIGTERM);
  usleep(100);
  if (!_is_running()) {
    return 0;
  }
  kill(program_pid, SIGKILL);
  sleep(2);
  int status = 0;
  int x = waitpid(program_pid, &status, WNOHANG|WUNTRACED|WCONTINUED);
  if (x<1) {
    fprintf(stderr, "Failed to stop target executable.\n");
  } else {
    if (kill(program_pid, 0) == 0) {
      fprintf(stderr, "Process id %d still exists.\n", program_pid);
    }
  }
  return x<1;
}

int restart_operation(char **args) {
  fprintf(stderr, "Restarting...\n");
  stop_operation(args);
  sleep(1);
  return start_operation(args);
}

int add_env_var(char **args) {
  if (args[2] == NULL) {
    fprintf(stderr, "Specify environment variable please!\n");
    return 0; // only return non-null on critical errors
  }
  int no_overwrite = (args[4] != NULL && strlen(args[4])>0);
  size_t i;
  for (i = 0; envs[i] != NULL && i < 63; i++) {
    if (strncmp(args[2], envs[i], strlen(args[2]))==0) {
      if (no_overwrite) return 0;
      if (strlen(args[3])==0) {
        // remove instead of reset
        free(envs[i++]);
        while (envs[i] != NULL && i < 63) {
          envs[i-1] = envs[i];
          i++;
        }
        envs[i-1] = NULL;
        if (DBG) fprintf(stderr, "Console: \tenvironment variable '%s' removed.\n", args[2]);
        return 0;
      }
      if (DBG) fprintf(stderr, "Console: \tenvironment variable '%s' reset to '%s'.\n", args[2], args[3]);
      snprintf(envs[i], 255, "%s=%s", args[2], args[3]);
      return 0;
    }
  } // not found, create a new one
  if (i >= 63) {
    fprintf(stderr, "Critical! environment buffer full!\n");
    return 1;
  }
  envs[i] = (char*)malloc(256);
  envs[i+1] = NULL;
  snprintf(envs[i], 255, "%s=%s", args[2], args[3]);
  if (DBG) fprintf(stderr, "Console: \tenvironment variable %lu '%s' set to '%s'.\n", i, args[2], args[3]);
  return 0;
}

int help_operation(char **args) {
  printf("Operations: \n");
  for (size_t i = 0; i < CONSOLE_COMMANDS; i++) {
    printf("\t%s\n", console_cmd[i]);
  }
  return 0;
}

int show_operation(char **args) {
  if (!args[1]) {
    printf("Not sure what to show?\n");
    return 0;
  }
  if (strcmp(args[1], "child") == 0) {
    printf("Child pid: %u\n", childpid);
    return 0;
  }
  if (strcmp(args[1], "msqid") == 0) {
    printf("msqid = %d\n", msqid);
    return 0;
  }
  if (strcmp(args[1], "running") == 0) {
    // If sig is 0 (the null signal), error checking is performed but no signal is actually sent.
    if (!_is_running()) {
      printf("Program is not running.\n");
      return 0;
    }
    printf("Program is running under pid %d\n", program_pid);
    return 0;
  }
  if (strcmp(args[1], "help") == 0) {
    return help_operation(args);
  }
  printf("Not sure that to show (requested: %s)\n", args[1]);
  return 0;
}

int set_operation(char **args) {
  if (!args[1]) {
    printf("Please specify what to set.\n");
    return 0;
  }
  if (!args[2]) {
    printf("Please specify a value\n");
    return 0;
  }
  if (strcmp(args[1], "debug") == 0 || strcmp(args[1], "dbg") == 0) {
    sscanf(args[2], "%hhu", &DBG);
    if (DBG) fprintf(stderr, "Console: \tDebug mode turned on in console.\n");
    send_msg(last_cmd, strlen(last_cmd));
    return 0;
  }
  if (strcmp(args[1], "config")==0 ||
      strcmp(args[1], "configfile")==0 ||
      strcmp(args[1], "c")==0) {
    return read_config_file(args[2]);
  }
  if (strcmp(args[1], "exec")==0
      || strcmp(args[1], "executable")==0
      || strcmp(args[1], "target")==0) {
    if (program == NULL) program = (char*) malloc(256);
    strllcpy(program, args[2], 255);
    program[255] = '\0';
    char to_send[261];
    sprintf(to_send, "exec %s", program);
    send_msg(last_cmd, 261);
    char *env_var[] = {NULL, NULL, "_", program, NULL};
    add_env_var(env_var);
    if (DBG) fprintf(stderr, "Console: \tTarget executable set to %s.\n", program);
    return 0;
  }
  if (strcmp(args[1], "preload")==0 || strcmp(args[1], "ld_preload")==0) {
    // newer :D
    args[3] = args[2];
    args[2] = "LD_PRELOAD";
    return add_env_var(args);
  }
  if (strcmp(args[1], "env")==0) {
    return add_env_var(args);
  }
  if (strcmp(args[1], "sleeper")==0) {
    sscanf(args[2], "%u", &next_sleeptime);
    if (next_sleeptime > 60) {
      fprintf(stderr, "Invalid value! Reset to 0.\n");
      next_sleeptime = 0;
    } else if (DBG) {
      fprintf(stderr, "Console: \tSet the sleeper to %u seconds.\n", next_sleeptime);
    }
    return 0;
  }
  // just send it to the manager, might be a manager export setting
  send_msg(last_cmd, strlen(last_cmd));
  return 0;
}

int copy_env() {
  // to avoid duplicates, let's break down the env vars instead of adding them
  // directly.
  char env_var[128];
  char val[128];
  char **s = environ;
  int ret = 0;
  char *token;
  while (*s != NULL) {
    token = strchr(*s, '=');
    size_t env_var_size = min(((size_t)token-(size_t)(*s)+1), 127);
    strllcpy(env_var, *s, env_var_size);
    env_var[env_var_size] = '\0';
    strllcpy(val, token+1, 127);
    env_var[127] = '\0';
    val[127] = '\0';
    char *args[] = {NULL, NULL, env_var, val, "1"};
    ret += add_env_var(args);
    s++;
    if (ret) return ret;
  }
  return ret;
}

int copy_operation(char **args) {
  if (strcmp(args[1], "env")==0 || strcmp(args[1], "environment")==0) {
    return copy_env();
  }
  fprintf(stderr, "Copy only knows 'env'. \n");
  return 0;
}

int exec_operation(char **args) {
  // execute in child so as not to block
  if (DBG) fprintf(stderr, "Console: \tExecuting command: '%s'\n", last_cmd+strlen(args[0])+1);
  if (fork() == 0) {
    if (system(last_cmd+strlen(args[0])+1)) {
      if (DBG) fprintf(stderr, "Console: \tExecution operation failed.\n");
    } else {
      if (DBG) fprintf(stderr, "Console: \tExecution operation successful.\n");
    }
    _Exit(0);
  }
  return 0;
}


/** Console helper functions **/
char *console_read_line(char *buffer) {
  size_t pos = 0;
  int c; // getchar value
  // we need the -1 for the nullbyte char anyway
  while (pos < CONSOLE_BUFSIZE-1) {
    c = getchar();
    if (c == EOF || c == '\n') {
      buffer[pos] = '\0';
      return buffer;
    }
    buffer[pos] = c; // should be fine regardless of typing?
    pos++;
  }
  // if the code gets here, the command was too long
  fprintf(stderr, "The command was too long. Executing nop.\n");
  memcpy(buffer, "NOP", 4);
  return buffer;
}

int next_operation(char **args) {
  if (next_sleeptime)
    sleep(next_sleeptime);
  return send_direct(args);
}

int send_direct(char **args) {
  send_msg(last_cmd, strlen(last_cmd));
  return 0;
}

int console_exec(char **args) {
  if (!args || !args[0]) return EXIT_FAILURE;
  if (args[0][0] == COMMENT_DEL) return 0;
  for (size_t i = 0; i < CONSOLE_COMMANDS; i++) {
    if (strcmp(args[0], console_cmd[i]) == 0) {
      if (DBG && strlen(args[0])>0) fprintf(stderr, "Console: \tHandling inside the client: %s\n", args[0]);
      return (*console_fcn[i])(args);
    }
  }
  return send_direct(args);
}

int console_handle_line(char *line) {
  memcpy(last_cmd, line, CONSOLE_BUFSIZE);
  str_split(args, line);
  return console_exec(args);
}

void start_console(char **configfiles) {
  // add handlers
  signal(SIGTERM, exit_sigcatcher);
  signal(SIGINT, exit_sigcatcher);
  signal(SIGHUP, exit_sigcatcher);
  // message queue environment var
  envs[0] = (char*)malloc(128);
  sprintf(envs[0], "HTH_MSQ=%u", msqid);
  envs[1] = (char*)0;
  int status = 0;
  args = (char**) malloc(CONSOLE_ARGSIZE*sizeof(char**));
  // this could simply be stack -,-'
  char *line = (char*)malloc(CONSOLE_BUFSIZE);

  if (!line || !args) {
    fprintf(stderr, "Console malloc failure");
    exit(EXIT_FAILURE);
  }
  while (configfiles != NULL && configfiles[0] != NULL) {
    read_config_file(configfiles[0]);
    configfiles++;
  }
  do {
    usleep(100);
    printf("> ");
    console_read_line(line);
    status = console_handle_line(line);
  } while (!status);
  fprintf(stderr, "Console exiting.\n");
  free(line);
  free(args);
}
