#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/config.h"
#include "../include/logger.h"
#include "../include/process.h"

static ManagedProcess watch_list[MAX_PROCESSES];
static int process_count = 0;

static void start_process(ManagedProcess *process);
static void get_death_reason(int status, char *reason, size_t size);

void process_init() {
  process_count = read_config(CONFIG_FILE, watch_list);

  if (process_count <= 0) {
    write_log("[SYSTEM] Błąd wczytywania lub plik config jest pusty!");
    return;
  }

  for (int i = 0; i < process_count; i++) {
    watch_list[i].current_pid = 0;
    start_process(&watch_list[i]);
  }
}

void process_reload() {
  write_log("[SYSTEM] Otrzymano SIGHUP. Przeładowywanie konfiguracji...");

  int isChanged = 0;

  ManagedProcess new_watch_list[MAX_PROCESSES];
  memset(new_watch_list, 0, sizeof(new_watch_list));
  int new_process_count = read_config(CONFIG_FILE, new_watch_list);

  if (new_process_count <= 0) {
    write_log(
        "[SYSTEM] Plik konfiguracyjny jest pusty! Przerywam przeładowanie.");
    return;
  }

  for (int i = 0; i < process_count; i++) {
    int found = 0;

    for (int j = 0; j < new_process_count; j++) {
      if (strcmp(watch_list[i].name, new_watch_list[j].name) == 0) {
        if (strcmp(watch_list[i].command, new_watch_list[j].command) == 0 &&
            watch_list[i].cpu_limit == new_watch_list[j].cpu_limit &&
            watch_list[i].mem_limit == new_watch_list[j].mem_limit) {

          new_watch_list[j].current_pid = watch_list[i].current_pid;
        } else {
          isChanged = 1;
          char msg[1024];

          snprintf(msg, sizeof(msg),
                   "[RELOAD] Zmodyfikowano limity dla '%s'. Zatrzymywanie "
                   "starego procesu...",
                   watch_list[i].name);

          write_log(msg);

          if (watch_list[i].current_pid > 0) {
            kill(-watch_list[i].current_pid, SIGTERM);
          }
        }
        found = 1;
        break;
      }
    }
    if (!found && watch_list[i].current_pid > 0) {
      isChanged = 1;
      char msg[1024];

      snprintf(msg, sizeof(msg),
               "[RELOAD] Usunięto proces '%s' z konfiguracji. Zatrzymywanie...",
               watch_list[i].name);

      write_log(msg);
      kill(-watch_list[i].current_pid, SIGTERM);
    }
  }

  memcpy(watch_list, new_watch_list, sizeof(ManagedProcess) * MAX_PROCESSES);
  process_count = new_process_count;

  for (int i = 0; i < process_count; i++) {
    if (watch_list[i].current_pid == 0) {
      isChanged = 1;
      start_process(&watch_list[i]);
    }
  }

  if (isChanged == 1) {
    write_log("[SYSTEM] Przeładowywanie konfiguracji zakończone pomyślnie.");
  } else {
    write_log("[SYSTEM] Konfiguracja nie uległa zmianie.");
  }
}

void process_handle_death(pid_t dead_pid, int status) {
  for (int i = 0; i < process_count; i++) {
    if (watch_list[i].current_pid == dead_pid) {
      watch_list[i].current_pid = 0;

      char reason[128];
      get_death_reason(status, reason, sizeof(reason));

      char msg[1024];
      snprintf(msg, sizeof(msg),
               "[RESTART] Proces '%s' (PID: %d) zakończył działanie. "
               "Przyczyna: %s. Uruchamianie ponownie...",
               watch_list[i].name, dead_pid, reason);
      write_log(msg);

      sleep(1);
      start_process(&watch_list[i]);
      break;
    }
  }
}

void process_shutdown() {
  write_log("[SYSTEM] Zamykanie procesów potomnych...");
  for (int i = 0; i < process_count; i++) {
    if (watch_list[i].current_pid > 0) {
      kill(-watch_list[i].current_pid, SIGTERM);
    }
  }
}

static void start_process(ManagedProcess *process) {
  if (access(process->command, X_OK) == -1) {
    char msg[2048];
    snprintf(msg, sizeof(msg),
             "[ERROR] Plik dla procesu '%s' (%s) nie istnieje lub brak praw do "
             "wykonania!",
             process->name, process->command);
    write_log(msg);
    return;
  }

  pid_t pid = fork();

  if (pid == 0) {
    setpgid(0, 0);

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
  } else {
    char msg[1024];
    snprintf(msg, sizeof(msg), "[ERROR] Blad fork() dla: %s", process->name);
    write_log(msg);
  }
}

static void get_death_reason(int status, char *reason, size_t size) {
  if (WIFEXITED(status)) {
    snprintf(reason, size, "kod wyjścia %d", WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
    int signal_num = WTERMSIG(status);
    if (signal_num == SIGKILL) {
      snprintf(reason, size, "SIGKILL (twardy limit CPU lub brak RAMu)");
    } else if (signal_num == SIGSEGV) {
      snprintf(reason, size, "SIGSEGV (błąd ochrony pamięci)");
    } else if (signal_num == SIGXCPU) {
      snprintf(reason, size, "SIGXCPU (miękki limit procesora)");
    } else if (signal_num == SIGTERM) {
      snprintf(reason, size, "SIGTERM (poproszono o wyłączenie)");
    } else {
      snprintf(reason, size, "sygnał nr %d", signal_num);
    }
  } else {
    snprintf(reason, size,
             "nieznana (zniknął w niewyjaśnionych okolicznościach)");
  }
}