#!/usr/bin/env bash
# setup-ide.sh - Configure IDE to use pixi-managed clang-format
#
# This script generates local IDE configuration files (not committed to git)
# that point to the clang-format in the pixi environment, ensuring consistent
# code formatting across all contributors.
#
# Usage: pixi run setup-ide [ide]
#   ide: vscode, emacs, vim, neovim, clion, all
#        If not specified, interactive selection is shown.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PIXI_ENV="${PROJECT_ROOT}/.pixi/envs/default"
CLANG_FORMAT="${PIXI_ENV}/bin/clang-format"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

info() { echo -e "${BLUE}[INFO]${NC} $*"; }
success() { echo -e "${GREEN}[OK]${NC} $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $*"; }
error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }

# Check if pixi environment exists
check_pixi_env() {
    if [[ ! -x "$CLANG_FORMAT" ]]; then
        error "clang-format not found at: $CLANG_FORMAT"
        error "Please run 'pixi install' first."
        exit 1
    fi

    local version
    version=$("$CLANG_FORMAT" --version | head -1)
    info "Using: $version"
}

# VSCode configuration
setup_vscode() {
    info "Setting up VSCode..."

    local vscode_dir="${PROJECT_ROOT}/.vscode"
    local settings_file="${vscode_dir}/settings.json"

    mkdir -p "$vscode_dir"

    # Generate settings.json
    # Note: xaver.clang-format uses ${workspaceRoot}, not ${workspaceFolder}
    #       C_Cpp extension uses ${workspaceFolder}
    cat > "$settings_file" << 'EOF'
{
    "clang-format.executable": "${workspaceRoot}/.pixi/envs/default/bin/clang-format",
    "editor.formatOnSave": true,
    "[cpp]": {
        "editor.defaultFormatter": "xaver.clang-format"
    },
    "[c]": {
        "editor.defaultFormatter": "xaver.clang-format"
    },
    "C_Cpp.formatting": "clangFormat",
    "C_Cpp.clang_format_path": "${workspaceFolder}/.pixi/envs/default/bin/clang-format"
}
EOF

    success "Created ${settings_file}"
    info "  Required extension: xaver.clang-format"
    info "  Install: code --install-extension xaver.clang-format"
}

# Emacs configuration
setup_emacs() {
    info "Setting up Emacs..."

    local dirlocals_file="${PROJECT_ROOT}/.dir-locals.el"

    cat > "$dirlocals_file" << 'EOF'
;;; Directory Local Variables for Goggles project
;;; Use pixi-managed clang-format for consistent formatting
;;;
;;; Requirements:
;;;   - clang-format package (MELPA) or emacs-clang-format
;;;   - Run 'pixi install' to get clang-format binary

((nil . ((eval . (setq-local clang-format-executable
                             (expand-file-name ".pixi/envs/default/bin/clang-format"
                                               (locate-dominating-file default-directory "pixi.toml"))))))

 (c-mode . ((mode . c++)
            (eval . (add-hook 'before-save-hook #'clang-format-buffer nil t))))

 (c++-mode . ((eval . (add-hook 'before-save-hook #'clang-format-buffer nil t)))))
EOF

    success "Created ${dirlocals_file}"
    info "  Required package: clang-format (from MELPA)"
    info "  Add to init.el: (use-package clang-format :ensure t)"
}

# Vim configuration
setup_vim() {
    info "Setting up Vim..."

    local vim_dir="${PROJECT_ROOT}/.vim"
    local exrc_file="${PROJECT_ROOT}/.exrc"

    mkdir -p "$vim_dir"

    # Create .exrc for classic vim
    cat > "$exrc_file" << 'EOF'
" Goggles project local Vim configuration
" Use pixi-managed clang-format for consistent formatting
"
" Requirements:
"   - vim-clang-format plugin (https://github.com/rhysd/vim-clang-format)
"   - Run 'pixi install' to get clang-format binary

let g:clang_format#command = getcwd() . '/.pixi/envs/default/bin/clang-format'
let g:clang_format#auto_format = 1
let g:clang_format#auto_format_on_insert_leave = 0

" Format on save for C/C++ files
augroup ClangFormatOnSave
    autocmd!
    autocmd BufWritePre *.c,*.cpp,*.h,*.hpp ClangFormat
augroup END
EOF

    success "Created ${exrc_file}"
    info "  Required plugin: vim-clang-format"
    info "  Add 'set exrc' to your .vimrc to enable project-local config"
}

