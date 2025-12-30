#!/usr/bin/env bash
# Install the pre-commit formatting hook (idempotent).
# Used by: pixi run init

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOK_SRC="${PROJECT_ROOT}/scripts/pre-commit-format.sh"

if ! git -C "$PROJECT_ROOT" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
  echo "[pre-commit] Not a git checkout; skipping hook install."
  exit 0
fi

HOOKS_DIR=""
HOOKS_PATH="$(git -C "$PROJECT_ROOT" config --get core.hooksPath || true)"
if [[ -n "$HOOKS_PATH" ]]; then
  if [[ "$HOOKS_PATH" = /* ]]; then
    HOOKS_DIR="$HOOKS_PATH"
  else
    HOOKS_DIR="${PROJECT_ROOT}/${HOOKS_PATH}"
  fi
else
  HOOKS_DIR_REL="$(git -C "$PROJECT_ROOT" rev-parse --git-path hooks)"
  if [[ "$HOOKS_DIR_REL" = /* ]]; then
    HOOKS_DIR="$HOOKS_DIR_REL"
  else
    HOOKS_DIR="${PROJECT_ROOT}/${HOOKS_DIR_REL}"
  fi
fi

HOOK_DST="${HOOKS_DIR}/pre-commit"

if [[ -e "$HOOK_DST" && ! -L "$HOOK_DST" ]]; then
  echo "[pre-commit] Existing hook not managed; leave it unchanged: $HOOK_DST"
  exit 0
fi

mkdir -p "$HOOKS_DIR"
ln -sf "$HOOK_SRC" "$HOOK_DST"
chmod +x "$HOOK_SRC" "$HOOK_DST"
echo "[pre-commit] Installed hook -> $HOOK_DST"
