#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

rm -rf \
  "$ROOT_DIR/builds" \
  "$ROOT_DIR/build" \
  "$ROOT_DIR/build-app" \
  "$ROOT_DIR/build-release" \
  "$ROOT_DIR/build-universal-release" \
  "$ROOT_DIR/dist" \
  "$ROOT_DIR/Synthesizer.app" \
  "$ROOT_DIR/Synthesizer.zip"

find "$ROOT_DIR" -type f -name '.DS_Store' -delete
