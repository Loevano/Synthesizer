#!/usr/bin/env bash
set -euo pipefail

MODE="${1:-pre-commit}"
MESSAGE_FILE="${2:-}"

ROOT_DIR="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$ROOT_DIR"

MAX_STAGED_FILES="${SYNTH_MAX_STAGED_FILES:-25}"
MAX_STAGED_CHURN="${SYNTH_MAX_STAGED_CHURN:-1200}"

fail() {
  echo "git rule failed: $*" >&2
  exit 1
}

notice() {
  echo "git rule: $*" >&2
}

current_branch() {
  git symbolic-ref --quiet --short HEAD 2>/dev/null || true
}

is_allowed_branch_name() {
  local branch="$1"
  case "$branch" in
    feature/*|feat/*|fix/*|docs/*|ui/*|test/*|chore/*|infra/*|core/*|dsp/*|routing/*|architecture/*|hotfix/*|release/*)
      return 0
      ;;
  esac
  return 1
}

is_core_change_branch() {
  local branch="$1"
  case "$branch" in
    core/*|dsp/*|routing/*|architecture/*|infra/*|hotfix/*|fix/core-*|fix/dsp-*|fix/routing-*)
      return 0
      ;;
  esac
  return 1
}

is_protected_path() {
  local path="$1"
  case "$path" in
    CMakeLists.txt|.github/workflows/*|src/audio/*|include/synth/audio/*|src/dsp/*|include/synth/dsp/*|src/graph/*|include/synth/graph/*|src/app/SynthHost.cpp|include/synth/app/SynthHost.hpp|include/synth/app/RealtimeCommands.hpp)
      return 0
      ;;
  esac
  return 1
}

check_branch_for_local_work() {
  local branch
  branch="$(current_branch)"

  [[ -n "$branch" ]] || fail "detached HEAD is not allowed for normal local work"

  case "$branch" in
    main|dev)
      fail "do not commit directly on '$branch'; create a short-lived branch from dev"
      ;;
  esac

  if ! is_allowed_branch_name "$branch"; then
    fail "branch '$branch' must use an approved prefix; see docs/GIT_RULES.md"
  fi
}

check_commit_message() {
  [[ -n "$MESSAGE_FILE" ]] || fail "commit message file was not provided"
  [[ -f "$MESSAGE_FILE" ]] || fail "commit message file does not exist: $MESSAGE_FILE"

  local subject
  subject="$(sed -n '1p' "$MESSAGE_FILE")"
  subject="${subject#"${subject%%[![:space:]]*}"}"
  subject="${subject%"${subject##*[![:space:]]}"}"

  [[ -n "$subject" ]] || fail "commit subject must not be empty"

  if [[ "${SYNTH_ALLOW_WEAK_COMMIT_MSG:-}" != "1" ]]; then
    ((${#subject} >= 8)) || fail "commit subject is too short"
    ((${#subject} <= 72)) || fail "commit subject is too long; keep the first line at 72 characters or less"

    local lowered
    lowered="$(printf '%s' "$subject" | tr '[:upper:]' '[:lower:]')"
    case "$lowered" in
      misc|misc.*|changes|changes.*|update|update.*|update\ stuff|wip|wip.*|stuff|stuff.*)
        fail "commit subject is too vague: '$subject'"
        ;;
    esac
  fi

  notice "commit message passed"
}

check_pre_commit() {
  check_branch_for_local_work

  local branch
  branch="$(current_branch)"

  local staged_files=()
  while IFS= read -r path; do
    [[ -n "$path" ]] && staged_files+=("$path")
  done < <(git diff --cached --name-only --diff-filter=ACMRTD)

  ((${#staged_files[@]} > 0)) || fail "no staged files; stage only the files that belong to this change"

  if ((${#staged_files[@]} > MAX_STAGED_FILES)) && [[ "${SYNTH_ALLOW_LARGE_CHANGE:-}" != "1" ]]; then
    fail "too many staged files (${#staged_files[@]} > $MAX_STAGED_FILES); split the change or set SYNTH_ALLOW_LARGE_CHANGE=1"
  fi

  local churn=0
  local added deleted path
  while read -r added deleted path; do
    [[ -n "${path:-}" ]] || continue
    [[ "$added" == "-" ]] && continue
    [[ "$deleted" == "-" ]] && continue
    churn=$((churn + added + deleted))
  done < <(git diff --cached --numstat)

  if ((churn > MAX_STAGED_CHURN)) && [[ "${SYNTH_ALLOW_LARGE_CHANGE:-}" != "1" ]]; then
    fail "staged churn is too large ($churn > $MAX_STAGED_CHURN lines); split the change or set SYNTH_ALLOW_LARGE_CHANGE=1"
  fi

  local protected_paths=()
  for path in "${staged_files[@]}"; do
    if is_protected_path "$path"; then
      protected_paths+=("$path")
    fi
  done

  if ((${#protected_paths[@]} > 0)) && [[ "${SYNTH_ALLOW_CORE_CHANGE:-}" != "1" ]] && ! is_core_change_branch "$branch"; then
    {
      echo "git rule failed: protected core paths changed on branch '$branch':"
      printf '  - %s\n' "${protected_paths[@]}"
      echo "Use a core/dsp/routing/architecture/infra/hotfix branch, or set SYNTH_ALLOW_CORE_CHANGE=1 for an intentional exception."
      echo "See docs/GIT_RULES.md."
    } >&2
    exit 1
  fi

  notice "pre-commit rules passed"
}

check_pre_push() {
  local branch
  branch="$(current_branch)"

  [[ -n "$branch" ]] || fail "detached HEAD is not allowed for normal pushes"
  is_allowed_branch_name "$branch" || {
    case "$branch" in
      main|dev) ;;
      *) fail "branch '$branch' must use an approved prefix before push" ;;
    esac
  }

  local local_ref local_sha remote_ref remote_sha
  while read -r local_ref local_sha remote_ref remote_sha; do
    [[ -n "${remote_ref:-}" ]] || continue
    case "$remote_ref" in
      refs/heads/main|refs/heads/dev)
        if [[ "${SYNTH_ALLOW_PROTECTED_PUSH:-}" != "1" ]]; then
          fail "direct pushes to ${remote_ref#refs/heads/} are blocked; use PRs"
        fi
        ;;
    esac
  done

  notice "pre-push rules passed"
}

case "$MODE" in
  pre-commit)
    check_pre_commit
    ;;
  commit-msg)
    check_commit_message
    ;;
  pre-push)
    check_pre_push
    ;;
  manual)
    check_pre_commit
    ;;
  *)
    fail "unknown mode '$MODE'"
    ;;
esac
