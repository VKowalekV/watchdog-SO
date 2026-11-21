> [Polska wersja](README.pl.md)

# Watchdog – Linux Process Supervisor Daemon

A lightweight, zero-dependency Linux process supervisor daemon written in pure C. It launches configured programs as supervised child processes, enforces per-process CPU and memory resource limits at the kernel level, monitors process health, and automatically restarts failed processes.

---

## Key Features

- **Daemonization & Foreground Mode:** Detaches from the controlling terminal using a double-`fork`, `setsid()`, `umask(0)`, `chdir("/")`, and `/dev/null` redirection, or stays in the foreground (`--foreground`) with logs mirrored to `stderr`.
- **Flexible Runtime Paths:** Supports custom config, log, and PID file paths via CLI flags (`--config`, `--log`, `--pid`), enabling non-root and containerized usage.
- **Declarative Configuration:** Supervises up to 32 concurrent processes (`MAX_PROCESSES`) defined in `/etc/watchdog/watchdog.conf`.
- **Kernel-Enforced Resource Limits:** Applies per-process limits via `setrlimit()` (`RLIMIT_CPU` in seconds and `RLIMIT_AS` in MB; `0` disables a limit).
- **Process-Group Isolation:** Places each child into its own process group (`setpgid(0, 0)`) so process trees can be cleanly terminated together.
- **Automatic Restart & Diagnostics:** Uses blocking `waitpid()` to detect termination and logs exit codes or terminating signals (`SIGXCPU`, `SIGSEGV`, `SIGKILL`, `SIGTERM`) before restarting.
- **Zero-Downtime Hot-Reload:** Reloads `/etc/watchdog/watchdog.conf` on `SIGHUP`, keeping unchanged processes running while updating or removing modified entries.
- **Graceful Shutdown:** Handles `SIGTERM` / `SIGINT`, sends `SIGTERM` to all supervised process groups, removes `/var/run/watchdog.pid`, and exits cleanly.
- **Single-Instance Guard:** Prevents duplicate daemon instances using `/var/run/watchdog.pid` and `kill(pid, 0)` liveness checks.

---

## Project Structure

```text
.
├── Makefile
├── watchdog.c          # Daemon entry point, PID lock, signal setup, main waitpid loop
├── include/
│   ├── config.h        # Configuration structs and constants (MAX_PROCESSES, CONFIG_FILE)
│   ├── daemon.h        # Daemonization interface
│   ├── logger.h        # Logging interface and LOG_FILE path
│   └── process.h       # Process lifecycle management interface
├── src/
│   ├── config.c        # Config parser and default template generator
│   ├── daemon.c        # Double-fork background daemonization
│   ├── logger.c        # Timestamped file logger
│   └── process.c       # fork/exec, setrlimit, SIGHUP reload, death handling, shutdown
└── tests/
    ├── CPU_test.c      # CPU stress test binary for RLIMIT_CPU verification
    ├── RAM_test.c      # Memory allocation test binary for RLIMIT_AS verification
    ├── luzny_test.sh   # Helper test script
    └── test_petla.sh   # Helper loop test script
```

---

## Requirements

- Linux operating system (tested on Ubuntu)
- `gcc` compiler and `make`
- `root` privileges only when using default system paths (`/etc/`, `/var/run/`, `/var/log/`) — passing custom paths via CLI flags works without `root`

---

## 1. Building

```bash
make
```

This builds:

- `build/watchdog` – main supervisor daemon
- `build/tests/CPU_test` – test binary that saturates CPU usage
- `build/tests/RAM_test` – test binary that continuously allocates RAM

To remove build artifacts:

```bash
make clean
```

---

## 2. Configuration

The configuration file is located at `/etc/watchdog/watchdog.conf`.

On the **first run**, if the file does not exist, the daemon automatically creates `/etc/watchdog/` and generates a default template, then exits with instructions. Edit the file before running the daemon again:

```bash
sudo nano /etc/watchdog/watchdog.conf
```

### File Format

Each non-comment line describes one supervised process (supporting optional CLI arguments and single/double quotes):

```text
# Lines starting with '#' are comments
# Format: <Name>  <CPU_sec>  <RAM_MB>  <Path> [Args...]

ProcessName  30  50  /absolute/path/to/executable --port 8080
```

| Field     | Description                                       |
| --------- | ------------------------------------------------- |
| `Name`    | Process label used in logs (no spaces, max 63 ch) |
| `CPU_sec` | CPU time limit in seconds (`0` = no limit)        |
| `RAM_MB`  | Virtual memory limit in MB (`0` = no limit)       |
| `Path`    | Absolute path to the executable binary            |
| `Args`    | Optional CLI arguments (supports `'...'` / `"..."`) |

### Example Configuration

