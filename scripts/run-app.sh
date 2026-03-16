#!/usr/bin/env bash
set -euo pipefail

EXTRA_ENV=()
while (($# > 0)); do
  case "$1" in
    --debug-crash)
      EXTRA_ENV+=(SYNTH_DEBUG_CRASH=1 SYNTH_DEBUG_BRIDGE=1 SYNTH_DEBUG_ROBIN=1)
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
if ((${#EXTRA_ENV[@]} > 0)); then
  exec env "${EXTRA_ENV[@]}" "./build/Synthesizer.app/Contents/MacOS/Synthesizer" "$@"
fi

exec "./build/Synthesizer.app/Contents/MacOS/Synthesizer" "$@"
