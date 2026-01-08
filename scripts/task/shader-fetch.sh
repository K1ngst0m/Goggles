#!/bin/bash
# Script to download RetroArch slang-shaders collection via git clone

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
SHADERS_DIR="${PROJECT_DIR}/shaders/retroarch"
REPO_URL="https://github.com/libretro/slang-shaders.git"

echo "=== Downloading RetroArch shaders ==="
echo "Source: ${REPO_URL}"
echo "Destination: ${SHADERS_DIR}"
echo ""

if [[ -d "${SHADERS_DIR}/.git" ]]; then
    echo "Updating existing clone..."
    if git -C "${SHADERS_DIR}" status --porcelain | grep -Eq '^[^ D?]|^ [^ D?]'; then
        echo "Warning: Local modifications will be discarded" >&2
    fi
    git -C "${SHADERS_DIR}" reset --hard
    if ! git -C "${SHADERS_DIR}" pull --ff-only; then
        echo "Error: Failed to update existing repository" >&2
        exit 1
    fi
else
    if [[ -z "${SHADERS_DIR}" ]] || [[ ! "${SHADERS_DIR}" =~ shaders/retroarch$ ]]; then
        echo "Error: SHADERS_DIR is not set correctly: '${SHADERS_DIR}'" >&2
        exit 1
    fi
    rm -rf "${SHADERS_DIR}"
    if ! git clone --depth 1 "${REPO_URL}" "${SHADERS_DIR}"; then
        echo "Error: Failed to clone repository" >&2
        exit 1
    fi
fi

echo ""
echo "=== Verifying installation ==="

# Check for key directories that should exist
REQUIRED_DIRS=("crt" "presets" "include")
for dir in "${REQUIRED_DIRS[@]}"; do
    if [[ ! -d "${SHADERS_DIR}/${dir}" ]]; then
        echo "Error: Expected directory '${dir}' not found. Installation may be incomplete."
        exit 1
    fi
done

# Count shader files
SHADER_COUNT=$(find "${SHADERS_DIR}" \( -name "*.slang" -o -name "*.slangp" \) | wc -l)
echo "Installed ${SHADER_COUNT} shader files"

COMMIT=$(git -C "${SHADERS_DIR}" rev-parse --short HEAD)
echo ""
echo "=== Complete ==="
echo "slang-shaders: ${COMMIT}"
echo "Location: ${SHADERS_DIR}"
