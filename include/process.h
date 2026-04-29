#ifndef PROCESS_H
#define PROCESS_H

#include <sys/types.h>
#include "config.h"

void process_init();
void process_reload();
void process_handle_death(pid_t dead_pid, int status);
void process_shutdown();

#endif