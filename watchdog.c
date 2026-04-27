#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "include/config.h"
#include "include/logger.h"
// #include "include/daemon.h"
#include "include/process.h"

ManagedProcess watch_list[MAX_PROCESSES];
int process_count = 0;

volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t reload_requested = 0;

void handle_sig(int sig) {
  if (sig == SIGHUP) {
    reload_requested = 1;
  } else if (sig == SIGINT || sig == SIGTERM) {
    keep_running = 0;
  }
}

void setup_signals() {
  struct sigaction sa;
  sa.sa_handler = handle_sig;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
}

void do_reload(const char *filename) {
  write_log("[SYSTEM] Otrzymano SIGHUP. Przeładowywanie konfiguracji...");

  ManagedProcess new_watch_list[MAX_PROCESSES];
  memset(new_watch_list, 0, sizeof(new_watch_list));
  int new_process_count = read_config(filename, new_watch_list);

  if (new_process_count == -1) {
    write_log("Blad odczytu pliku! Przerywam przeładowanie. Stara konfiguracja "
              "się nie zmieniła.");
    return;
  }

  if (new_process_count == 0) {
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
          char msg[1024];
          snprintf(msg, sizeof(msg),
                   "[RELOAD] Zmodyfikowano limity dla '%s'. Zatrzymywanie "
                   "starej instancji...",
                   watch_list[i].name);
          write_log(msg);
          if (watch_list[i].current_pid > 0)
            kill(watch_list[i].current_pid, SIGTERM);
        }
        found = 1;
        break;
      }
    }
    if (!found && watch_list[i].current_pid > 0) {
      char msg[1024];
      snprintf(msg, sizeof(msg),
               "[RELOAD] Usunięto proces '%s' z konfiguracji. Zatrzymywanie...",
               watch_list[i].name);
      write_log(msg);
      kill(watch_list[i].current_pid, SIGTERM);
    }
  }

  memcpy(watch_list, new_watch_list, sizeof(ManagedProcess) * MAX_PROCESSES);
  process_count = new_process_count;

  for (int i = 0; i < process_count; i++) {
    if (watch_list[i].current_pid == 0) {
      start_process(&watch_list[i]);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("Uzycie: %s <plik_konfiguracyjny>\n", argv[0]);
    return EXIT_FAILURE;
  }

  char *config_file = argv[1];
  process_count = read_config(config_file, watch_list);

  if (process_count == -1) {
    return EXIT_FAILURE;
  }

  if (process_count == 0) {
    return EXIT_SUCCESS;
  }

  // daemonize();
  setup_signals();

  write_log("[SYSTEM] Watchdog wystartował pomyślnie i rozpoczął nadzór.");

  for (int i = 0; i < process_count; i++) {
    start_process(&watch_list[i]);
  }

  while (keep_running) {
    if (reload_requested) {
      reload_requested = 0;
      do_reload(config_file);
    }

    int status;
    pid_t dead_pid = waitpid(-1, &status, 0);

    if (dead_pid < 0 && errno == EINTR) {
      continue;
    }

    if (dead_pid > 0 && keep_running) {
      for (int i = 0; i < process_count; i++) {
        if (watch_list[i].current_pid == dead_pid) {
          watch_list[i].current_pid = 0;

          char reason[128];
          if (WIFEXITED(status)) {
            snprintf(reason, sizeof(reason),
                     "zakończył się samoczynnie z kodem: %d",
                     WEXITSTATUS(status));
          } else if (WIFSIGNALED(status)) {
            int signal_num = WTERMSIG(status);

            if (signal_num == SIGKILL) {
              snprintf(reason, sizeof(reason),
                       "SIGKILL (twardy limit CPU lub brak RAMu)");
            } else if (signal_num == SIGSEGV) {
              snprintf(reason, sizeof(reason),
                       "SIGSEGV (błąd ochrony pamięci)");
            } else if (signal_num == SIGXCPU) {
              snprintf(reason, sizeof(reason),
                       "SIGXCPU (miękki limit procesora)");
            } else if (signal_num == SIGTERM) {
              snprintf(reason, sizeof(reason),
                       "SIGTERM (poproszono o wyłączenie)");
            } else {
              snprintf(reason, sizeof(reason), "sygnał nr %d", signal_num);
            }
          } else {
            snprintf(reason, sizeof(reason),
                     "nieznana (zniknął w niewyjaśnionych okolicznościach)");
          }

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
  }

  write_log("[SYSTEM] Zamykanie procesów potomnych...");

  for (int i = 0; i < process_count; i++) {
    if (watch_list[i].current_pid > 0) {
      kill(watch_list[i].current_pid, SIGTERM);
    }
  }

  write_log("[SYSTEM] Watchdog zakończył działanie.");

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  return EXIT_SUCCESS;
}