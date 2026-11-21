#include "../include/config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../include/logger.h"

int parse_command_args(const char *command_line, char *buf, size_t buf_size, char *argv[], int max_args) {
  if (command_line == NULL || buf == NULL || argv == NULL || buf_size == 0 || max_args < 2) {
    return -1;
  }

  size_t pos = 0;
  int argc = 0;
  const char *p = command_line;

  while (*p != '\0') {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
      p++;
    }

    if (*p == '\0') {
      break;
    }

    if (argc >= max_args - 1 || pos >= buf_size) {
      return -1;
    }

    argv[argc++] = buf + pos;
    char quote = '\0';

    while (*p != '\0') {
      char c = *p;
      if (quote != '\0') {
        if (c == quote) {
          quote = '\0';
          p++;
          continue;
        }
        if (pos + 1 >= buf_size) {
          return -1;
        }
        buf[pos++] = c;
        p++;
      } else {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
          break;
        }
        if (c == '\'' || c == '"') {
          quote = c;
          p++;
          continue;
        }
        if (pos + 1 >= buf_size) {
          return -1;
        }
        buf[pos++] = c;
        p++;
      }
    }

    if (quote != '\0') {
      return -1;
    }

    buf[pos++] = '\0';
  }

  if (argc == 0) {
    return -1;
  }

  argv[argc] = NULL;
  return argc;
}

/**
 * @brief Reads and parses the configuration file.
 * If missing, creates required parent directory and generates a default template.
 * Protects against buffer overflows and malformed entries.
 *
 * @param filename Path to the configuration file
 * @param watch_list Destination array for parsed ManagedProcess entries
 * @return Number of loaded processes, 0 if a template was created, or -1 on error
 */
int read_config(const char *filename, ManagedProcess *watch_list) {
  FILE *file = fopen(filename, "r");

  if (!file) {
    const char *last_slash = strrchr(filename, '/');
    if (last_slash != NULL && last_slash != filename) {
      size_t dir_len = (size_t)(last_slash - filename);
      if (dir_len < 1024) {
        char parent_dir[1024];
        memcpy(parent_dir, filename, dir_len);
        parent_dir[dir_len] = '\0';
        mkdir(parent_dir, 0755);
      }
    }

    file = fopen(filename, "w");

    if (!file) {
      perror("Fatal error: Unable to create configuration file (missing permissions? try sudo)");
      return -1;
    }

    fprintf(file, "# --- Watchdog Daemon Configuration File ---\n");
    fprintf(file, "# Entry format:\n");
    fprintf(file, "# Format: <Name> <CPU_sec> <RAM_MB> <Path> [Args...]\n");
    fprintf(file, "#\n");
    fprintf(file, "# CPU_sec = 0 means no limit\n");
    fprintf(file, "# RAM_MB = 0 means no limit\n");
    fprintf(file, "#\n");
    fprintf(file, "# Name       CPU_sec     RAM_MB      Path [Args...]\n");
    fprintf(file, "# Example    20          20          /bin/sh -c \"while true; do sleep 1; done\"\n");
    fclose(file);

    printf("========================================================\n");
    printf("Created default configuration file at:\n");
    printf("--> %s\n\n", filename);
    printf("Edit it (e.g. sudo nano %s) to add supervised processes,\n", filename);
    printf("then run Watchdog again.\n");
    printf("========================================================\n");
    return 0;
  }

  int process_count = 0;
  char line[2048];  // Buffer large enough to hold process name, limits, and command with arguments

  // Read file line by line
  while (fgets(line, sizeof(line), file)) {
    size_t start = 0;
    while (line[start] == ' ' || line[start] == '\t') {
      start++;
    }

    // Ignore empty lines and comments starting with '#'
    if (line[start] == '#' || line[start] == '\n' || line[start] == '\r' || line[start] == '\0') {
      continue;
    }

    // Warn when maximum supervised process limit is reached
    if (process_count >= MAX_PROCESSES) {
      char msg[256];
      snprintf(msg, sizeof(msg),
               "[WARNING] Reached maximum number of supervised processes (%d). Remaining entries were ignored.",
               MAX_PROCESSES);
      write_log(msg);
      break;
    }

    // Parse first 3 columns and keep the remainder of the line as command + arguments
    int offset = 0;
    if (sscanf(line + start, "%63s %d %d %n", watch_list[process_count].name, &watch_list[process_count].cpu_limit,
               &watch_list[process_count].mem_limit, &offset) == 3 &&
        offset > 0) {
      const char *cmd_start = line + start + offset;
      while (*cmd_start == ' ' || *cmd_start == '\t') {
        cmd_start++;
      }

      size_t len = strlen(cmd_start);
      while (len > 0 && (cmd_start[len - 1] == '\n' || cmd_start[len - 1] == '\r' || cmd_start[len - 1] == ' ' ||
                         cmd_start[len - 1] == '\t')) {
        len--;
      }

      if (len > 0 && len < sizeof(watch_list[process_count].command)) {
        memcpy(watch_list[process_count].command, cmd_start, len);
        watch_list[process_count].command[len] = '\0';

        char arg_buf[sizeof(watch_list[process_count].command)];
        char *argv[MAX_ARGS];
        if (parse_command_args(watch_list[process_count].command, arg_buf, sizeof(arg_buf), argv, MAX_ARGS) > 0) {
          watch_list[process_count].current_pid = 0;
          process_count++;
        } else {
          char msg[256];
          snprintf(msg, sizeof(msg), "[WARNING] Invalid command syntax for '%s' - entry skipped.",
                   watch_list[process_count].name);
          write_log(msg);
        }
      }
    }
  }

  fclose(file);

  // Report if no valid process entries were loaded
  if (process_count == 0) {
    fprintf(stderr, "Configuration file '%s' is empty or contains only comments.\n", filename);
    fprintf(stderr, "Please add at least one process to supervise.\n");

    return -1;
  }

  return process_count;
}
