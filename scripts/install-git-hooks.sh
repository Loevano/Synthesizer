#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

git -C "$ROOT_DIR" config core.hooksPath .githooks
chmod +x "$ROOT_DIR/.githooks/pre-commit" \
  "$ROOT_DIR/.githooks/commit-msg" \
  "$ROOT_DIR/.githooks/pre-push" \
  "$ROOT_DIR/scripts/check-git-rules.sh"

echo "Installed Synthesizer git hooks from .githooks"