# Neovim configuration
setup_neovim() {
    info "Setting up Neovim..."

    local nvim_file="${PROJECT_ROOT}/.nvim.lua"

    cat > "$nvim_file" << 'EOF'
-- Goggles project local Neovim configuration
-- Use pixi-managed clang-format for consistent formatting
--
-- Requirements:
--   - conform.nvim OR null-ls.nvim OR vim-clang-format
--   - Run 'pixi install' to get clang-format binary
--
-- Add to your init.lua: vim.o.exrc = true

local project_root = vim.fn.getcwd()
local clang_format_path = project_root .. "/.pixi/envs/default/bin/clang-format"

-- For vim-clang-format plugin
vim.g.clang_format_command = clang_format_path
vim.g["clang_format#command"] = clang_format_path
vim.g["clang_format#auto_format"] = 1

-- For conform.nvim (recommended)
-- Add this to your conform setup:
-- require("conform").setup({
--   formatters = {
--     clang_format = {
--       command = clang_format_path,
--     },
--   },
-- })

-- For null-ls.nvim
-- local null_ls = require("null-ls")
-- null_ls.setup({
--   sources = {
--     null_ls.builtins.formatting.clang_format.with({
--       command = clang_format_path,
--     }),
--   },
-- })

-- Auto-format on save
vim.api.nvim_create_autocmd("BufWritePre", {
    pattern = { "*.c", "*.cpp", "*.h", "*.hpp" },
    callback = function()
        -- For conform.nvim users, uncomment:
        -- require("conform").format({ bufnr = 0 })

        -- For vim-clang-format users:
        if vim.fn.exists(":ClangFormat") == 2 then
            vim.cmd("ClangFormat")
        end
    end,
})

print("[Goggles] Using clang-format: " .. clang_format_path)
EOF

    success "Created ${nvim_file}"
    info "  Recommended: conform.nvim or vim-clang-format"
    info "  Add 'vim.o.exrc = true' to your init.lua"
}

# CLion/JetBrains configuration
setup_clion() {
    info "Setting up CLion/JetBrains..."

    local idea_dir="${PROJECT_ROOT}/.idea"
    local clangformat_file="${idea_dir}/clangFormatSettings.xml"

    mkdir -p "$idea_dir"

    cat > "$clangformat_file" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<project version="4">
  <component name="ClangFormatSettings">
    <option name="ENABLED" value="true" />
    <option name="CLANG_FORMAT_PATH" value="\$PROJECT_DIR\$/.pixi/envs/default/bin/clang-format" />
  </component>
</project>
EOF

    success "Created ${clangformat_file}"
    info "  Enable: Settings -> Editor -> Code Style -> Enable ClangFormat"
}

# Show interactive menu
show_menu() {
    echo ""
    echo "╔════════════════════════════════════════════════════════════╗"
    echo "║          Goggles IDE Setup - clang-format Config           ║"
    echo "╠════════════════════════════════════════════════════════════╣"
    echo "║  This will generate local config files (not in git) that  ║"
    echo "║  point your IDE to the pixi-managed clang-format binary.  ║"
    echo "╚════════════════════════════════════════════════════════════╝"
    echo ""
    echo "Select your IDE:"
    echo "  1) VSCode"
    echo "  2) Emacs"
    echo "  3) Vim"
    echo "  4) Neovim"
    echo "  5) CLion/JetBrains"
    echo "  6) All of the above"
    echo "  q) Quit"
    echo ""
    read -rp "Enter choice [1-6, q]: " choice

    case "$choice" in
        1) setup_vscode ;;
        2) setup_emacs ;;
        3) setup_vim ;;
        4) setup_neovim ;;
        5) setup_clion ;;
        6)
            setup_vscode
            setup_emacs
            setup_vim
            setup_neovim
            setup_clion
            ;;
        q|Q) exit 0 ;;
        *) error "Invalid choice"; exit 1 ;;
    esac
}

# Main
main() {
    cd "$PROJECT_ROOT"

    check_pixi_env

    local ide="${1:-}"

    case "$ide" in
        vscode)  setup_vscode ;;
        emacs)   setup_emacs ;;
        vim)     setup_vim ;;
        neovim)  setup_neovim ;;
        clion)   setup_clion ;;
        all)
            setup_vscode
            setup_emacs
            setup_vim
            setup_neovim
            setup_clion
            ;;
        "")      show_menu ;;
        *)
            error "Unknown IDE: $ide"
            echo "Usage: $0 [vscode|emacs|vim|neovim|clion|all]"
            exit 1
            ;;
    esac

    echo ""
    success "IDE setup complete!"
    info "Generated files are in .gitignore and won't be committed."
}

main "$@"
