#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/parse-preset.sh" "$@"
mkdir -p "$CCACHE_TEMPDIR"
cmake --preset "${PRESET}-i686"
cmake --build --preset "${PRESET}-i686"
