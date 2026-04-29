#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "include/config.h"
#include "include/daemon.h"
#include "include/logger.h"
#include "include/process.h"

#define PID_FILE "/var/run/watchdog.pid"

volatile sig_atomic_t keep_running = 1;
volatile sig_atomic_t reload_requested = 0;

void handle_signal(int sig) {
  if (sig == SIGHUP) {
    reload_requested = 1;
  } else if (sig == SIGINT || sig == SIGTERM) {
    keep_running = 0;
  }
}

void setup_signals() {
  struct sigaction sa;
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGHUP, &sa, NULL);
}

void create_pid_file() {
  FILE *pid_file = fopen(PID_FILE, "w");

  if (pid_file) {
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);
  }
}

int check_pid_file() {
  FILE *pid_file = fopen(PID_FILE, "r");

  if (pid_file) {
    pid_t old_pid;

    if (fscanf(pid_file, "%d", &old_pid) == 1) {
      if (kill(old_pid, 0) == 0) {
        printf("Watchdog już działa (PID: %d)!\n", old_pid);
        fclose(pid_file);
        return -1;
      }
    }
    fclose(pid_file);
  }

  return 0;
}

int main() {
  if (check_pid_file() == -1) {
    return EXIT_FAILURE;
  }

  ManagedProcess temp_list[MAX_PROCESSES];
  int check = read_config(CONFIG_FILE, temp_list);
  if (check <= 0) {
    return EXIT_SUCCESS;
  }

  daemonize();
  create_pid_file();
  setup_signals();

  write_log("[SYSTEM] Watchdog wystartował pomyślnie i rozpoczął nadzór.");

  process_init();

  while (keep_running) {
    if (reload_requested) {
      reload_requested = 0;
      process_reload();
    }

    int status;
    pid_t dead_pid = waitpid(-1, &status, 0);

    if (dead_pid == 0) {
      sleep(1);
      continue;
    }

    if (dead_pid < 0 && errno == EINTR) {
      continue;
    }

    if (dead_pid > 0 && keep_running) {
      process_handle_death(dead_pid, status);
    }
  }

  process_shutdown();
  write_log("[SYSTEM] Watchdog zakończył działanie.");

  unlink(PID_FILE);

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;

  return EXIT_SUCCESS;
}