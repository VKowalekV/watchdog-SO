/**
 * @file config.h
 * @brief Nagłówek modułu konfiguracyjnego.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <sys/types.h>

/**
 * @def MAX_PROCESSES
 * @brief Maksymalna liczba procesów, jaką może jednocześnie nadzorować demon.
 */
#define MAX_PROCESSES 32

/**
 * @def MAX_ARGS
 * @brief Maksymalna liczba argumentów (włącznie ze ścieżką i końcowym NULL) dla procesu.
 */
#define MAX_ARGS 32

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
  char name[64];       // Nazwa własna procesu (dla czytelności logów)
  int cpu_limit;       // Twardy limit czasu procesora (w sekundach) 0 jeśli brak limitu
  int mem_limit;       // Twardy limit pamięci RAM (w megabajtach) 0 jeśli brak limitu
  char command[1024];  // Ścieżka do pliku wykonywalnego wraz z opcjonalnymi argumentami CLI
  pid_t current_pid;   // Aktualny numer PID uruchomionego procesu (0 jeśli martwy)
} ManagedProcess;

/**
 * @brief Dzieli polecenie z konfiguracji na tablicę argumentów argv dla execvp.
 * Obsługuje cudzysłowy pojedyncze ('...') i podwójne ("...").
 *
 * @param command_line Pełny napis polecenia (ścieżka + opcjonalne argumenty)
 * @param buf Bufor roboczy na ztokenizowane argumenty
 * @param buf_size Rozmiar bufora roboczego w bajtach
 * @param argv Tablica docelowa wskaźników (zakończona NULL)
 * @param max_args Pojemność tablicy argv (minimum 2)
 * @return Liczba sparsowanych argumentów (> 0) lub -1 przy błędzie składni/przepełnieniu
 */
int parse_command_args(const char *command_line, char *buf, size_t buf_size, char *argv[], int max_args);

/**
 * @brief Wczytuje i parsuje systemowy plik konfiguracyjny
 * Jeśli plik nie istnieje, tworzy wymagane katalogi i generuje domyślny plik.
 * Zabezpiecza program przed przepełnieniem buforów i niewłaściwym formatem.
 *
 * @param filename Ścieżka do pliku konfiguracyjnego
 * @param watch_list Tablica docelowa na odczytane obiekty ManagedProcess
 * @return Liczba pomyślnie wczytanych procesów, 0 gdy utworzono szablon, lub -1 przy błędzie
 */
int read_config(const char *filename, ManagedProcess *watch_list);

#endif
