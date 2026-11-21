/**
 * @file config.h
 * @brief Configuration module header.
 */
#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>
#include <sys/types.h>

/**
 * @def MAX_PROCESSES
 * @brief Maximum number of processes the daemon can supervise simultaneously.
 */
#define MAX_PROCESSES 32

/**
 * @def MAX_ARGS
 * @brief Maximum number of arguments (including executable path and trailing NULL) per process.
 */
#define MAX_ARGS 32

/**
 * @def CONFIG_FILE
 * @brief Default system configuration file path.
 */
#define CONFIG_FILE "/etc/watchdog/watchdog.conf"

/**
 * @struct ManagedProcess
 * @brief Structure holding metadata and limits for a single supervised process.
 */
typedef struct {
  char name[64];       // Process label (used in log entries)
  int cpu_limit;       // Hard CPU time limit in seconds (0 = unlimited)
  int mem_limit;       // Hard RAM limit in megabytes (0 = unlimited)
  char command[1024];  // Executable path with optional CLI arguments
  pid_t current_pid;   // Current PID of the running process (0 if inactive)
} ManagedProcess;

/**
 * @brief Splits a configuration command string into an argv array for execvp.
 * Supports single ('...') and double ("...") quotes.
 *
 * @param command_line Full command string (path + optional arguments)
 * @param buf Scratch buffer for tokenized arguments
 * @param buf_size Size of scratch buffer in bytes
 * @param argv Output pointer array (NULL-terminated)
 * @param max_args Capacity of argv array (minimum 2)
 * @return Number of parsed arguments (> 0), or -1 on syntax/overflow error
 */
int parse_command_args(const char *command_line, char *buf, size_t buf_size, char *argv[], int max_args);

/**
 * @brief Reads and parses the configuration file.
 * If missing, creates required parent directory and generates a default template.
 * Protects against buffer overflows and malformed entries.
 *
 * @param filename Path to the configuration file
 * @param watch_list Destination array for parsed ManagedProcess entries
 * @return Number of loaded processes, 0 if a template was created, or -1 on error
 */
int read_config(const char *filename, ManagedProcess *watch_list);

#endif
