#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

"$SCRIPT_DIR/scripts/fetch-and-build-app.sh"

echo
read -r -p "Press Return to close this window..."
