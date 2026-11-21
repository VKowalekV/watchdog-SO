/**
 * @file daemon.h
 * @brief Header for background daemonization and terminal detachment.
 */
#ifndef DAEMON_H
#define DAEMON_H

/**
 * @brief Turns the current process into a background system daemon.
 * Forks a new process, detaches from the session (setsid), closes standard
 * I/O streams, and redirects them to /dev/null.
 */
void daemonize();

#endif
