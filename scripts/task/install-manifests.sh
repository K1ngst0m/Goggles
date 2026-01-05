#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$(dirname "$0")"
source "$SCRIPT_DIR/parse-preset.sh" "$@"

# Ensure both architectures are built
"$SCRIPT_DIR/build.sh" -p "$PRESET"
"$SCRIPT_DIR/build-i686.sh" -p "$PRESET"

# Install manifests (handle case where no .json files exist)
mkdir -p "$HOME/.local/share/vulkan/implicit_layer.d"
shopt -s nullglob
cp "build/$PRESET/share/vulkan/implicit_layer.d/"*.json "$HOME/.local/share/vulkan/implicit_layer.d/" 2>/dev/null || true
cp "build/${PRESET}-i686/share/vulkan/implicit_layer.d/"*.json "$HOME/.local/share/vulkan/implicit_layer.d/" 2>/dev/null || true
shopt -u nullglob
echo "Manifests installed"
