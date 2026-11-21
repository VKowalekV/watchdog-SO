#!/usr/bin/env bash
set -euo pipefail

WATCHDOG_BIN="./build/watchdog"
TMP_DIR="$(mktemp -d)"
CONF_FILE="$TMP_DIR/watchdog.conf"
LOG_FILE="$TMP_DIR/watchdog.log"
PID_FILE="$TMP_DIR/watchdog.pid"
STDERR_LOG="$TMP_DIR/watchdog.stderr.log"
WD_PID=""
STEADY_PID=""
ADDED_PID=""

cleanup() {
  local exit_code=$?
  if [[ -n "$WD_PID" ]] && kill -0 "$WD_PID" 2>/dev/null; then
    kill -TERM "$WD_PID" 2>/dev/null || true
    wait "$WD_PID" 2>/dev/null || true
  fi
  if [[ -n "$STEADY_PID" ]] && kill -0 "$STEADY_PID" 2>/dev/null; then
    kill -KILL "$STEADY_PID" 2>/dev/null || true
  fi
  if [[ -n "$ADDED_PID" ]] && kill -0 "$ADDED_PID" 2>/dev/null; then
    kill -KILL "$ADDED_PID" 2>/dev/null || true
  fi
  if [[ $exit_code -ne 0 ]]; then
    echo "=== Integration test failed (exit $exit_code) ===" >&2
    if [[ -f "$LOG_FILE" ]]; then
      echo "--- watchdog.log ---" >&2
      cat "$LOG_FILE" >&2
    fi
    if [[ -f "$STDERR_LOG" ]]; then
      echo "--- watchdog.stderr.log ---" >&2
      cat "$STDERR_LOG" >&2
    fi
  fi
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

wait_for_log() {
  local pattern="$1"
  local timeout_iters="${2:-50}"
  for _ in $(seq 1 "$timeout_iters"); do
    if [[ -f "$LOG_FILE" ]] && grep -qE "$pattern" "$LOG_FILE"; then
      return 0
    fi
    sleep 0.1
  done
  echo "Timed out waiting for log pattern: $pattern" >&2
  return 1
}

echo "[1/7] Checking CLI flags (--version, --help, invalid flag)..."
VERSION_OUT="$("$WATCHDOG_BIN" --version)"
echo "$VERSION_OUT" | grep -q "watchdog "

HELP_OUT="$("$WATCHDOG_BIN" --help)"
echo "$HELP_OUT" | grep -q -- "--foreground"

if "$WATCHDOG_BIN" --no-such-flag >/dev/null 2>&1; then
  echo "Expected non-zero exit code for unknown flag" >&2
  exit 1
fi

echo "[2/7] Creating temporary configuration..."
cat > "$CONF_FILE" <<'EOF'
steady 0 0 /bin/sh -c "while true; do sleep 1; done"
crasher 0 0 /bin/sh -c "exit 42"
EOF

echo "[3/7] Starting watchdog in --foreground mode..."
"$WATCHDOG_BIN" --foreground -c "$CONF_FILE" -l "$LOG_FILE" -p "$PID_FILE" 2>"$STDERR_LOG" &
WD_PID=$!

for _ in $(seq 1 50); do
  if [[ -s "$PID_FILE" ]]; then
    break
  fi
  sleep 0.1
done

if [[ ! -s "$PID_FILE" ]]; then
  echo "PID file was not created: $PID_FILE" >&2
  exit 1
fi

RECORDED_PID="$(cat "$PID_FILE")"
if [[ "$RECORDED_PID" != "$WD_PID" ]]; then
  echo "PID mismatch: expected $WD_PID, got $RECORDED_PID" >&2
  exit 1
fi

echo "[4/7] Checking single-instance guard..."
if "$WATCHDOG_BIN" -f -c "$CONF_FILE" -l "$LOG_FILE" -p "$PID_FILE" >/dev/null 2>&1; then
  echo "Expected second watchdog instance to fail" >&2
  exit 1
fi

echo "[5/7] Waiting for process startup and crasher restart logs..."
wait_for_log "\[START\] Started 'steady'" 50
wait_for_log "\[START\] Started 'crasher'" 50
wait_for_log "\[RESTART\].*crasher.*exit code 42" 50

STEADY_PID="$(grep "\[START\] Started 'steady'" "$LOG_FILE" | head -n1 | sed -E 's/.*\(PID: ([0-9]+)\).*/\1/')"
if [[ -z "$STEADY_PID" ]] || ! kill -0 "$STEADY_PID" 2>/dev/null; then
  echo "Steady process is not running (PID: '$STEADY_PID')" >&2
  exit 1
fi

echo "[6/7] Testing SIGHUP hot-reload..."
cat > "$CONF_FILE" <<'EOF'
steady 0 0 /bin/sh -c "while true; do sleep 1; done"
added 0 0 /bin/sh -c "while true; do sleep 1; done"
EOF

kill -HUP "$WD_PID"

wait_for_log "\[RELOAD\] Removed process 'crasher'" 50
wait_for_log "\[START\] Started 'added'" 50
wait_for_log "\[SYSTEM\] Configuration reload completed successfully\." 50

ADDED_PID="$(grep "\[START\] Started 'added'" "$LOG_FILE" | head -n1 | sed -E 's/.*\(PID: ([0-9]+)\).*/\1/')"

if ! kill -0 "$STEADY_PID" 2>/dev/null; then
  echo "Expected unchanged 'steady' process (PID $STEADY_PID) to stay alive across SIGHUP" >&2
  exit 1
fi

STEADY_STARTS="$(grep -c "\[START\] Started 'steady'" "$LOG_FILE")"
if [[ "$STEADY_STARTS" -ne 1 ]]; then
  echo "Expected 'steady' to start exactly once, got $STEADY_STARTS" >&2
  exit 1
fi

echo "[7/7] Testing graceful SIGTERM shutdown..."
kill -TERM "$WD_PID"
wait "$WD_PID"
WD_STATUS=$?
WD_PID=""

if [[ "$WD_STATUS" -ne 0 ]]; then
  echo "Watchdog exited with non-zero status on SIGTERM: $WD_STATUS" >&2
  exit 1
fi

grep -q "\[SYSTEM\] Watchdog shut down cleanly\." "$LOG_FILE"

if [[ -e "$PID_FILE" ]]; then
  echo "Expected PID file to be removed on shutdown: $PID_FILE" >&2
  exit 1
fi

for _ in $(seq 1 20); do
  if ! kill -0 "$STEADY_PID" 2>/dev/null; then
    break
  fi
  sleep 0.1
done

if kill -0 "$STEADY_PID" 2>/dev/null; then
  echo "Expected 'steady' process ($STEADY_PID) to terminate on shutdown" >&2
  exit 1
fi

echo "All integration tests passed."
