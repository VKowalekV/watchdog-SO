#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_FILE "/tmp/watchdog.log"

void write_log(const char *message);

#endif