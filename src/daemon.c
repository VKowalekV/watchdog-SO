#include "../include/daemon.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

/**
 * @brief Zamienia aktualny proces w usługę systemową działającą w tle.
 * Tworzy nowy proces (fork), odłącza go od sesji (setsid), zamyka standardowe
 * strumienie (I/O) i przekierowuje je do /dev/null.
 */
void daemonize() {
  // Rozwidlenie i zakończenie głównego procesu
  pid_t pid = fork();
  if (pid < 0) exit(EXIT_FAILURE);
  if (pid > 0) exit(EXIT_SUCCESS);  // Rodzic kończy pracę

  // Utworzenie nowej sesji, by proces stał się jej liderem
  if (setsid() < 0) exit(EXIT_FAILURE);

  // Drugie rozwidlenie
  pid = fork();
  if (pid < 0) exit(EXIT_FAILURE);
  if (pid > 0) exit(EXIT_SUCCESS);  // Lider sesji kończy pracę

  // Zmiana maski praw dostępu plików oraz przejście do katalogu głównego
  umask(0);
  if (chdir("/") < 0) exit(EXIT_FAILURE);

  // Zamknięcie i przekierowanie wejścia/wyjścia (I/O) do /dev/null
  int fd = open("/dev/null", O_RDWR);
  if (fd != -1) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2) close(fd);
  }
}