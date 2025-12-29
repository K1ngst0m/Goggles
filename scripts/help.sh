#!/bin/bash
cat << 'EOF'
Goggles Pixi Tasks
══════════════════

Build Commands
  pixi run dev [PRESET]                Build full dev environment
  pixi run build [PRESET]              Build 64-bit app + layer
  pixi run build-i686 [PRESET]         Build 32-bit layer only
  pixi run test [PRESET]               Run tests
  pixi run install-manifests [PRESET]  Install Vulkan layer manifests

Run Commands
  pixi run start <APP> [PRESET]        Launch app with capture + viewer

Utilities
  pixi run format                      Format C/C++ and TOML files
  pixi run clean [PRESET]              Remove build directories
  pixi run distclean                   Remove all build directories
  pixi run shader-fetch                Download RetroArch slang shaders
  pixi run setup-ide [IDE]             Configure IDE clang-format
  pixi run init                        First-time project setup

Arguments
  [PRESET]  Build preset: debug (default), release, asan, tsan
  <APP>     Application to launch (required)
  [IDE]     IDE to configure: vscode, emacs, vim, neovim, clion, all
EOF
