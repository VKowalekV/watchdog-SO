#include "../include/logger.h"

#include <stdio.h>
#include <time.h>

/**
 * @brief Bezpieczny zapis logu z datą i czasem do pliku logów.
 * @param message Treść komunikatu do zapisania
 */
void write_log(const char *message) {
  // Otwarcie pliku w trybie dopsiywania
  FILE *file = fopen(LOG_FILE, "a");
  if (!file) {
    return;  // Jeśli nie mamy uprawnień, po prostu ignorujemy
  }

  // Pobranie aktualnego czasu systemowego
  time_t now = time(NULL);
  struct tm *t = localtime(&now);
  char date[32];

  // Formatowanie daty w foramcie (YYYY-MM-DD HH:MM:SS)
  strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", t);

  // Zapis linii do pliku w formacie [DATA] KOMUNIKAT
  fprintf(file, "[%s] %s\n", date, message);
  fclose(file);
}