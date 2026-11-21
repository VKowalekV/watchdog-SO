#include "../include/process.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "../include/config.h"
#include "../include/logger.h"

// Global array holding currently supervised processes
static ManagedProcess watch_list[MAX_PROCESSES];
// Number of currently supervised processes
static int process_count = 0;

// Internal static helper declarations
static void start_process(ManagedProcess *process);
static void get_death_reason(int status, char *reason, size_t size);

/**
 * @brief Initializes supervised processes on daemon startup.
 * Loads the configuration file and spawns each configured entry via fork/exec.
 * @param config_path Path to the configuration file
 */
void process_init(const char *config_path) {
  process_count = read_config(config_path, watch_list);

  if (process_count <= 0) {
    write_log("[SYSTEM] Failed to load configuration or config file is empty.");
    return;
  }

  for (int i = 0; i < process_count; i++) {
    watch_list[i].current_pid = 0;
    start_process(&watch_list[i]);
  }
}

/**
 * @brief Handles SIGHUP. Diffs new configuration against running state and applies changes.
 * @param config_path Path to the configuration file
 */
void process_reload(const char *config_path) {
  write_log("[SYSTEM] Received SIGHUP. Reloading configuration...");

  int isChanged = 0;

  ManagedProcess new_watch_list[MAX_PROCESSES];
  memset(new_watch_list, 0, sizeof(new_watch_list));
  int new_process_count = read_config(config_path, new_watch_list);

  if (new_process_count <= 0) {
    write_log("[SYSTEM] Configuration file is empty. Aborting reload.");
    return;
  }

  // Check existing processes
  // If a process was removed or its parameters (e.g. limits) changed,
  // terminate its entire process group with SIGTERM
  for (int i = 0; i < process_count; i++) {
    int found = 0;

    for (int j = 0; j < new_process_count; j++) {
      if (strcmp(watch_list[i].name, new_watch_list[j].name) == 0) {
        if (strcmp(watch_list[i].command, new_watch_list[j].command) == 0 &&
            watch_list[i].cpu_limit == new_watch_list[j].cpu_limit &&
            watch_list[i].mem_limit == new_watch_list[j].mem_limit) {
          // Preserve current PID in the new table so it is not restarted
          new_watch_list[j].current_pid = watch_list[i].current_pid;
        } else {
          isChanged = 1;
          char msg[1024];

          snprintf(msg, sizeof(msg), "[RELOAD] Configuration changed for '%s'. Stopping previous process...",
                   watch_list[i].name);

          write_log(msg);

          if (watch_list[i].current_pid > 0) {
            kill(-watch_list[i].current_pid, SIGTERM);
          }
        }
        found = 1;
        break;
      }
    }
    if (!found && watch_list[i].current_pid > 0) {
      isChanged = 1;
      char msg[1024];

      snprintf(msg, sizeof(msg), "[RELOAD] Removed process '%s' from configuration. Stopping...", watch_list[i].name);

      write_log(msg);
      kill(-watch_list[i].current_pid, SIGTERM);
    }
  }

  // Copy updated configuration into active state
  memcpy(watch_list, new_watch_list, sizeof(ManagedProcess) * MAX_PROCESSES);
  process_count = new_process_count;

  // Start new or modified processes whose PID is 0
  for (int i = 0; i < process_count; i++) {
    if (watch_list[i].current_pid == 0) {
      isChanged = 1;
      start_process(&watch_list[i]);
    }
  }

  if (isChanged == 1) {
    write_log("[SYSTEM] Configuration reload completed successfully.");
  } else {
    write_log("[SYSTEM] Configuration unchanged.");
  }
}

/**
 * @brief Handles child termination (logs cause and restarts the process).
 * @param dead_pid PID of the terminated child process
 * @param status Wait status returned by waitpid
 */
