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
 * @brief Bezpieczny zapis logu z datą i czasem do pliku logów.
 * @param message Treść komunikatu do zapisania
 */
void write_log(const char *message);

#endif