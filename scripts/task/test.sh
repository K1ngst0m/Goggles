#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(dirname "$0")"
source "$SCRIPT_DIR/parse-preset.sh" "$@"

# Build first
"$SCRIPT_DIR/build.sh" -p "$PRESET"

# By default, skip visual tests in generic test runs (CI-safe fast path).
# Set GOGGLES_INCLUDE_VISUAL_TESTS=1 to include visual tests explicitly.
CTEST_LABEL_EXCLUDE_ARGS=(--label-exclude visual)
if [[ "${GOGGLES_INCLUDE_VISUAL_TESTS:-0}" == "1" ]]; then
  CTEST_LABEL_EXCLUDE_ARGS=()
fi

# Run tests
ctest --preset "$PRESET" --output-on-failure "${CTEST_LABEL_EXCLUDE_ARGS[@]}"

# Validate installed downstream consumers against the packaged standalone library.
"$SCRIPT_DIR/validate-installed-consumers.sh" -p "$PRESET"
