#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../include/config.h"
#include "../include/logger.h"
#include "../include/process.h"

void start_process(ManagedProcess *process) {
  if (access(process->command, X_OK) == -1) {
    char msg[1024];
    snprintf(msg, sizeof(msg),
             "[ERROR] Plik dla procesu '%s' (%s) nie istnieje lub brak praw do "
             "wykonania!",
             process->name, process->command);
    write_log(msg);
    return;
  }

  pid_t pid = fork();

  if (pid == 0) {
    if (process->mem_limit > 0) {
      struct rlimit rl_mem;
      rl_mem.rlim_cur = rl_mem.rlim_max = process->mem_limit * 1024 * 1024;
      setrlimit(RLIMIT_AS, &rl_mem);
    }

    if (process->cpu_limit > 0) {
      struct rlimit rl_cpu;
      rl_cpu.rlim_cur = rl_cpu.rlim_max = process->cpu_limit;
      setrlimit(RLIMIT_CPU, &rl_cpu);
    }

    char *args[] = {process->command, NULL};
    execvp(args[0], args);
    exit(EXIT_FAILURE);

  } else if (pid > 0) {
    process->current_pid = pid;

    char msg[1024];
    snprintf(msg, sizeof(msg),
             "[START] Uruchomiono '%s' (PID: %d) | Limity: CPU %d s, RAM %d MB",
             process->name, pid, process->cpu_limit, process->mem_limit);

    write_log(msg);
  }
}