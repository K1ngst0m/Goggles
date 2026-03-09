#!/usr/bin/env bash
# Init: install or repair the managed pre-commit hook.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

bash "${PROJECT_ROOT}/scripts/install-pre-commit-hook.sh"
