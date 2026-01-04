#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
Usage: scripts/build_all_presets.sh [--no-i686] [--clean]

Builds all non-hidden CMake configure/build presets. By default, also builds the
corresponding i686 preset for each non-i686 preset (e.g. debug + debug-i686).

Options:
  --no-i686   Skip all i686 builds
  --clean     Remove build directories for each preset before building
EOF
}

include_i686=1
clean=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-i686)
      include_i686=0
      shift
      ;;
    --clean)
      clean=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

ccache_tempdir="${CCACHE_TEMPDIR:-"$PWD/.cache/ccache-tmp"}"
mkdir -p "$ccache_tempdir"
export CCACHE_TEMPDIR="$ccache_tempdir"

mapfile -t all_configure_presets < <(
  cmake --list-presets=configure | sed -n 's/^  "\([^"]\+\)".*/\1/p'
)

declare -A preset_set=()
for preset in "${all_configure_presets[@]}"; do
  preset_set["$preset"]=1
done

base_presets=()
for preset in "${all_configure_presets[@]}"; do
  if [[ "$preset" == *-i686 ]]; then
    continue
  fi
  base_presets+=("$preset")
done

build_one() {
  local preset="$1"

  echo "==> Building preset: $preset"
  if [[ $clean -eq 1 ]]; then
    if [[ -z "$preset" || "$preset" == *"/"* || "$preset" == *".."* ]]; then
      echo "Refusing to clean unsafe preset name: '$preset'" >&2
      exit 1
    fi
    rm -rf "build/$preset"
  fi

  cmake --preset "$preset"
  cmake --build --preset "$preset"
}

for preset in "${base_presets[@]}"; do
  build_one "$preset"

  if [[ $include_i686 -eq 1 ]]; then
    i686_preset="${preset}-i686"
    if [[ -z "${preset_set[$i686_preset]+x}" ]]; then
      echo "Missing i686 preset for '$preset': expected '$i686_preset'" >&2
      exit 1
    fi
    build_one "$i686_preset"
  fi
done

echo "All presets built successfully."