```text
# Name       CPU_sec  RAM_MB  Path [Args...]
CPU_test     20       100     /home/user/watchdog-SO/build/tests/CPU_test
RAM_test     50       30      /home/user/watchdog-SO/build/tests/RAM_test
worker       0        128     /usr/bin/python3 /opt/worker.py --interval 5
shell_loop   0        0       /bin/sh -c "while true; do sleep 1; done"
```

> **Note:** Paths must be absolute and target executables must have execute permissions (`chmod +x`).

---

## 3. Running & CLI Options

Default background daemon mode (uses `/etc/watchdog/watchdog.conf`, `/var/log/watchdog.log`, `/var/run/watchdog.pid`):

```bash
sudo ./build/watchdog
```

### CLI Options

| Flag | Description | Default |
| ---- | ----------- | ------- |
| `-c, --config <path>` | Configuration file path | `/etc/watchdog/watchdog.conf` |
| `-l, --log <path>` | Log file path | `/var/log/watchdog.log` |
| `-p, --pid <path>` | PID lock file path | `/var/run/watchdog.pid` |
| `-f, --foreground` | Run in foreground (no daemonize; mirror logs to `stderr`) | off |
| `-h, --help` | Display usage help and exit | — |
| `-v, --version` | Display version and exit | — |

### Running Without `root` (Foreground Mode)

```bash
./build/watchdog --foreground -c ./watchdog.conf -l ./watchdog.log -p ./watchdog.pid
```

When running as a background daemon, you can follow logs in real time:

```bash
sudo tail -f /var/log/watchdog.log
```

---

## 4. Logs

Log file path: `/var/log/watchdog.log`

Entries are formatted with timestamps as `[YYYY-MM-DD HH:MM:SS] <message>`.

---

## 5. Checking Status

The daemon writes its PID to `/var/run/watchdog.pid`:

```bash
cat /var/run/watchdog.pid
```

Check whether the daemon is running:

```bash
ps aux | grep watchdog
```

---

## 6. Signal Control & Hot-Reload

Check the daemon PID:

```bash
cat /var/run/watchdog.pid
```

| Action                        | Command                    |
| ----------------------------- | -------------------------- |
| Reload configuration (hot)    | `sudo kill -SIGHUP <PID>`  |
| Stop daemon gracefully        | `sudo kill -SIGTERM <PID>` |

### Hot-Reload (Updating Config Without Downtime)

1. Edit the configuration file:
   ```bash
   sudo nano /etc/watchdog/watchdog.conf
   ```
2. Send `SIGHUP` to the daemon:
   ```bash
   sudo kill -SIGHUP <PID>
   ```
3. The daemon diffs the new configuration against currently supervised processes:
   - **Unchanged** processes stay running without interruption.
   - **Modified or removed** processes are stopped via `SIGTERM` sent to their process group.
   - **New or modified** processes are started with the updated configuration.

---

## 7. Stopping the Daemon

```bash
sudo kill -SIGTERM <PID>
```

The daemon sends `SIGTERM` to all supervised process groups, logs shutdown, removes `/var/run/watchdog.pid`, reaps remaining child processes, and exits.

---

## 8. Test Programs

Two test programs are included in `tests/` to demonstrate kernel resource-limit enforcement and automatic restarts. After running `make`, binaries are placed in `build/tests/`.

### `CPU_test`

Simulates a process consuming 100% CPU in an infinite computation loop to verify `RLIMIT_CPU`.

Once the CPU limit is reached, the kernel sends `SIGXCPU`; the daemon logs the reason and restarts the process.

**Config entry** (`/etc/watchdog/watchdog.conf`):

```text
# Name     CPU_sec  RAM_MB  Path
CPU_test   20       20      /path/to/project/build/tests/CPU_test
```

### `RAM_test`

Simulates a memory leak by allocating 5 MB per second to verify `RLIMIT_AS`.

Once the address-space limit is reached, `malloc()` returns `NULL` and the test dereferences it to trigger `SIGSEGV`. The daemon detects the termination, logs it, and restarts the process.

**Config entry** (`/etc/watchdog/watchdog.conf`):

```text
# Name     CPU_sec  RAM_MB  Path
RAM_test   30       30      /path/to/project/build/tests/RAM_test
```

### Running Both Test Programs Together

```text
CPU_test   20   20    /home/user/watchdog-SO/build/tests/CPU_test
RAM_test   30   30    /home/user/watchdog-SO/build/tests/RAM_test
```

Watch `/var/log/watchdog.log`:

```bash
sudo tail -f /var/log/watchdog.log
```

You should see `[RESTART]` entries showing the detected termination cause (`SIGXCPU` for `CPU_test`, `SIGSEGV` for `RAM_test`).

---

## 9. Single-Instance Protection

Attempting to start a second daemon instance while another is active prints:

```text
Watchdog już działa (PID: 1234)!
```

and exits immediately with a non-zero status.

---

## License

This project is licensed under the MIT License – see the [LICENSE](LICENSE) file for details.
