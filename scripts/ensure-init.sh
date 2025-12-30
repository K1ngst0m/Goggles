#!/bin/bash
# Fail if project not initialized (pre-commit hook missing)
# Used as dependency for main tasks (dev, build)
# Skipped in CI environments (CI=true)

# Skip in CI - no pre-commit hook needed
if [[ "${CI:-}" == "true" ]]; then
  exit 0
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Handle git worktrees
if [[ -f "$PROJECT_ROOT/.git" ]]; then
  GIT_DIR=$(cat "$PROJECT_ROOT/.git" | sed 's/gitdir: //')
else
  GIT_DIR="$PROJECT_ROOT/.git"
fi

HOOK="$GIT_DIR/hooks/pre-commit"

if [[ ! -f "$HOOK" ]]; then
  echo -e "\033[31m[ERROR] Pre-commit hook not installed. Run: pixi run init\033[0m" >&2
  exit 1
fi
