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
  pixi run appimage-stage [-p PRESET]

Stages an AppDir at dist/appimage/Goggles.AppDir using build/<preset>/ outputs.
EOF
      exit 0
      ;;
    *)
      die "Unknown option: $1"
      ;;
  esac
done

[[ -n "${CONDA_PREFIX:-}" ]] || die "CONDA_PREFIX is not set (run via pixi)"

cd "$REPO_ROOT"

VERSION="$(sed -n 's/^project(goggles VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt | tail -n 1)"
[[ -n "$VERSION" ]] || die "Failed to detect version from CMakeLists.txt"

copy_git_tracked_paths() {
  local dest_root="$1"
  shift

  mkdir -p "$dest_root"

  local path
  while IFS= read -r -d '' path; do
    mkdir -p "$dest_root/$(dirname "$path")"
    cp -a "$REPO_ROOT/$path" "$dest_root/$path"
  done < <(git -C "$REPO_ROOT" ls-files -z -- "$@")
}

BUILD_DIR="$REPO_ROOT/build/$PRESET"
BUILD_I686_DIR="$REPO_ROOT/build/${PRESET}-i686"

GOGGLES_BIN="$BUILD_DIR/bin/goggles"
LAYER64="$BUILD_DIR/lib/x86_64/libgoggles_vklayer.so"
LAYER32="$BUILD_I686_DIR/lib/i386/libgoggles_vklayer.so"

[[ -x "$GOGGLES_BIN" ]] || die "Missing built viewer binary: $GOGGLES_BIN"
[[ -f "$LAYER64" ]] || die "Missing built 64-bit layer: $LAYER64"
[[ -f "$LAYER32" ]] || die "Missing built 32-bit layer: $LAYER32"

OUT_ROOT="$REPO_ROOT/dist/appimage"
APPDIR="$OUT_ROOT/Goggles.AppDir"

rm -rf "$APPDIR"
mkdir -p \
  "$APPDIR/usr/bin" \
  "$APPDIR/usr/lib" \
  "$APPDIR/usr/share/goggles/config" \
  "$APPDIR/usr/share/goggles/assets" \
  "$APPDIR/usr/share/goggles/shaders" \
  "$APPDIR/usr/share/goggles/vulkan-layers/x86_64" \
  "$APPDIR/usr/share/goggles/vulkan-layers/i386"

cp -f "$REPO_ROOT/platform/linux/appimage/AppRun" "$APPDIR/AppRun"
cp -f "$REPO_ROOT/platform/linux/appimage/goggles.desktop" "$APPDIR/goggles.desktop"

# AppImage icon convention: <IconName>.png at AppDir root (IconName from .desktop).
cp -f "$REPO_ROOT/showcase-crt-royale.png" "$APPDIR/goggles.png"

cp -f "$GOGGLES_BIN" "$APPDIR/usr/bin/goggles"

cp -f "$REPO_ROOT/config/goggles.toml" "$APPDIR/usr/share/goggles/config/goggles.toml"

# Package only git-tracked resources (avoid shipping locally-downloaded shader packs).
copy_git_tracked_paths "$APPDIR/usr/share/goggles" assets shaders

cp -f "$LAYER64" "$APPDIR/usr/share/goggles/vulkan-layers/x86_64/libgoggles_vklayer.so"
cp -f "$LAYER32" "$APPDIR/usr/share/goggles/vulkan-layers/i386/libgoggles_vklayer.so"

echo "$VERSION" >"$APPDIR/usr/share/goggles/VERSION"

# Bundle Pixi/Conda libs used by the viewer into AppDir/usr/lib.
echo "Collecting runtime libraries from CONDA_PREFIX..."
mapfile -t deps < <(ldd "$APPDIR/usr/bin/goggles" | awk -v p="$CONDA_PREFIX" '
  $0 ~ /=>/ {
    for (i=1;i<=NF;i++) if ($i ~ /^\//) { print $i }
  }
' | sort -u)

for lib in "${deps[@]}"; do
  case "$lib" in
    "$CONDA_PREFIX"/*)
      # Preserve symlink names like libfmt.so.11 -> libfmt.so.11.2.0 so DT_NEEDED lookups succeed.
      cp -Pf "$lib" "$APPDIR/usr/lib/$(basename "$lib")"
      real="$(readlink -f "$lib" 2>/dev/null || echo "$lib")"
      if [[ "$real" != "$lib" ]]; then
        cp -Lf "$real" "$APPDIR/usr/lib/$(basename "$real")"
      fi
      ;;
  esac
done

# Make the executable load bundled libs first.
patchelf --set-rpath '$ORIGIN/../lib' "$APPDIR/usr/bin/goggles" 2>/dev/null || true
patchelf --set-interpreter /lib64/ld-linux-x86-64.so.2 "$APPDIR/usr/bin/goggles" 2>/dev/null || true

chmod +x "$APPDIR/AppRun"

echo "Staged: $APPDIR"
