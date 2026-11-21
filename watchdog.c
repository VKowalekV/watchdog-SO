#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "include/config.h"
#include "include/daemon.h"
#include "include/logger.h"
#include "include/process.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define WATCHDOG_VERSION "1.1.0"
#define PID_FILE "/var/run/watchdog.pid"

// Flags signaling main loop control flow
// Must use `volatile sig_atomic_t` for async-signal-safe updates
volatile sig_atomic_t keep_running = 1;      // Indicates whether the daemon should keep running
volatile sig_atomic_t reload_requested = 0;  // Indicates whether a configuration reload was requested

/**
 * @brief Async signal handler.
 * @param sig Received signal number
 */
void handle_signal(int sig) {
  if (sig == SIGHUP) {
    reload_requested = 1;  // SIGHUP = reload configuration file
  } else if (sig == SIGINT || sig == SIGTERM) {
    keep_running = 0;  // SIGTERM / SIGINT = shut down daemon
  }
}

/**
 * @brief Registers OS signal handlers.
 */
void setup_signals() {
  struct sigaction sa;
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGTERM, &sa, NULL);  // Register graceful shutdown
  sigaction(SIGINT, &sa, NULL);   // Register interrupt handling
  sigaction(SIGHUP, &sa, NULL);   // Register hot-reload
}

/**
 * @brief Resolves a relative path to an absolute path before chdir("/").
 * @return 0 on success, -1 on failure or path truncation
 */
static int resolve_path(const char *input, char *out, size_t out_size) {
  if (input == NULL || input[0] == '\0' || out == NULL || out_size == 0) {
    return -1;
  }

  if (input[0] == '/') {
    size_t len = strlen(input);
    if (len >= out_size) {
      return -1;
    }
    memcpy(out, input, len + 1);
    return 0;
  }

  char cwd[PATH_MAX];
  if (getcwd(cwd, sizeof(cwd)) == NULL) {
    return -1;
  }

  size_t cwd_len = strlen(cwd);
  size_t in_len = strlen(input);
  if (cwd_len + 1 + in_len + 1 > out_size) {
    return -1;
  }

  memcpy(out, cwd, cwd_len);
  out[cwd_len] = '/';
  memcpy(out + cwd_len + 1, input, in_len + 1);
  return 0;
}

/**
 * @brief Prints CLI usage instructions.
 */
static void print_usage(FILE *stream, const char *prog_name) {
  fprintf(stream,
          "Usage: %s [OPTIONS]\n"
          "\n"
          "Options:\n"
          "  -c, --config <path>  Path to configuration file (default: %s)\n"
          "  -l, --log <path>     Path to log file (default: %s)\n"
          "  -p, --pid <path>     Path to PID file (default: %s)\n"
          "  -f, --foreground     Run in foreground without daemonizing (mirror logs to stderr)\n"
          "  -h, --help           Display this help message and exit\n"
          "  -v, --version        Display version information and exit\n",
          prog_name, CONFIG_FILE, LOG_FILE, PID_FILE);
}

/**
 * @brief Creates the PID file to prevent duplicate daemon instances.
 */
void create_pid_file(const char *pid_file_path) {
  FILE *pid_file = fopen(pid_file_path, "w");

  if (pid_file) {
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);
  }
}

/**
 * @brief Checks whether another daemon instance is already running.
 * Reads the PID file and queries the kernel via kill(pid, 0).
 * @return 0 if free, -1 if already running
 */
int check_pid_file(const char *pid_file_path) {
  FILE *pid_file = fopen(pid_file_path, "r");

  if (pid_file) {
    pid_t old_pid;

    if (fscanf(pid_file, "%d", &old_pid) == 1) {
      if (old_pid > 0 && kill(old_pid, 0) == 0) {
        fprintf(stderr, "Watchdog is already running (PID: %d)!\n", old_pid);
        fclose(pid_file);
        return -1;
      }
    }
    fclose(pid_file);
  }

  return 0;
}

/**
 * @brief Main daemon entry point and supervision loop.
 */
