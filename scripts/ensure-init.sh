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

if ! git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo -e "\033[31m[ERROR] Not a git checkout.\033[0m" >&2
  exit 1
fi

HOOKS_PATH="$(git -C "$PROJECT_ROOT" config --get core.hooksPath || true)"
if [[ -n "$HOOKS_PATH" ]]; then
  if [[ "$HOOKS_PATH" = /* ]]; then
    HOOK="$HOOKS_PATH/pre-commit"
  else
    HOOK="$PROJECT_ROOT/$HOOKS_PATH/pre-commit"
  fi
else
  HOOK="$(git -C "$PROJECT_ROOT" rev-parse --git-path hooks)/pre-commit"
fi

if [[ ! -f "$HOOK" ]]; then
  echo -e "\033[31m[ERROR] Pre-commit hook not installed. Run: pixi run init\033[0m" >&2
  exit 1
fi
