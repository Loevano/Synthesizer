#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 \"commit message\" [path ...]"
  echo
  echo "Stage intentionally. Pass paths after the message, or stage files before running this helper."
  exit 1
fi

MESSAGE="$1"
shift

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

if [[ $# -gt 0 ]]; then
  git add -- "$@"
fi

if git diff --cached --quiet; then
  echo "No staged files. Stage only the files that belong to this change."
  exit 1
fi

"$ROOT_DIR/scripts/check-git-rules.sh" pre-commit
git commit -m "${MESSAGE}"
