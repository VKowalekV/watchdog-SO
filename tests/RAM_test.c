/**
 * @file RAM_test.c
 * @brief Test binary for verifying memory address-space limits (RLIMIT_AS).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
  printf("[PID: %d] RAM_test started. Allocating memory...\n", getpid());

  char *ptrs[1000];
  int count = 0;

  // Infinite loop allocating RAM every second
  while (1) {
    // Allocate 5 MB chunk
    ptrs[count] = malloc(5 * 1024 * 1024);

    if (ptrs[count] != NULL) {
      // Touch allocated pages with memset to back them physically
      memset(ptrs[count], 0, 5 * 1024 * 1024);
      printf("[PID: %d] Allocated %d MB RAM\n", getpid(), (count + 1) * 5);
    } else {
      // Allocation failure indicates the kernel memory limit has been reached
      printf("[PID: %d] Memory allocation failed! Triggering crash...\n", getpid());
      ptrs[count][0] = 'X';  // Dereferencing NULL triggers Segmentation Fault (SIGSEGV)
    }

    count++;
    sleep(1);
  }
  return 0;
}