int main(int argc, char *argv[]) {
  char config_path[PATH_MAX];
  char log_path[PATH_MAX];
  char pid_path[PATH_MAX];
  int foreground = 0;

  snprintf(config_path, sizeof(config_path), "%s", CONFIG_FILE);
  snprintf(log_path, sizeof(log_path), "%s", LOG_FILE);
  snprintf(pid_path, sizeof(pid_path), "%s", PID_FILE);

  static const struct option long_options[] = {
      {"config", required_argument, NULL, 'c'},
      {"log", required_argument, NULL, 'l'},
      {"pid", required_argument, NULL, 'p'},
      {"foreground", no_argument, NULL, 'f'},
      {"help", no_argument, NULL, 'h'},
      {"version", no_argument, NULL, 'v'},
      {NULL, 0, NULL, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "c:l:p:fhv", long_options, NULL)) != -1) {
    switch (opt) {
      case 'c':
        if (resolve_path(optarg, config_path, sizeof(config_path)) != 0) {
          fprintf(stderr, "Invalid or too long config path: %s\n", optarg);
          return EXIT_FAILURE;
        }
        break;
      case 'l':
        if (resolve_path(optarg, log_path, sizeof(log_path)) != 0) {
          fprintf(stderr, "Invalid or too long log path: %s\n", optarg);
          return EXIT_FAILURE;
        }
        break;
      case 'p':
        if (resolve_path(optarg, pid_path, sizeof(pid_path)) != 0) {
          fprintf(stderr, "Invalid or too long PID path: %s\n", optarg);
          return EXIT_FAILURE;
        }
        break;
      case 'f':
        foreground = 1;
        break;
      case 'h':
        print_usage(stdout, argv[0]);
        return EXIT_SUCCESS;
      case 'v':
        printf("watchdog %s\n", WATCHDOG_VERSION);
        return EXIT_SUCCESS;
      default:
        print_usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }
  }

  if (optind < argc) {
    fprintf(stderr, "Unexpected positional argument: %s\n", argv[optind]);
    print_usage(stderr, argv[0]);
    return EXIT_FAILURE;
  }

  // Prevent starting a second daemon instance
  if (check_pid_file(pid_path) == -1) {
    return EXIT_FAILURE;
  }

  logger_init(log_path, foreground);

  // Load configuration before daemonizing so fatal errors print to the terminal
  ManagedProcess temp_list[MAX_PROCESSES];
  int check = read_config(config_path, temp_list);
  if (check < 0) {
    return EXIT_FAILURE;
  }
  if (check == 0) {
    return EXIT_SUCCESS;  // Generated a new default configuration file
  }

  // Daemonize unless --foreground was passed
  if (!foreground) {
    daemonize();
  }

  // Record final PID after daemonize() forks
  create_pid_file(pid_path);
  setup_signals();

  write_log("[SYSTEM] Watchdog started successfully and began supervision.");

  // Spawn initial processes from configuration
  process_init(config_path);

  // Main daemon supervision loop
  while (keep_running) {
    // Handle configuration reload on SIGHUP
    if (reload_requested) {
      reload_requested = 0;
      process_reload(config_path);
    }

    // Block waiting for any child process to terminate (-1)
    int status;
    pid_t dead_pid = waitpid(-1, &status, 0);

    // Handle waitpid() interruption by async signals (e.g. SIGHUP) or no active children
    if (dead_pid < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == ECHILD) {
        sleep(1);
        continue;
      }
      break;
    }

    // Restart terminated child if daemon is still running
    if (dead_pid > 0 && keep_running) {
      process_handle_death(dead_pid, status);
    }
  }

  // Shut down daemon
  process_shutdown();  // Send SIGTERM to all supervised processes
  write_log("[SYSTEM] Watchdog shut down cleanly.");

  unlink(pid_path);  // Remove watchdog PID file

  // Reap any child processes still exiting
  while (waitpid(-1, NULL, WNOHANG) > 0);

  return EXIT_SUCCESS;
}
