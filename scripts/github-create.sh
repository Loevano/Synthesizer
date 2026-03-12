#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 2 ]]; then
  echo "Usage: $0 <github-username> <repo-name>"
  exit 1
fi

USERNAME="$1"
REPO="$2"

if ! command -v gh >/dev/null 2>&1; then
  echo "GitHub CLI (gh) is not installed."
  exit 1
fi

if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  git init
fi

gh repo create "${USERNAME}/${REPO}" --private --source . --remote origin --push
