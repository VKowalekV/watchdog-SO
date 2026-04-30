/**
 * @file RAM_test.c
 * @brief Skrypt testowy przeznaczony do weryfikacji twardego limitu pamięci
 * (RLIMIT_AS).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main() {
  printf("[PID: %d] RAM_test wystartował. Zaczynam zjadać pamięć...\n", getpid());

  char *ptrs[1000];
  int count = 0;

  // Nieskończona pętla rezerwująca RAM co 1 sekundę
  while (1) {
    // malloc: Alokuje 5 Megabajtów
    ptrs[count] = malloc(5 * 1024 * 1024);

    if (ptrs[count] != NULL) {
      // memset wymusza fizyczne zaalokowanie stron pamięci
      memset(ptrs[count], 0, 5 * 1024 * 1024);
      printf("[PID: %d] Zaalokowano %d MB RAM\n", getpid(), (count + 1) * 5);
    } else {
      // Brak możliwości zaalokowania pamięci oznacza, że limit sprzętowy zablokował dostęp
      printf("[PID: %d] Blad alokacji pamieci! Wymuszam crash...\n", getpid());
      ptrs[count][0] = 'X';  // Próba dostępu wywoła błąd Segmentation Fault (SIGSEGV)
    }

    count++;
    sleep(1);
  }
  return 0;
}