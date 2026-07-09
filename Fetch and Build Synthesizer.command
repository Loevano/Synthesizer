#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

"$SCRIPT_DIR/scripts/fetch-and-build-app.sh"

echo
echo "Build complete. Closing this Terminal window..."
osascript -e 'tell application "Terminal" to close front window' >/dev/null 2>&1 &
