#include "../include/config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/logger.h"

/**
 * @brief Wczytuje i parsuje systemowy plik konfiguracyjny (watchdog.conf).
 * Jeśli plik nie istnieje, tworzy wymagane katalogi i generuje domyślny plik.
 * Zabezpiecza program przed przepełnieniem buforów i niewłaściwym formatem.
 *
 * @param filename Ścieżka do pliku konfiguracyjnego
 * @param watch_list Tablica docelowa na odczytane obiekty ManagedProcess
 * @return Liczba pomyślnie wczytanych procesów lub -1 w przypadku błędu
 */
int read_config(const char *filename, ManagedProcess *watch_list) {
  FILE *file = fopen(filename, "r");

  if (!file) {
    const char *last_slash = strrchr(filename, '/');
    if (last_slash != NULL && last_slash != filename) {
      size_t dir_len = (size_t)(last_slash - filename);
      if (dir_len < 1024) {
        char parent_dir[1024];
        memcpy(parent_dir, filename, dir_len);
        parent_dir[dir_len] = '\0';
        mkdir(parent_dir, 0755);
      }
    }

    file = fopen(filename, "w");

    if (!file) {
      perror("Krytyczny blad: Nie mozna utworzyc pliku konfiguracyjnego (brak uprawnień root? użyj sudo)");
      return -1;
    }

    fprintf(file, "# --- Plik konfiguracyjny Demona Watchdog ---\n");
    fprintf(file, "# Format wpisu:\n");
    fprintf(file, "# Format: <Nazwa> <CPU_sek> <RAM_MB> <Sciezka>\n");
    fprintf(file, "#\n");
    fprintf(file, "# CPU_sek = 0 oznacza brak limitu\n");
    fprintf(file, "# RAM_MB = 0 oznacza brak limitu\n");
    fprintf(file, "#\n");
    fprintf(file, "# Nazwa      CPU_sek     RAM_MB      Sciezka\n");
    fprintf(file, "# Testowy       20       20          /bin/bash\n");
    fclose(file);

    printf("========================================================\n");
    printf("Utworzono domyślny plik konfiguracyjny w:\n");
    printf("--> %s\n\n", filename);
    printf("Wpisz komendę: sudo nano %s\n", filename);
    printf("aby dodać procesy do nadzorowania, a następnie\n");
    printf("uruchom program Watchdog ponownie.\n");
    printf("========================================================\n");
    return 0;
  }

  int process_count = 0;
  char line[2048];  // Bezpieczny bufor zdolny pomieścić ścieżkę nazwę oraz limity

  // Odczyt pliku linia po linii
  while (fgets(line, sizeof(line), file)) {
    // Ignorowanie pustych linii i komentarzy (zaczynających się od '#')
    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
      continue;
    }

    // Informowanie załadowaniu maksymalnej ilości procesów
    if (process_count >= MAX_PROCESSES) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "[WARNING] Załadowano maksymalną dozwoloną liczbę procesów "
               "(%d). Reszta pliku konfiguracyjnego została pominięta.",
               MAX_PROCESSES);
      write_log(msg);
      break;
    }

    // Parsowanie linii do struktury i kontrola przepełnienia
    // Zwiększa licznik tylko jeśli wszystkie 4 argumenty wczytano poprawnie
    if (sscanf(line, "%63s %d %d %1023s", watch_list[process_count].name, &watch_list[process_count].cpu_limit,
               &watch_list[process_count].mem_limit, watch_list[process_count].command) == 4) {
      process_count++;
    }
  }

  fclose(file);

  // Wysłanie komunikatu w przypadku pustego pliku konfiguracyjnego
  if (process_count == 0) {
    printf("Plik konfiguracyjny '%s' jest pusty lub zawiera tylko komentarze.\n", filename);
    printf("Proszę dodać przynajmniej jeden proces do nadzorowania.\n");

    return -1;
  }

  return process_count;
}