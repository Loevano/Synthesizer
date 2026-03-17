#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-release}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
DIST_DIR="${DIST_DIR:-dist}"
VERSION_LABEL="${1:-}"

cd "$ROOT_DIR"

cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" -j

APP_PATH="$BUILD_DIR/Synthesizer.app"
if [[ ! -d "$APP_PATH" ]]; then
  echo "Expected app bundle at $APP_PATH" >&2
  exit 1
fi

mkdir -p "$DIST_DIR"

if [[ -z "$VERSION_LABEL" ]]; then
  VERSION_LABEL="$(git describe --tags --always --dirty 2>/dev/null || echo dev)"
fi

ARCH_LABEL="$(uname -m)"
ZIP_NAME="Synthesizer-${VERSION_LABEL}-macOS-${ARCH_LABEL}.zip"
ZIP_PATH="$DIST_DIR/$ZIP_NAME"

rm -f "$ZIP_PATH"
ditto -c -k --sequesterRsrc --keepParent "$APP_PATH" "$ZIP_PATH"

echo "Created $ZIP_PATH"
