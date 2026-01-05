#!/usr/bin/env bash
set -euo pipefail
source "$(dirname "$0")/parse-preset.sh" "$@"
rm -rf "build/$PRESET" "build/${PRESET}-i686"
echo "Removed build/$PRESET and build/${PRESET}-i686"
