#!/bin/bash
cat << 'EOF'
Goggles Pixi Tasks
══════════════════

Build Commands
  pixi run dev [-p PRESET]                Build full dev environment
  pixi run build [-p PRESET]              Build 64-bit app + layer
  pixi run build-i686 [-p PRESET]         Build 32-bit layer only
  pixi run build-all-presets              Build all CMake presets (incl. i686)
  pixi run test [-p PRESET]               Run tests
  pixi run install-manifests [-p PRESET]  Install Vulkan layer manifests

Run Commands
  pixi run start [-p PRESET] [--] <APP> [APP_ARGS...]
                                          Launch app with capture + viewer

Utilities
  pixi run format                         Format C/C++ and TOML files
  pixi run clean [-p PRESET]              Remove build directories
  pixi run distclean                      Remove all build directories
  pixi run shader-fetch                   Download RetroArch slang shaders
  pixi run init                           First-time project setup (pre-commit hook)

Options
  -p, --preset PRESET   Build preset (default: debug)
                        Valid: debug, release, relwithdebinfo, asan, ubsan, test

Examples
  pixi run build                          Build with default preset (debug)
  pixi run build -p release               Build with release preset
  pixi run start vkcube                   Run vkcube with capture
  pixi run start -p release vkcube        Run with release build
EOF