void process_handle_death(pid_t dead_pid, int status) {
  for (int i = 0; i < process_count; i++) {
    if (watch_list[i].current_pid == dead_pid) {
      watch_list[i].current_pid = 0;

      char reason[128];
      get_death_reason(status, reason, sizeof(reason));

      char msg[1024];
      snprintf(msg, sizeof(msg), "[RESTART] Process '%s' (PID: %d) terminated. Reason: %s. Restarting...",
               watch_list[i].name, dead_pid, reason);
      write_log(msg);

      sleep(1);
      start_process(&watch_list[i]);
      break;
    }
  }
}

/**
 * @brief Gracefully stops all supervised processes by sending SIGTERM.
 */
void process_shutdown() {
  write_log("[SYSTEM] Stopping supervised child processes...");
  for (int i = 0; i < process_count; i++) {
    if (watch_list[i].current_pid > 0) {
      kill(-watch_list[i].current_pid, SIGTERM);
    }
  }
}

/**
 * @brief Forks a new child process and applies resource limits.
 * @param process Pointer to the supervised process entry
 */
static void start_process(ManagedProcess *process) {
  char arg_buf[sizeof(process->command)];
  char *argv[MAX_ARGS];
  if (parse_command_args(process->command, arg_buf, sizeof(arg_buf), argv, MAX_ARGS) <= 0) {
    char msg[2048];
    snprintf(msg, sizeof(msg), "[ERROR] Invalid command for process '%s' (%s).", process->name, process->command);
    write_log(msg);
    return;
  }

  if (access(argv[0], X_OK) == -1) {
    char msg[2048];
    snprintf(msg, sizeof(msg), "[ERROR] Executable for process '%s' (%s) does not exist or is not executable.",
             process->name, argv[0]);
    write_log(msg);
    return;
  }

  pid_t pid = fork();  // Clone process address space

  if (pid == 0) {
    // Isolate process in its own process group so child trees can be terminated together
    setpgid(0, 0);

    // Apply kernel resource limits
    if (process->mem_limit > 0) {
      struct rlimit rl_mem;
      rl_mem.rlim_cur = rl_mem.rlim_max = (rlim_t)process->mem_limit * 1024ULL * 1024ULL;  // MB to bytes
      setrlimit(RLIMIT_AS, &rl_mem);
    }

    if (process->cpu_limit > 0) {
      struct rlimit rl_cpu;
      rl_cpu.rlim_cur = rl_cpu.rlim_max = process->cpu_limit;
      setrlimit(RLIMIT_CPU, &rl_cpu);
    }

    // Replace cloned Watchdog process with target executable
    execvp(argv[0], argv);
    _exit(EXIT_FAILURE);  // Should never be reached unless executable disappeared

  } else if (pid > 0) {
    process->current_pid = pid;

    char msg[1024];
    snprintf(msg, sizeof(msg), "[START] Started '%s' (PID: %d) | Limits: CPU %d s, RAM %d MB", process->name, pid,
             process->cpu_limit, process->mem_limit);

    write_log(msg);
  } else {
    char msg[1024];
    snprintf(msg, sizeof(msg), "[ERROR] fork() failed for '%s'.", process->name);
    write_log(msg);
  }
}

/**
 * @brief Converts a waitpid termination status into a human-readable reason string.
 * @param status Wait status from waitpid
 * @param reason Destination buffer for description
 * @param size Buffer size
 */
static void get_death_reason(int status, char *reason, size_t size) {
  if (WIFEXITED(status)) {
    snprintf(reason, size, "exit code %d", WEXITSTATUS(status));
  } else if (WIFSIGNALED(status)) {
    int signal_num = WTERMSIG(status);
    if (signal_num == SIGKILL) {
      snprintf(reason, size, "SIGKILL (hard CPU limit or OOM kill)");
    } else if (signal_num == SIGSEGV) {
      snprintf(reason, size, "SIGSEGV (segmentation fault)");
    } else if (signal_num == SIGXCPU) {
      snprintf(reason, size, "SIGXCPU (CPU time limit exceeded)");
    } else if (signal_num == SIGTERM) {
      snprintf(reason, size, "SIGTERM (terminated)");
    } else {
      snprintf(reason, size, "signal %d", signal_num);
    }
  } else {
    snprintf(reason, size, "unknown");
  }
}
