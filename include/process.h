/**
 * @file process.h
 * @brief Header for child process supervision module.
 */
#ifndef PROCESS_H
#define PROCESS_H

#include "config.h"
#include <sys/types.h>

/**
 * @brief Initializes supervised processes on daemon startup.
 * Loads the configuration file and spawns each configured entry via fork/exec.
 * @param config_path Path to the configuration file
 */
void process_init(const char *config_path);

/**
 * @brief Handles SIGHUP. Diffs new configuration against running state and applies changes.
 * @param config_path Path to the configuration file
 */
void process_reload(const char *config_path);

/**
 * @brief Handles child termination (logs cause and restarts the process).
 * @param dead_pid PID of the terminated child process
 * @param status Wait status returned by waitpid
 */
void process_handle_death(pid_t dead_pid, int status);

/**
 * @brief Gracefully stops all supervised processes by sending SIGTERM.
 */
void process_shutdown();

#endif
