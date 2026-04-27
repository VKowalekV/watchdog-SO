#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include "../include/logger.h"

void write_log(const char *message) {
  int fd = open(LOG_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);

  if (fd == -1)
    return;

  time_t now;
  time(&now);

  struct tm t_info;
  localtime_r(&now, &t_info);
  char date[32];

  strftime(date, sizeof(date), "%Y-%m-%d %H:%M:%S", &t_info);
  dprintf(fd, "[%s] %s\n", date, message);
  close(fd);
}