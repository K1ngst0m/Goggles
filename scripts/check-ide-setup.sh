#!/usr/bin/env bash
# check-ide-setup.sh - Check if IDE is configured for clang-format
# This script runs on pixi shell/run activation and prompts once if setup is needed.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
MARKER_FILE="${PROJECT_ROOT}/.pixi/.ide-setup-prompted"

# Colors
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

# Skip if already prompted
[[ -f "$MARKER_FILE" ]] && exit 0

# Check if any IDE config exists
ide_configured=false
[[ -f "${PROJECT_ROOT}/.vscode/settings.json" ]] && ide_configured=true
[[ -f "${PROJECT_ROOT}/.dir-locals.el" ]] && ide_configured=true
[[ -f "${PROJECT_ROOT}/.exrc" ]] && ide_configured=true
[[ -f "${PROJECT_ROOT}/.nvim.lua" ]] && ide_configured=true
[[ -f "${PROJECT_ROOT}/.idea/clangFormatSettings.xml" ]] && ide_configured=true

if [[ "$ide_configured" == "false" ]]; then
    echo ""
    echo -e "${YELLOW}┌─────────────────────────────────────────────────────────────┐${NC}"
    echo -e "${YELLOW}│${NC}  ${CYAN}IDE clang-format not configured${NC}                            ${YELLOW}│${NC}"
    echo -e "${YELLOW}│${NC}  Run: ${CYAN}pixi run setup-ide${NC}  to configure your IDE             ${YELLOW}│${NC}"
    echo -e "${YELLOW}└─────────────────────────────────────────────────────────────┘${NC}"
    echo ""
fi

# Create marker so we don't prompt again
mkdir -p "$(dirname "$MARKER_FILE")"
touch "$MARKER_FILE"
