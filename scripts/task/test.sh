#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(dirname "$0")"
source "$SCRIPT_DIR/parse-preset.sh" "$@"

# Build first
"$SCRIPT_DIR/build.sh" -p "$PRESET"

# Run tests
ctest --test-dir "build/$PRESET" --output-on-failure
