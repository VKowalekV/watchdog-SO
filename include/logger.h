/**
 * @file logger.h
 * @brief Logging module header.
 */
#ifndef LOGGER_H
#define LOGGER_H

/**
 * @def LOG_FILE
 * @brief Default system path for the daemon log file.
 */
#define LOG_FILE "/var/log/watchdog.log"

/**
 * @brief Configures the target log file and optional mirroring to stderr.
 * @param log_path Path to log file (NULL or empty string keeps the default)
 * @param mirror_stderr Non-zero value enables mirroring log lines to stderr
 */
void logger_init(const char *log_path, int mirror_stderr);

/**
 * @brief Safely appends a timestamped message to the log file.
 * @param message Message text to write
 */
void write_log(const char *message);

#endif
