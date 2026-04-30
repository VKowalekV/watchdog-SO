/**
 * @file config.h
 * @brief Nagłówek modułu konfiguracyjnego.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <sys/types.h>

/**
 * @def MAX_PROCESSES
 * @brief Maksymalna liczba procesów, jaką może jednocześnie nadzorować demon.
 */
#define MAX_PROCESSES 32

/**
 * @def CONFIG_FILE
 * @brief Globalna ścieżka do pliku konfiguracyjnego w systemie.
 */
#define CONFIG_FILE "/etc/watchdog/watchdog.conf"

/**
 * @struct ManagedProcess
 * @brief Struktura przechowująca pełną informację o pojedynczym nadzorowanym procesie.
 */
typedef struct {
  char name[64];      // Nazwa własna procesu (dla czytelności logów)
  int cpu_limit;      // Twardy limit czasu procesora (w sekundach) 0 jeśli brak limitu
  int mem_limit;      // Twardy limit pamięci RAM (w megabajtach) 0 jeśli brak limitu
  char command[1024]; // Bezwzględna ścieżka do pliku wykonywalnego
  pid_t current_pid;  // Aktualny numer PID uruchomionego procesu (0 jeśli martwy)
} ManagedProcess;

/**
 * @brief Wczytuje i parsuje systemowy plik konfiguracyjny
 * Jeśli plik nie istnieje, tworzy wymagane katalogi i generuje domyślny plik.
 * Zabezpiecza program przed przepełnieniem buforów i niewłaściwym formatem.
 *
 * @param filename Ścieżka do pliku konfiguracyjnego
 * @param watch_list Tablica docelowa na odczytane obiekty ManagedProcess
 * @return Liczba pomyślnie wczytanych procesów lub -1 w przypadku błędu
 */
int read_config(const char *filename, ManagedProcess *watch_list);

#endif