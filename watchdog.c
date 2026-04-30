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

// Flagi sygnalizujące głównej pętli, co ma robić
// Muszą być typu `volatile sig_atomic_t`, aby modyfikacje z procedury obsługi
// sygnału były bezpieczne
volatile sig_atomic_t keep_running = 1;      // flga która mówi czy demon ma nadal działać
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
 * @brief Tworzy plik PID.
 * Służy aby uniknąć dwóch instancji demona.
 */
void create_pid_file() {
  FILE *pid_file = fopen(PID_FILE, "w");

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

/**
 * @brief Główna pętla i wejście programu.
 */
int main() {
  // Zabezpieczenie przed podwójnym uruchomieniem
  if (check_pid_file() == -1) {
    return EXIT_FAILURE;
  }

  // odczytanie konfiguracji
  // Pozwala to wypisać błędy krytyczne na ekran terminala
  ManagedProcess temp_list[MAX_PROCESSES];
  int check = read_config(CONFIG_FILE, temp_list);
  if (check <= 0) {
    return EXIT_SUCCESS;  // Brak procesów do nadzoru lub utworzono nowy plik
  }

  // Proces staje się demonem systemowym
  daemonize();

  // Zapisujemy ostateczny PID z racji tego, że daemonize() zmieniło nam PID
  create_pid_file();
  setup_signals();

  write_log("[SYSTEM] Watchdog wystartował pomyślnie i rozpoczął nadzór.");

  // Inicjalizacja procesów z konfiguracji
  process_init();

  // Główna nieskończona pętla Demona
  while (keep_running) {
    // przeładowanie (na sygnał SIGHUP)
    if (reload_requested) {
      reload_requested = 0;
      process_reload();
    }

    // Blokujące oczekiwanie na śmierć któregokolwiek z dzieci (-1)
    int status;
    pid_t dead_pid = waitpid(-1, &status, 0);

    // Przerwanie waitpid() przez sygnał asynchroniczny (np. SIGHUP)
    if (dead_pid < 0 && errno == EINTR) {
      continue;
    }

    // Jeśli potomek faktycznie zginął i demon wciąż ma działać
    if (dead_pid > 0 && keep_running) {
      process_handle_death(dead_pid, status);  // Ubija proces i odpala go na nowo
    }
  }

  // Zamykanie Demona
  process_shutdown();  // Wysyła SIGTERM do wszystkich nadzorowanych procesów
  write_log("[SYSTEM] Watchdog zakończył działanie.");

  unlink(PID_FILE);  // usunięcie pliku z PID watchdoga

  // Zabezpieczenie na wypadek procesów, które potrzebują chwili na zamknięcie
  while (waitpid(-1, NULL, WNOHANG) > 0);

  return EXIT_SUCCESS;
}