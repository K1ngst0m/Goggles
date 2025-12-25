#!/bin/bash
# Script to download and extract RetroArch slang-shaders collection

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SHADERS_DIR="${PROJECT_DIR}/shaders/retroarch"

# Check for download tool (prefer wget, fallback to curl)
if command -v wget &> /dev/null; then
    DOWNLOAD_CMD="wget -O"
elif command -v curl &> /dev/null; then
    DOWNLOAD_CMD="curl -L -o"
else
    echo "Error: Neither wget nor curl found. Install one to continue."
    exit 1
fi

# Check for unzip
if ! command -v unzip &> /dev/null; then
    echo "Error: unzip not found. Install unzip to continue."
    exit 1
fi

# Create temporary directory with cleanup trap
TEMP_DIR=$(mktemp -d -t goggles-shaders-XXXXXX)
trap 'rm -rf "${TEMP_DIR}"' EXIT ERR

ZIP_FILE="${TEMP_DIR}/slang-shaders.zip"
URL="https://github.com/libretro/slang-shaders/archive/refs/heads/master.zip"

echo "=== Downloading RetroArch shaders ==="
echo "Source: ${URL}"
echo "Destination: ${SHADERS_DIR}"
echo ""

$DOWNLOAD_CMD "${ZIP_FILE}" "${URL}"

echo ""
echo "=== Extracting shaders ==="

# Extract to temp directory
unzip -q "${ZIP_FILE}" -d "${TEMP_DIR}"

# The archive extracts to slang-shaders-master/
SOURCE_DIR="${TEMP_DIR}/slang-shaders-master"

if [[ ! -d "${SOURCE_DIR}" ]]; then
    echo "Error: Expected directory 'slang-shaders-master' not found after extraction"
    exit 1
fi

# Create target directory if it doesn't exist
mkdir -p "${SHADERS_DIR}"

# Copy all contents, overwriting existing files
# Using rsync if available, cp as fallback
if command -v rsync &> /dev/null; then
    rsync -a --delete "${SOURCE_DIR}/" "${SHADERS_DIR}/"
else
    # Remove existing contents first to match rsync --delete behavior
    find "${SHADERS_DIR}" -mindepth 1 -delete
    cp -r "${SOURCE_DIR}/"* "${SHADERS_DIR}/"
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
SHADER_COUNT=$(find "${SHADERS_DIR}" -name "*.slang" -o -name "*.slangp" | wc -l)
echo "Installed ${SHADER_COUNT} shader files"

echo ""
echo "=== Installation complete ==="
echo "RetroArch shaders installed to: ${SHADERS_DIR}"
echo ""
echo "Tracked files (in git):"
echo "  - crt/zfast-crt.slangp"
echo "  - crt/shaders/zfast_crt/zfast_crt_finemask.slang"
echo "  - crt/shaders/zfast_crt/zfast_crt_impl.inc"
echo ""
echo "All other files are ignored by git."
