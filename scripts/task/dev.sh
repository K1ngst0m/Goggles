#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(dirname "$0")"
source "$SCRIPT_DIR/parse-preset.sh" "$@"

# Build full dev environment (both archs + manifests)
"$SCRIPT_DIR/install-manifests.sh" -p "$PRESET"
