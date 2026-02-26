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
  pixi run appimage [-p PRESET]

Builds Goggles (viewer) and produces an AppImage under dist/.
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

echo "Building preset '${PRESET}'..."
"$SCRIPT_DIR/build.sh" -p "$PRESET"

echo "Staging AppDir..."
"$SCRIPT_DIR/appimage_stage.sh" -p "$PRESET"

echo "Building AppImage..."
"$SCRIPT_DIR/appimage_build.sh" -p "$PRESET"

echo "Done."

