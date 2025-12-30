#!/usr/bin/env bash
# Pre-commit hook: format staged files only (C/C++ via clang-format, TOML via taplo).
# Install: ln -s ../../scripts/pre-commit-format.sh .git/hooks/pre-commit

set -euo pipefail

SCRIPT_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
ROOT_DIR="$(cd "$(dirname "$SCRIPT_PATH")/.." && pwd)"
cd "$ROOT_DIR"

# Use pixi run for formatting tools
run_clang_format() { pixi run -q clang-format "$@"; }
run_taplo() { pixi run -q taplo "$@"; }

format_cpp() {
    mapfile -d '' -t staged_cpp < <(git diff --cached --name-only -z --diff-filter=d | grep -zE '\.(c|cc|cpp|cxx|h|hh|hpp|hxx)$' || true)
    if ((${#staged_cpp[@]} == 0)); then
        return
    fi

    echo "[pre-commit] Formatting C/C++ staged files (${#staged_cpp[@]})"
    run_clang_format -i "${staged_cpp[@]}"

    if ! git diff --quiet --exit-code -- "${staged_cpp[@]}"; then
        git add -- "${staged_cpp[@]}"
        echo "[pre-commit] Re-staged formatted C/C++ files."
    fi
}

format_toml() {
    mapfile -d '' -t staged_toml < <(git diff --cached --name-only -z --diff-filter=d | grep -zE '\.toml$' || true)
    if ((${#staged_toml[@]} == 0)); then
        return
    fi

    echo "[pre-commit] Formatting TOML staged files (${#staged_toml[@]})"
    run_taplo fmt "${staged_toml[@]}"

    if ! git diff --quiet --exit-code -- "${staged_toml[@]}"; then
        git add -- "${staged_toml[@]}"
        echo "[pre-commit] Re-staged formatted TOML files."
    fi
}

format_cpp
format_toml

echo "[pre-commit] Formatting clean."
