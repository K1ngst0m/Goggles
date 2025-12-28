#!/usr/bin/env bash
# Run a command with Vulkan layer overrides stripped from the host.
# Default: unset host VK_ADD_LAYER_PATH to avoid leaking explicit layers into pixi tasks.
# Opt-out: set KEEP_HOST_VK_ADD_LAYER=1 to keep the host value (for debugging GPU apps).

set -euo pipefail

if [[ -z "${KEEP_HOST_VK_ADD_LAYER:-}" ]]; then
    unset VK_ADD_LAYER_PATH
fi

exec "$@"
