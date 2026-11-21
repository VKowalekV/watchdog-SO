#include "../include/daemon.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Turns the current process into a background system daemon.
 * Forks a new process, detaches from the session (setsid), closes standard
 * I/O streams, and redirects them to /dev/null.
 */
void daemonize() {
  // First fork and terminate parent process
  pid_t pid = fork();
  if (pid < 0) exit(EXIT_FAILURE);
  if (pid > 0) exit(EXIT_SUCCESS);  // Parent exits

  // Create new session so child becomes session leader
  if (setsid() < 0) exit(EXIT_FAILURE);

  // Second fork
  pid = fork();
  if (pid < 0) exit(EXIT_FAILURE);
  if (pid > 0) exit(EXIT_SUCCESS);  // Session leader exits

  // Reset file mode mask and change working directory to root
  umask(0);
  if (chdir("/") < 0) exit(EXIT_FAILURE);

  // Redirect standard I/O streams to /dev/null
  int fd = open("/dev/null", O_RDWR);
  if (fd != -1) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) close(fd);
  }
}
