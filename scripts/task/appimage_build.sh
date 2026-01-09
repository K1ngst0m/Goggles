#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

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

APPIMAGETOOL="$TOOLS_DIR/appimagetool-x86_64.AppImage"
if [[ ! -x "$APPIMAGETOOL" ]]; then
  echo "Downloading appimagetool..."
  curl -L --fail -o "$APPIMAGETOOL" \
    "https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage"
  chmod +x "$APPIMAGETOOL"
fi

OUTPUT="$REPO_ROOT/dist/Goggles-${VERSION}-x86_64.AppImage"
ARCH=x86_64 "$APPIMAGETOOL" "$APPDIR" "$OUTPUT"

echo "Built: $OUTPUT"
