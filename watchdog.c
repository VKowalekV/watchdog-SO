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

// Flagi sygnalizujące głównej pętli, co ma robić
// Muszą być typu `volatile sig_atomic_t`, aby modyfikacje z procedury obsługi
// sygnału były bezpieczne
volatile sig_atomic_t keep_running = 1;      // flaga która mówi czy demon ma nadal działać
volatile sig_atomic_t reload_requested = 0;  // flaga która mówi czy zażądano przeładowania konfiguracji

/**
 * @brief handler obsługi sygnałów asynchronicznych.
 * @param sig Numer odebranego sygnału
 */
void handle_signal(int sig) {
  if (sig == SIGHUP) {
    reload_requested = 1;  // SIGHUP = przeładuj plik konfiguracyjny
  } else if (sig == SIGINT || sig == SIGTERM) {
    keep_running = 0;  // SIGTERM / SIGINT = zamknij demona
  }
}

/**
 * @brief Rejestracja sygnałów dla systemu operacyjnego.
 */
void setup_signals() {
  struct sigaction sa;
  sa.sa_handler = handle_signal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  sigaction(SIGTERM, &sa, NULL);  // Rejestracja zamykania
  sigaction(SIGINT, &sa, NULL);   // Rejestracja przerwania
  sigaction(SIGHUP, &sa, NULL);   // Rejestracja hot-reload
}

/**
 * @brief Zamienia ścieżkę względną na bezwzględną przed ewentualnym chdir("/").
 * @return 0 w razie sukcesu, -1 przy błędzie lub zbyt długiej ścieżce
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
 * @brief Wypisuje instrukcję użycia flag CLI.
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
 * @brief Tworzy plik PID.
 * Służy aby uniknąć dwóch instancji demona.
 */
void create_pid_file(const char *pid_file_path) {
  FILE *pid_file = fopen(pid_file_path, "w");

  if (pid_file) {
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);
  }
}

/**
 * @brief Sprawdza, czy inna instancja demona już działa.
 * Wczytuje PID z pliku i za pomocą kill(pid, 0) pyta jądro systemu, czy ten
 * proces wciąż istnieje.
 * @return 0 jeśli wolne, -1 jeśli zajęte
 */
int check_pid_file(const char *pid_file_path) {
  FILE *pid_file = fopen(pid_file_path, "r");

  if (pid_file) {
    pid_t old_pid;

    if (fscanf(pid_file, "%d", &old_pid) == 1) {
      if (old_pid > 0 && kill(old_pid, 0) == 0) {
        printf("Watchdog już działa (PID: %d)!\n", old_pid);
        fclose(pid_file);
        return -1;
      }
    }
    fclose(pid_file);
  }

  return 0;
}

/**
 * @brief Główna pętla i wejście programu.
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

  // Zabezpieczenie przed podwójnym uruchomieniem
  if (check_pid_file(pid_path) == -1) {
    return EXIT_FAILURE;
  }

  logger_init(log_path, foreground);

  // odczytanie konfiguracji
  // Pozwala to wypisać błędy krytyczne na ekran terminala
  ManagedProcess temp_list[MAX_PROCESSES];
  int check = read_config(config_path, temp_list);
  if (check < 0) {
    return EXIT_FAILURE;
  }
  if (check == 0) {
    return EXIT_SUCCESS;  // Utworzono nowy domyślny plik konfiguracyjny
  }

  // Proces staje się demonem systemowym (chyba że wybrano --foreground)
  if (!foreground) {
    daemonize();
  }

  // Zapisujemy ostateczny PID z racji tego, że daemonize() zmieniło nam PID
  create_pid_file(pid_path);
  setup_signals();

  write_log("[SYSTEM] Watchdog wystartował pomyślnie i rozpoczął nadzór.");

  // Inicjalizacja procesów z konfiguracji
  process_init(config_path);

  // Główna nieskończona pętla Demona
  while (keep_running) {
    // przeładowanie (na sygnał SIGHUP)
    if (reload_requested) {
      reload_requested = 0;
      process_reload(config_path);
    }

    // Blokujące oczekiwanie na śmierć któregokolwiek z dzieci (-1)
    int status;
    pid_t dead_pid = waitpid(-1, &status, 0);

    // Przerwanie waitpid() przez sygnał asynchroniczny (np. SIGHUP) lub brak dzieci
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

    // Jeśli potomek faktycznie zginął i demon wciąż ma działać
    if (dead_pid > 0 && keep_running) {
      process_handle_death(dead_pid, status);  // Ubija proces i odpala go na nowo
    }
  }

  // Zamykanie Demona
  process_shutdown();  // Wysyła SIGTERM do wszystkich nadzorowanych procesów
  write_log("[SYSTEM] Watchdog zakończył działanie.");

  unlink(pid_path);  // usunięcie pliku z PID watchdoga

  // Zabezpieczenie na wypadek procesów, które potrzebują chwili na zamknięcie
  while (waitpid(-1, NULL, WNOHANG) > 0);

  return EXIT_SUCCESS;
}
