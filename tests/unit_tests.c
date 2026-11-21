#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/config.h"
#include "../include/logger.h"

static int tests_run = 0;

#define RUN_TEST(fn)                                     \
  do {                                                   \
    printf("Running %s...\n", #fn);                      \
    fn();                                                \
    tests_run++;                                         \
  } while (0)

static void test_parse_single_path(void) {
  char buf[256];
  char *argv[MAX_ARGS];

  int argc = parse_command_args("/bin/ls", buf, sizeof(buf), argv, MAX_ARGS);
  assert(argc == 1);
  assert(strcmp(argv[0], "/bin/ls") == 0);
  assert(argv[1] == NULL);
}

static void test_parse_multiple_args(void) {
  char buf[256];
  char *argv[MAX_ARGS];

  int argc = parse_command_args("/usr/bin/python3 /opt/app.py --port 8080", buf, sizeof(buf), argv, MAX_ARGS);
  assert(argc == 4);
  assert(strcmp(argv[0], "/usr/bin/python3") == 0);
  assert(strcmp(argv[1], "/opt/app.py") == 0);
  assert(strcmp(argv[2], "--port") == 0);
  assert(strcmp(argv[3], "8080") == 0);
  assert(argv[4] == NULL);
}

static void test_parse_quotes_and_whitespace(void) {
  char buf[256];
  char *argv[MAX_ARGS];

  int argc = parse_command_args("  /bin/sh\t -c  \"while true; do sleep 1; done\"  'single quoted'  foo'bar baz'qux  ",
                                buf, sizeof(buf), argv, MAX_ARGS);
  assert(argc == 5);
  assert(strcmp(argv[0], "/bin/sh") == 0);
  assert(strcmp(argv[1], "-c") == 0);
  assert(strcmp(argv[2], "while true; do sleep 1; done") == 0);
  assert(strcmp(argv[3], "single quoted") == 0);
  assert(strcmp(argv[4], "foobar bazqux") == 0);
  assert(argv[5] == NULL);

  argc = parse_command_args("/bin/echo \"\" end", buf, sizeof(buf), argv, MAX_ARGS);
  assert(argc == 3);
  assert(strcmp(argv[0], "/bin/echo") == 0);
  assert(strcmp(argv[1], "") == 0);
  assert(strcmp(argv[2], "end") == 0);
  assert(argv[3] == NULL);
}

static void test_parse_invalid_inputs(void) {
  char buf[256];
  char small_buf[8];
  char *argv[MAX_ARGS];

  assert(parse_command_args("", buf, sizeof(buf), argv, MAX_ARGS) == -1);
  assert(parse_command_args("   \t\r\n  ", buf, sizeof(buf), argv, MAX_ARGS) == -1);
  assert(parse_command_args(NULL, buf, sizeof(buf), argv, MAX_ARGS) == -1);
  assert(parse_command_args("/bin/ls", NULL, sizeof(buf), argv, MAX_ARGS) == -1);
  assert(parse_command_args("/bin/ls", buf, 0, argv, MAX_ARGS) == -1);
  assert(parse_command_args("/bin/ls", buf, sizeof(buf), argv, 1) == -1);

  assert(parse_command_args("/bin/sh -c \"unterminated", buf, sizeof(buf), argv, MAX_ARGS) == -1);
  assert(parse_command_args("/bin/sh -c 'unterminated", buf, sizeof(buf), argv, MAX_ARGS) == -1);

  assert(parse_command_args("/bin/echo too_long_for_small_buffer", small_buf, sizeof(small_buf), argv, MAX_ARGS) == -1);

  char many_args[512] = "/bin/echo";
  for (int i = 0; i < MAX_ARGS; i++) {
    strcat(many_args, " a");
  }
  assert(parse_command_args(many_args, buf, sizeof(buf), argv, MAX_ARGS) == -1);
}

static void make_temp_dir(char *tmpl, size_t size) {
  snprintf(tmpl, size, "/tmp/watchdog_unit_XXXXXX");
  char *created = mkdtemp(tmpl);
  assert(created != NULL);
}

static void test_read_config_valid_and_comments(void) {
  char dir[128];
  make_temp_dir(dir, sizeof(dir));

  char conf_path[256];
  char log_path[256];
  snprintf(conf_path, sizeof(conf_path), "%s/watchdog.conf", dir);
  snprintf(log_path, sizeof(log_path), "%s/test.log", dir);
  logger_init(log_path, 0);

  FILE *f = fopen(conf_path, "w");
  assert(f != NULL);
  fprintf(f,
          "# top comment\n"
          "   # indented comment\n"
          "\n"
          "   \t  \n"
          "alpha 0 0 /bin/sh -c \"sleep 1\"\n"
          "beta 15 64 /usr/bin/env VAR=1 /bin/true\n");
  fclose(f);

  ManagedProcess list[MAX_PROCESSES];
  int count = read_config(conf_path, list);
  assert(count == 2);
  assert(strcmp(list[0].name, "alpha") == 0);
  assert(list[0].cpu_limit == 0);
  assert(list[0].mem_limit == 0);
  assert(strcmp(list[0].command, "/bin/sh -c \"sleep 1\"") == 0);
  assert(list[0].current_pid == 0);

  assert(strcmp(list[1].name, "beta") == 0);
  assert(list[1].cpu_limit == 15);
  assert(list[1].mem_limit == 64);
  assert(strcmp(list[1].command, "/usr/bin/env VAR=1 /bin/true") == 0);
  assert(list[1].current_pid == 0);

  unlink(conf_path);
  unlink(log_path);
  rmdir(dir);
}

static void test_read_config_empty_only_comments(void) {
  char dir[128];
  make_temp_dir(dir, sizeof(dir));

  char conf_path[256];
  snprintf(conf_path, sizeof(conf_path), "%s/empty.conf", dir);

  FILE *f = fopen(conf_path, "w");
  assert(f != NULL);
  fprintf(f, "# only comment\n\n  # indented comment\n");
  fclose(f);

  ManagedProcess list[MAX_PROCESSES];
  assert(read_config(conf_path, list) == -1);

  unlink(conf_path);
  rmdir(dir);
}

static void test_read_config_creates_template_when_missing(void) {
  char dir[128];
  make_temp_dir(dir, sizeof(dir));

  char sub_dir[256];
  char conf_path[512];
  snprintf(sub_dir, sizeof(sub_dir), "%s/cfg", dir);
  snprintf(conf_path, sizeof(conf_path), "%s/watchdog.conf", sub_dir);

  ManagedProcess list[MAX_PROCESSES];
  assert(read_config(conf_path, list) == 0);
  assert(access(conf_path, F_OK) == 0);

  /* Generated template has only comments, so re-reading returns -1 */
  assert(read_config(conf_path, list) == -1);

  unlink(conf_path);
  rmdir(sub_dir);
  rmdir(dir);
}

static void test_read_config_skips_invalid_syntax_line(void) {
  char dir[128];
  make_temp_dir(dir, sizeof(dir));

  char conf_path[256];
  char log_path[256];
  snprintf(conf_path, sizeof(conf_path), "%s/bad_line.conf", dir);
  snprintf(log_path, sizeof(log_path), "%s/test.log", dir);
  logger_init(log_path, 0);

  FILE *f = fopen(conf_path, "w");
  assert(f != NULL);
  fprintf(f,
          "broken 0 0 /bin/sh -c \"unclosed quote\n"
          "valid 10 20 /bin/true --flag\n");
  fclose(f);

  ManagedProcess list[MAX_PROCESSES];
  int count = read_config(conf_path, list);
  assert(count == 1);
  assert(strcmp(list[0].name, "valid") == 0);
  assert(strcmp(list[0].command, "/bin/true --flag") == 0);

  unlink(conf_path);
  unlink(log_path);
  rmdir(dir);
}

static void test_read_config_caps_at_max_processes(void) {
  char dir[128];
  make_temp_dir(dir, sizeof(dir));

  char conf_path[256];
  char log_path[256];
  snprintf(conf_path, sizeof(conf_path), "%s/many.conf", dir);
  snprintf(log_path, sizeof(log_path), "%s/test.log", dir);
  logger_init(log_path, 0);

  FILE *f = fopen(conf_path, "w");
  assert(f != NULL);
  for (int i = 0; i < MAX_PROCESSES + 2; i++) {
    fprintf(f, "proc_%d 0 0 /bin/true\n", i);
  }
  fclose(f);

  ManagedProcess list[MAX_PROCESSES];
  int count = read_config(conf_path, list);
  assert(count == MAX_PROCESSES);
  assert(strcmp(list[0].name, "proc_0") == 0);
  assert(strcmp(list[MAX_PROCESSES - 1].name, "proc_31") == 0);

  unlink(conf_path);
  unlink(log_path);
  rmdir(dir);
}

int main(void) {
  RUN_TEST(test_parse_single_path);
  RUN_TEST(test_parse_multiple_args);
  RUN_TEST(test_parse_quotes_and_whitespace);
  RUN_TEST(test_parse_invalid_inputs);
  RUN_TEST(test_read_config_valid_and_comments);
  RUN_TEST(test_read_config_empty_only_comments);
  RUN_TEST(test_read_config_creates_template_when_missing);
  RUN_TEST(test_read_config_skips_invalid_syntax_line);
  RUN_TEST(test_read_config_caps_at_max_processes);

  printf("All %d unit tests passed.\n", tests_run);
  return 0;
}
