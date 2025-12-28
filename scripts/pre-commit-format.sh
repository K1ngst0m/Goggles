#!/usr/bin/env bash
# Pre-commit hook: run project formatter and block commit on failure/changes.
# Install: ln -s ../../scripts/pre-commit-format.sh .git/hooks/pre-commit

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

if ! command -v pixi >/dev/null 2>&1; then
    echo "[pre-commit] pixi not found; install pixi first." >&2
    exit 1
fi

echo "[pre-commit] Running formatter: pixi run format"
if ! pixi run format; then
    echo "[pre-commit] Formatting failed." >&2
    exit 1
fi

# If formatting changed files, stop the commit so user can review/stage.
if ! git diff --quiet --exit-code; then
    echo "[pre-commit] Formatting applied; please stage the changes and re-run commit." >&2
    exit 1
fi

echo "[pre-commit] Formatting clean."
