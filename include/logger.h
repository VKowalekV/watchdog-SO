/**
 * @file logger.h
 * @brief Nagłówek modułu logowania.
 */
#ifndef LOGGER_H
#define LOGGER_H

/**
 * @def LOG_FILE
 * @brief Globalna ścieżka do pliku systemowego, w którym zapisywane są logi demona.
 */
#define LOG_FILE "/var/log/watchdog.log"

/**
 * @brief Konfiguruje docelowy plik logów oraz opcjonalne wypisywanie na stderr.
 * @param log_path Ścieżka do pliku logów (NULL lub pusty napis zachowuje domyślną)
 * @param mirror_stderr Wartość niezerowa włącza dublowanie logów na stderr
 */
void logger_init(const char *log_path, int mirror_stderr);

/**
 * @brief Bezpieczny zapis logu z datą i czasem do pliku logów.
 * @param message Treść komunikatu do zapisania
 */
void write_log(const char *message);

#endif