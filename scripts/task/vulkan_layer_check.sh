#!/usr/bin/env bash
set -euo pipefail

die() {
  echo "Error: $*" >&2
  exit 1
}

[[ -n "${CONDA_PREFIX:-}" ]] || die "CONDA_PREFIX is not set (run via pixi)"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT_DIR="${REPO_ROOT}/.cache/vulkan-layer-check"
mkdir -p "$OUT_DIR"

SRC="${OUT_DIR}/vk_layer_check.c"
BIN="${OUT_DIR}/vk_layer_check"

cat >"$SRC" <<'C'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan.h>

static int has_layer(const char *needle) {
  uint32_t count = 0;
  VkResult r = vkEnumerateInstanceLayerProperties(&count, NULL);
  if (r != VK_SUCCESS) {
    fprintf(stderr, "vkEnumerateInstanceLayerProperties(count) failed: %d\n", (int)r);
    return -1;
  }

  VkLayerProperties *props = (VkLayerProperties *)calloc(count ? count : 1, sizeof(VkLayerProperties));
  if (!props) return -1;

  r = vkEnumerateInstanceLayerProperties(&count, props);
  if (r != VK_SUCCESS) {
    fprintf(stderr, "vkEnumerateInstanceLayerProperties(list) failed: %d\n", (int)r);
    free(props);
    return -1;
  }

  int found = 0;
  for (uint32_t i = 0; i < count; ++i) {
    if (strcmp(props[i].layerName, needle) == 0) {
      found = 1;
      break;
    }
  }

  free(props);
  return found;
}

int main(void) {
  const char *layer = "VK_LAYER_goggles_capture_64";
  int found = has_layer(layer);
  if (found < 0) return 2;
  if (!found) {
    fprintf(stderr, "Layer not found: %s\n", layer);
    return 1;
  }
  printf("Found layer: %s\n", layer);
  return 0;
}
C

CC_BIN="${CC:-cc}"
"$CC_BIN" -O2 -I"${CONDA_PREFIX}/include" -L"${CONDA_PREFIX}/lib" \
  -Wl,-rpath,"${CONDA_PREFIX}/lib" -o "$BIN" "$SRC" -lvulkan

export GOGGLES_CAPTURE=1
export VK_LOADER_DEBUG="${VK_LOADER_DEBUG:-error}"

"$BIN"

