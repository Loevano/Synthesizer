#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REMOTE="${REMOTE:-origin}"
APP_DEST="$ROOT_DIR/Synthesizer.app"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "This script builds a clickable macOS .app and must be run on macOS." >&2
  exit 1
fi

cd "$ROOT_DIR"

if [[ ! -d .git ]]; then
  echo "Expected a Git checkout at $ROOT_DIR." >&2
  exit 1
fi

current_branch="$(git branch --show-current)"
if [[ -z "$current_branch" ]]; then
  echo "This checkout is not on a branch. Check out a branch before fetching and building." >&2
  exit 1
fi

BRANCH="${BRANCH:-$current_branch}"

if [[ -n "$(git status --porcelain)" ]]; then
  echo "There are local changes in this checkout." >&2
  echo "Commit, stash, or discard them before fetching and building." >&2
  git status --short
  exit 1
fi

echo "Fetching $REMOTE/$BRANCH..."
git fetch "$REMOTE" "$BRANCH"

if [[ "$current_branch" != "$BRANCH" ]]; then
  echo "Switching from $current_branch to $BRANCH..."
  git checkout "$BRANCH"
fi

echo "Updating $BRANCH..."
git merge --ff-only "$REMOTE/$BRANCH"

echo "Building Synthesizer.app..."
"$ROOT_DIR/scripts/build-app.sh"

echo
echo "Done."
echo "Built app: $APP_DEST"
