#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/daemon.h"

void daemonize() {
  pid_t pid;

  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  if (setsid() < 0) {
    exit(EXIT_FAILURE);
  }

  signal(SIGHUP, SIG_IGN);

  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  umask(0);

  if (chdir("/") < 0) {
    exit(EXIT_FAILURE);
  }

  long max_fd = sysconf(_SC_OPEN_MAX);
  for (long fd = 0; fd < max_fd; fd++) {
    close(fd);
  }

  int fd = open("/dev/null", O_RDWR);
  if (fd != -1) {
    if (dup2(fd, STDIN_FILENO) == -1) {
      exit(EXIT_FAILURE);
    }
    if (dup2(fd, STDOUT_FILENO) == -1) {
      exit(EXIT_FAILURE);
    }
    if (dup2(fd, STDERR_FILENO) == -1) {
      exit(EXIT_FAILURE);
    }
    if (fd > 2) {
      close(fd);
    }
  }
}