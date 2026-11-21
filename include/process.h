/**
 * @file process.h
 * @brief Nagłówek modułu nadzorującego procesy potomne.
 */
#ifndef PROCESS_H
#define PROCESS_H

#include "config.h"
#include <sys/types.h>

/**
 * @brief Inicjalizuje procesy podczas startu demona.
 * Wczytuje plik konfiguracyjny i dla każdego zdefiniowanego procesu wywołuje fork/exec.
 * @param config_path Ścieżka do pliku konfiguracyjnego
 */
void process_init(const char *config_path);

/**
 * @brief Obsługuje sygnał SIGHUP. Porównuje nową konfigurację ze starą i wprowadza zmiany.
 * @param config_path Ścieżka do pliku konfiguracyjnego
 */
void process_reload(const char *config_path);

/**
 * @brief Reakcja na śmierć procesu potomnego (restart i zalogowanie przyczyny).
 * @param dead_pid Numer PID procesu, który zmarł
 * @param status Kod wyjścia zwrócony przez wywołanie waitpid
 */
void process_handle_death(pid_t dead_pid, int status);

/**
 * @brief Bezpieczne zamknięcie wszystkich nadzorowanych procesów poprzez wysłanie SIGTERM.
 */
void process_shutdown();

#endif