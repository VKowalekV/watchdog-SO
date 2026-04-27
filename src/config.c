#include "../include/config.h"
#include <stdio.h>

int read_config(const char *filename, ManagedProcess *watch_list) {
  FILE *file = fopen(filename, "r");

  if (!file) {
    file = fopen(filename, "w");

    if (!file) {
      perror("Krytyczny blad: Nie mozna utworzyc pliku konfiguracyjnego");
      return -1;
    }

    fprintf(file, "# --- Plik konfiguracyjny Demona Watchdog ---\n");
    fprintf(file, "# Format wpisu:\n");
    fprintf(file, "# Format: <Nazwa> <CPU_sek> <RAM_MB> <Sciezka>\n");
    fprintf(file, "# CPU_sek = 0 oznacza brak limitu\n");
    fprintf(file, "# RAM_MB = 0 oznacza brak limitu\n");
    fprintf(file, "#\n");
    fprintf(file, "# Nazwa      CPU_sek    RAM_MB    Sciezka\n");
    fprintf(file, "# Testowy      20         20      /bin/bash\n");
    fclose(file);

    printf("========================================================\n");
    printf("Utworzono domyślny plik: %s.\n", filename);
    printf("Proszę edytować ten plik, dodać procesy do nadzorowania\n");
    printf("i uruchomić program Watchdog ponownie.\n");
    printf("========================================================\n");
    return 0;
  }

  int process_count = 0;
  char line[512];

  while (process_count < MAX_PROCESSES && fgets(line, sizeof(line), file)) {

    if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
      continue;
    }

    if (sscanf(line, "%63s %d %d %255s", watch_list[process_count].name,
               &watch_list[process_count].cpu_limit,
               &watch_list[process_count].mem_limit,
               watch_list[process_count].command) == 4) {
      process_count++;
    }
  }

  fclose(file);

  if (process_count == 0) {
    printf(
        "Plik konfiguracyjny '%s' jest pusty lub zawiera tylko komentarze.\n",
        filename);
    printf("Proszę dodać przynajmniej jeden proces do nadzorowania.\n");

    return -1;
  }

  return process_count;
}