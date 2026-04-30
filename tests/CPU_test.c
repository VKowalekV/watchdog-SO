/**
 * @file CPU_test.c
 * @brief Skrypt testowy przeznaczony do weryfikacji twardego limitu CPU.
 */
#include <stdio.h>
#include <unistd.h>

int main() {
  printf("[PID: %d] CPU_test wystartował. Zaczynam palić procesor...\n", getpid());

  int loop_counter = 0;

  // Nieskończona pętla symulująca olbrzymie zużycie czasu procesora
  while (1) {
    // Pusta, ale czasochłonna pętla z modyfikatorem 'volatile'
    // 'volatile' zapobiega wycięciu tej pętli przez kompilator
    for (volatile int i = 0; i < 50000000; i++);

    loop_counter++;

    if (loop_counter % 10 == 0) {
      printf("[PID: %d] Trwa intensywne liczenie...\n", getpid());
    }
  }
  return 0;
}