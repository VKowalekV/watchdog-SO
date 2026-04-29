#ifndef CONFIG_H
#define CONFIG_H

#include <sys/types.h>

#define MAX_PROCESSES 32
#define CONFIG_FILE "/etc/watchdog/watchdog.conf"

typedef struct {
  char name[64];     // Nazwa wlasna procesu (dla czytelnosci logow)
  int cpu_limit;     // Limit CPU w sekundach
  int mem_limit;     // Limit RAM w megabajtach
  char command[1024]; // Sciezka do pliku wykonywalnego
  pid_t current_pid; // Aktualny numer PID uruchomionego procesu 0 jesli jest
                     // martwy
} ManagedProcess;

int read_config(const char *filename, ManagedProcess *list);

#endif