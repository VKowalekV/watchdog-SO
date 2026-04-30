/**
 * @file daemon.h
 * @brief Nagłówek modułu odpowiedzialnego za tło i odłączenie od terminala.
 */
#ifndef DAEMON_H
#define DAEMON_H

/**
 * @brief Funkcja zmieniająca aktualny proces w systemowego demona.
 * Tworzy nowy proces (fork), odłącza go od sesji (setsid), zamyka standardowe
 * strumienie (I/O) i przekierowuje je do /dev/null.
 */
void daemonize();

#endif