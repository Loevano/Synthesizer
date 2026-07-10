#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/builds/debug}"

"$ROOT_DIR/scripts/build.sh"
"$BUILD_DIR/synth_host" "$@"
