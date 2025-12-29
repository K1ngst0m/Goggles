#!/usr/bin/env bash
# Initialize formatting setup: interactively configure IDE formatting if missing, then install pre-commit hook.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Reuse the existing detection from check-ide-setup for the "is there any config?" test.
has_ide_config=false
[[ -f "${PROJECT_ROOT}/.vscode/settings.json" ]] && has_ide_config=true
[[ -f "${PROJECT_ROOT}/.dir-locals.el" ]] && has_ide_config=true
[[ -f "${PROJECT_ROOT}/.exrc" ]] && has_ide_config=true
[[ -f "${PROJECT_ROOT}/.nvim.lua" ]] && has_ide_config=true
[[ -f "${PROJECT_ROOT}/.idea/clangFormatSettings.xml" ]] && has_ide_config=true

# Always run the one-time prompt marker script (it exits quietly after first run)
bash "${PROJECT_ROOT}/scripts/check-ide-setup.sh"

if [[ "$has_ide_config" == "false" ]]; then
    echo "[init] No IDE formatting config detected; launching interactive setup-ide (choose your IDE)."
    bash "${PROJECT_ROOT}/scripts/setup-ide.sh"
else
    echo "[init] IDE formatting config already present; skipping setup-ide."
fi

bash "${PROJECT_ROOT}/scripts/install-pre-commit-hook.sh"
