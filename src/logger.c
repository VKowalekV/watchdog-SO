#include "../include/logger.h"

#include <limits.h>
#include <stdio.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static char current_log_file[PATH_MAX] = LOG_FILE;
static int mirror_to_stderr = 0;

void logger_init(const char *log_path, int mirror_stderr) {
  if (log_path != NULL && log_path[0] != '\0') {
    snprintf(current_log_file, sizeof(current_log_file), "%s", log_path);
  }
  mirror_to_stderr = mirror_stderr;
}

/**
 * @brief Bezpieczny zapis logu z datą i czasem do pliku logów.
 * @param message Treść komunikatu do zapisania
 */
void write_log(const char *message) {
  // Pobranie aktualnego czasu systemowego
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char date[32];

  // Formatowanie daty w formacie (YYYY-MM-DD HH:MM:SS)
  if (t != NULL) {
    strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", t);
  } else {
    snprintf(date, sizeof(date), "0000-00-00 00:00:00");
  }

  if (mirror_to_stderr) {
    fprintf(stderr, "[%s] %s\n", date, message);
    fflush(stderr);
  }

  // Otwarcie pliku w trybie dopisywania
  FILE *file = fopen(current_log_file, "a");
  if (!file) {
    return;  // Jeśli nie mamy uprawnień, po prostu ignorujemy
  }

  // Zapis linii do pliku w formacie [DATA] KOMUNIKAT
  fprintf(file, "[%s] %s\n", date, message);
  fclose(file);
}
