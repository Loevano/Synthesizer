#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILT_APP="$SCRIPT_DIR/builds/Synthesizer.app"
MAIN_APP="$SCRIPT_DIR/Synthesizer.app"

"$SCRIPT_DIR/scripts/fetch-and-build-app.sh"

if [[ ! -d "$BUILT_APP" ]]; then
  echo "Expected built app at $BUILT_APP" >&2
  exit 1
fi

echo "Copying app to main folder..."
ditto "$BUILT_APP" "$MAIN_APP"
echo "Copied app: $MAIN_APP"

echo
echo "Build complete. Closing this Terminal window..."
osascript -e 'tell application "Terminal" to close front window' >/dev/null 2>&1 &
