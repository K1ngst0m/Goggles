#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

TRACY_VERSION="${TRACY_VERSION_OVERRIDE:-0.13.1}"
TRACY_TAG="v${TRACY_VERSION}"
TOOLS_ROOT="${REPO_ROOT}/build/tools/tracy-${TRACY_VERSION}"
SRC_DIR="${TOOLS_ROOT}/src"

clone_tracy_source() {
  if [[ -d "${SRC_DIR}/.git" ]]; then
    return
  fi

  mkdir -p "${TOOLS_ROOT}"
  git clone --depth 1 --branch "${TRACY_TAG}" https://github.com/wolfpld/tracy.git "${SRC_DIR}"
}

build_tool() {
  local subdir="$1"
  local target="$2"
  local build_dir="${TOOLS_ROOT}/${subdir}-build"
  local binary="${build_dir}/${target}"

  if [[ -x "${binary}" ]]; then
    printf '%s\n' "${binary}"
    return
  fi

  rm -rf "${build_dir}"
  cmake -S "${SRC_DIR}/${subdir}" -B "${build_dir}" -G Ninja -DCMAKE_BUILD_TYPE=Debug >&2
  cmake --build "${build_dir}" -j >&2

  if [[ ! -x "${binary}" ]]; then
    echo "Error: expected Tracy tool not found: ${binary}" >&2
    exit 1
  fi

  printf '%s\n' "${binary}"
}

clone_tracy_source

TRACY_CAPTURE_BIN="$(build_tool capture tracy-capture)"
TRACY_CSVEXPORT_BIN="$(build_tool csvexport tracy-csvexport)"
TRACY_IMPORT_CHROME_BIN="$(build_tool import tracy-import-chrome)"

printf 'TRACY_CAPTURE_BIN=%q\n' "${TRACY_CAPTURE_BIN}"
printf 'TRACY_CSVEXPORT_BIN=%q\n' "${TRACY_CSVEXPORT_BIN}"
printf 'TRACY_IMPORT_CHROME_BIN=%q\n' "${TRACY_IMPORT_CHROME_BIN}"
