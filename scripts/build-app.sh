#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-app}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
ARCHS="${ARCHS:-arm64;x86_64}"
SIGN_IDENTITY="${SIGN_IDENTITY:--}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "The clickable .app bundle can only be built on macOS." >&2
  exit 1
fi

cd "$ROOT_DIR"

cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -DCMAKE_OSX_ARCHITECTURES="$ARCHS"

cmake --build "$BUILD_DIR" --target Synthesizer -j

APP_PATH="$BUILD_DIR/Synthesizer.app"
if [[ ! -d "$APP_PATH" ]]; then
  echo "Expected app bundle at $APP_PATH" >&2
  exit 1
fi

if [[ "${SIGN_APP:-1}" != "0" ]]; then
  codesign --force --deep --sign "$SIGN_IDENTITY" "$APP_PATH"
  codesign --verify --deep --strict --verbose=2 "$APP_PATH"
fi

echo "Built $APP_PATH"
echo "Open it with: open \"$APP_PATH\""
