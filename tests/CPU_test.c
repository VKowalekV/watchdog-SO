/**
 * @file CPU_test.c
 * @brief Test binary for verifying hard CPU time limits (RLIMIT_CPU).
 */
#include <stdio.h>
#include <unistd.h>

int main() {
  printf("[PID: %d] CPU_test started. Burning CPU...\n", getpid());

  int loop_counter = 0;

  // Infinite loop simulating heavy CPU usage
  while (1) {
    // Busy loop using 'volatile' to prevent compiler optimization
    for (volatile int i = 0; i < 50000000; i++);

    loop_counter++;

    if (loop_counter % 10 == 0) {
      printf("[PID: %d] Still computing...\n", getpid());
    }
  }
  return 0;
}
