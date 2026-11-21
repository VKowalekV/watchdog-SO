#include "../include/config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/logger.h"

int parse_command_args(const char *command_line, char *buf, size_t buf_size, char *argv[], int max_args) {
  if (command_line == NULL || buf == NULL || argv == NULL || buf_size == 0 || max_args < 2) {
    return -1;
  }

  size_t pos = 0;
  int argc = 0;
  const char *p = command_line;

  while (*p != '\0') {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
      p++;
    }

    if (*p == '\0') {
      break;
    }

    if (argc >= max_args - 1 || pos >= buf_size) {
      return -1;
    }

    argv[argc++] = buf + pos;
    char quote = '\0';

    while (*p != '\0') {
      char c = *p;
      if (quote != '\0') {
        if (c == quote) {
          quote = '\0';
          p++;
          continue;
        }
        if (pos + 1 >= buf_size) {
          return -1;
        }
        buf[pos++] = c;
        p++;
      } else {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
          break;
        }
        if (c == '\'' || c == '"') {
          quote = c;
          p++;
          continue;
        }
        if (pos + 1 >= buf_size) {
          return -1;
        }
        buf[pos++] = c;
        p++;
      }
    }

    if (quote != '\0') {
      return -1;
    }

    buf[pos++] = '\0';
  }

  if (argc == 0) {
    return -1;
  }

  argv[argc] = NULL;
  return argc;
}

/**
 * @brief Wczytuje i parsuje systemowy plik konfiguracyjny (watchdog.conf).
 * Jeśli plik nie istnieje, tworzy wymagane katalogi i generuje domyślny plik.
 * Zabezpiecza program przed przepełnieniem buforów i niewłaściwym formatem.
 *
 * @param filename Ścieżka do pliku konfiguracyjnego
 * @param watch_list Tablica docelowa na odczytane obiekty ManagedProcess
 * @return Liczba pomyślnie wczytanych procesów, 0 gdy utworzono szablon, lub -1 w przypadku błędu
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
    fprintf(file, "# Format: <Nazwa> <CPU_sek> <RAM_MB> <Sciezka> [Argumenty...]\n");
    fprintf(file, "#\n");
    fprintf(file, "# CPU_sek = 0 oznacza brak limitu\n");
    fprintf(file, "# RAM_MB = 0 oznacza brak limitu\n");
    fprintf(file, "#\n");
    fprintf(file, "# Nazwa      CPU_sek     RAM_MB      Sciezka [Argumenty...]\n");
    fprintf(file, "# Testowy       20       20          /bin/sh -c \"while true; do sleep 1; done\"\n");
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
  char line[2048];  // Bezpieczny bufor zdolny pomieścić nazwę, limity oraz polecenie z argumentami

  // Odczyt pliku linia po linii
  while (fgets(line, sizeof(line), file)) {
    size_t start = 0;
    while (line[start] == ' ' || line[start] == '\t') {
      start++;
    }

    // Ignorowanie pustych linii i komentarzy (zaczynających się od '#')
    if (line[start] == '#' || line[start] == '\n' || line[start] == '\r' || line[start] == '\0') {
      continue;
    }

    // Informowanie o załadowaniu maksymalnej liczby procesów
    if (process_count >= MAX_PROCESSES) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "[WARNING] Załadowano maksymalną dozwoloną liczbę procesów "
               "(%d). Reszta pliku konfiguracyjnego została pominięta.",
               MAX_PROCESSES);
      write_log(msg);
      break;
    }

    // Parsowanie pierwszych 3 kolumn oraz reszty linii jako polecenie z argumentami
    int offset = 0;
    if (sscanf(line + start, "%63s %d %d %n", watch_list[process_count].name, &watch_list[process_count].cpu_limit,
               &watch_list[process_count].mem_limit, &offset) == 3 &&
        offset > 0) {
      const char *cmd_start = line + start + offset;
      while (*cmd_start == ' ' || *cmd_start == '\t') {
        cmd_start++;
      }

      size_t len = strlen(cmd_start);
      while (len > 0 && (cmd_start[len - 1] == '\n' || cmd_start[len - 1] == '\r' || cmd_start[len - 1] == ' ' ||
                         cmd_start[len - 1] == '\t')) {
        len--;
      }

      if (len > 0 && len < sizeof(watch_list[process_count].command)) {
        memcpy(watch_list[process_count].command, cmd_start, len);
        watch_list[process_count].command[len] = '\0';

        char arg_buf[sizeof(watch_list[process_count].command)];
        char *argv[MAX_ARGS];
        if (parse_command_args(watch_list[process_count].command, arg_buf, sizeof(arg_buf), argv, MAX_ARGS) > 0) {
          watch_list[process_count].current_pid = 0;
          process_count++;
        } else {
          char msg[256];
          snprintf(msg, sizeof(msg), "[WARNING] Nieprawidłowa składnia polecenia dla '%s' — pominięto.",
                   watch_list[process_count].name);
          write_log(msg);
        }
      }
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
