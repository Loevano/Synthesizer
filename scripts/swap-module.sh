#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <module-target>"
  echo "Example: $0 highpass_module"
  exit 1
fi

TARGET="$1"

cmake --build build --target "${TARGET}"

echo "Rebuilt ${TARGET}. If host is running with that module file, it will auto-reload."
