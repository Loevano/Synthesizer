#!/usr/bin/env bash
set -euo pipefail

APP_BUNDLE="./build/Synthesizer.app"
APP_EXECUTABLE="$APP_BUNDLE/Contents/MacOS/Synthesizer"
APP_BUNDLE_ID="com.loevano.synthesizer"

APP_ARGS=()
while (($# > 0)); do
  case "$1" in
    --debug-crash)
      APP_ARGS+=("$1")
      shift
      ;;
    --debug-bridge|--debug-robin)
      APP_ARGS+=("$1")
      shift
      ;;
    --)
      shift
      break
      ;;
    *)
      break
      ;;
  esac
done

./scripts/build.sh

if (($# > 0)); then
  APP_ARGS+=("$@")
fi

list_app_pids() {
  pgrep -f "$APP_EXECUTABLE" || true
}

known_pid() {
  local candidate="$1"
  local pid
  for pid in "${EXISTING_APP_PIDS[@]:-}"; do
    if [[ "$pid" == "$candidate" ]]; then
      return 0
    fi
  done
  return 1
}

capture_launched_app_pid() {
  local candidate
  while IFS= read -r candidate; do
    [[ -n "$candidate" ]] || continue
    if ! known_pid "$candidate"; then
      APP_PID="$candidate"
      return 0
    fi
  done < <(list_app_pids)
  return 1
}

request_app_quit() {
  osascript -e "tell application id \"$APP_BUNDLE_ID\" to quit" >/dev/null 2>&1 || true

  if [[ -n "${APP_PID:-}" ]]; then
    local attempt
    for attempt in {1..20}; do
      if ! kill -0 "$APP_PID" 2>/dev/null; then
        return 0
      fi
      sleep 0.1
    done

    kill -TERM "$APP_PID" 2>/dev/null || true
  fi
}

handle_interrupt() {
  request_app_quit
  wait "$OPEN_PID" 2>/dev/null || true
  exit 130
}

EXISTING_APP_PIDS=()
while IFS= read -r existing_pid; do
  [[ -n "$existing_pid" ]] || continue
  EXISTING_APP_PIDS+=("$existing_pid")
done < <(list_app_pids)

if ((${#APP_ARGS[@]} > 0)); then
  open -W -n "$APP_BUNDLE" --args "${APP_ARGS[@]}" &
else
  open -W -n "$APP_BUNDLE" &
fi

OPEN_PID=$!
APP_PID=""

trap handle_interrupt INT TERM

for _ in {1..50}; do
  if capture_launched_app_pid; then
    break
  fi

  if ! kill -0 "$OPEN_PID" 2>/dev/null; then
    break
  fi

  sleep 0.1
done

wait "$OPEN_PID"
