#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null || true)"
if [[ -z "$REPO_ROOT" ]]; then
  REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
fi

die() {
  echo "Error: $*" >&2
  exit 1
}

PRESET="release"

while [[ $# -gt 0 ]]; do
  case "$1" in
    -p|--preset)
      [[ $# -ge 2 ]] || die "$1 requires a preset name"
      PRESET="$2"
      shift 2
      ;;
    --preset=*)
      PRESET="${1#*=}"
      shift
      ;;
    -h|--help)
      cat <<EOF
Usage:
  pixi run appimage-build [-p PRESET]

Builds an AppImage from dist/appimage/Goggles.AppDir.
EOF
      exit 0
      ;;
    *)
      die "Unknown option: $1"
      ;;
  esac
done

cd "$REPO_ROOT"

OUT_ROOT="$REPO_ROOT/dist/appimage"
APPDIR="$OUT_ROOT/Goggles.AppDir"
[[ -d "$APPDIR" ]] || die "Missing staged AppDir: $APPDIR (run appimage-stage first)"

VERSION="$(cat "$APPDIR/usr/share/goggles/VERSION" 2>/dev/null || true)"
[[ -n "$VERSION" ]] || die "Missing VERSION in AppDir"

TOOLS_DIR="$REPO_ROOT/.cache/appimage-tools"
mkdir -p "$TOOLS_DIR"

# appimagetool is not available on conda-forge (as of 2026-01-09), so we download it.
# Pin to a stable release for reproducibility.
APPIMAGETOOL_VERSION="${APPIMAGETOOL_VERSION:-1.9.1}"
APPIMAGETOOL_URL="${APPIMAGETOOL_URL:-https://github.com/AppImage/appimagetool/releases/download/${APPIMAGETOOL_VERSION}/appimagetool-x86_64.AppImage}"
APPIMAGETOOL_SHA256="${APPIMAGETOOL_SHA256:-ed4ce84f0d9caff66f50bcca6ff6f35aae54ce8135408b3fa33abfc3cb384eb0}"
APPIMAGETOOL="$TOOLS_DIR/appimagetool-${APPIMAGETOOL_VERSION}-x86_64.AppImage"

command -v curl >/dev/null 2>&1 || die "curl is required to download appimagetool"
command -v sha256sum >/dev/null 2>&1 || die "sha256sum is required to verify appimagetool"

need_download=1
if [[ -f "$APPIMAGETOOL" ]]; then
  current_sha="$(sha256sum "$APPIMAGETOOL" | awk '{print $1}')"
  if [[ "$current_sha" == "$APPIMAGETOOL_SHA256" ]]; then
    need_download=0
  else
    echo "appimagetool checksum mismatch; re-downloading (got=$current_sha expected=$APPIMAGETOOL_SHA256)"
    rm -f "$APPIMAGETOOL"
  fi
fi

if [[ "$need_download" -eq 1 ]]; then
  echo "Downloading appimagetool ${APPIMAGETOOL_VERSION}..."
  tmp="$(mktemp)"
  trap 'rm -f "$tmp"' RETURN
  curl -L --fail -o "$tmp" "$APPIMAGETOOL_URL"
  got_sha="$(sha256sum "$tmp" | awk '{print $1}')"
  [[ "$got_sha" == "$APPIMAGETOOL_SHA256" ]] || die "appimagetool checksum mismatch (got=$got_sha expected=$APPIMAGETOOL_SHA256)"
  mv -f "$tmp" "$APPIMAGETOOL"
  chmod +x "$APPIMAGETOOL"
fi

OUTPUT_SUFFIX=""
if [[ -n "${PRESET}" ]] && [[ "${PRESET}" != "release" ]]; then
  # Avoid creating nested paths if a preset contains '/'.
  safe_preset="${PRESET//\//-}"
  OUTPUT_SUFFIX="-${safe_preset}"
fi

OUTPUT="$REPO_ROOT/dist/Goggles-${VERSION}${OUTPUT_SUFFIX}-x86_64.AppImage"
ARCH=x86_64 "$APPIMAGETOOL" "$APPDIR" "$OUTPUT"

echo "Built: $OUTPUT"
