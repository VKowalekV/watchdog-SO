#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
  printf("[PID: %d] RAM_test wystartował. Zaczynam zjadać pamięć...\n",
         getpid());

  char *ptrs[1000];
  int count = 0;

  while (1) {
    ptrs[count] = malloc(5 * 1024 * 1024);

    if (ptrs[count] != NULL) {
      memset(ptrs[count], 0, 5 * 1024 * 1024);
      printf("[PID: %d] Zaalokowano %d MB RAM\n", getpid(), (count + 1) * 5);
    } else {
      // printf("[PID: %d] Blad alokacji pamieci! Limit osiagniety?\n",
      // getpid());
      printf("[PID: %d] Blad alokacji pamieci! Wymuszam crash...\n", getpid());
      ptrs[count][0] = 'X';
    }

    count++;
    sleep(1);
  }
  return 0;
}