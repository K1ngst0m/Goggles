#!/usr/bin/env bash
# Install the pre-commit formatting hook (idempotent).
# Used by: pixi run init

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOK_SRC="${PROJECT_ROOT}/scripts/pre-commit-format.sh"
HOOK_DST="${PROJECT_ROOT}/.git/hooks/pre-commit"

if [[ ! -d "${PROJECT_ROOT}/.git" ]]; then
    echo "[pre-commit] Not a git checkout; skipping hook install."
    exit 0
fi

if [[ -e "$HOOK_DST" && ! -L "$HOOK_DST" ]]; then
    echo "[pre-commit] Existing .git/hooks/pre-commit not managed; leave it unchanged."
    exit 0
fi

mkdir -p "$(dirname "$HOOK_DST")"
ln -sf "$HOOK_SRC" "$HOOK_DST"
chmod +x "$HOOK_SRC" "$HOOK_DST"
echo "[pre-commit] Installed hook -> $HOOK_DST"
