#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(dirname "$0")"
source "$SCRIPT_DIR/parse-preset.sh" "$@"

# Build both architectures
"$SCRIPT_DIR/build.sh" -p "$PRESET"
"$SCRIPT_DIR/build-i686.sh" -p "$PRESET"

# Install manifests
mkdir -p "$HOME/.local/share/vulkan/implicit_layer.d"
cp "build/$PRESET/share/vulkan/implicit_layer.d/"*.json "$HOME/.local/share/vulkan/implicit_layer.d/"
cp "build/${PRESET}-i686/share/vulkan/implicit_layer.d/"*.json "$HOME/.local/share/vulkan/implicit_layer.d/"
echo "Manifests installed"
