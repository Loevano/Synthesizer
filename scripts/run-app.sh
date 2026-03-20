#!/usr/bin/env bash
set -euo pipefail

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

if ((${#APP_ARGS[@]} > 0)); then
  exec open -W -n "./build/Synthesizer.app" --args "${APP_ARGS[@]}"
fi

exec open -W -n "./build/Synthesizer.app"
