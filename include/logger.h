#ifndef LOGGER_H
#define LOGGER_H

#define LOG_FILE "/var/log/watchdog.log"

void write_log(const char *message);

#endif