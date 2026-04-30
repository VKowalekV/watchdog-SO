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
 * @brief Zapisuje wiadomość do pliku logów wraz z aktualną datą i czasem.
 * @param message Treść komunikatu
 */
void write_log(const char *message);

#endif