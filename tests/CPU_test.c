#include <stdio.h>
#include <unistd.h>

int main() {
  printf("[PID: %d] CPU_test wystartował. Zaczynam palić procesor...\n",
         getpid());

  int loop_counter = 0;

  while (1) {
    for (volatile int i = 0; i < 50000000; i++)
      ;

    loop_counter++;
    if (loop_counter % 10 == 0) {
      printf("[PID: %d] Trwa intensywne liczenie...\n", getpid());
    }
  }
  return 0;
}