#!/bin/bash
# Test script for Vulkan layer capture with vkcube

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build/debug"

# Check if build exists
if [[ ! -f "${BUILD_DIR}/src/capture/goggles_vklayer.so" ]]; then
    echo "Error: Layer not built. Run 'cmake --preset=debug && cmake --build build/debug' first."
    exit 1
fi

if [[ ! -f "${BUILD_DIR}/src/app/goggles" ]]; then
    echo "Error: App not built. Run 'cmake --preset=debug && cmake --build build/debug' first."
    exit 1
fi

# Check if vkcube is available
if ! command -v vkcube &> /dev/null; then
    echo "Error: vkcube not found. Install vulkan-tools."
    exit 1
fi

echo "=== Starting Goggles App ==="
echo "The app will listen for captured frames..."
echo ""

# Start Goggles app in background
cd "${PROJECT_DIR}"
"${BUILD_DIR}/src/app/goggles" &
GOGGLES_PID=$!

# Give the app time to start and bind socket
sleep 2

if ! kill -0 $GOGGLES_PID 2>/dev/null; then
    echo "Error: Goggles app failed to start"
    exit 1
fi

echo "=== Starting vkcube with capture layer ==="
echo "Press Ctrl+C to stop"
echo ""

# Set up layer environment
export VK_LAYER_PATH="${BUILD_DIR}"
export VK_INSTANCE_LAYERS="VK_LAYER_goggles_capture"
export GOGGLES_CAPTURE=1

# Run vkcube
vkcube --width 800 --height 600

# Cleanup
echo ""
echo "=== Stopping Goggles App ==="
kill $GOGGLES_PID 2>/dev/null || true
wait $GOGGLES_PID 2>/dev/null || true

echo "Done."
