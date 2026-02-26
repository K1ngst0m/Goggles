#!/bin/bash
cat << 'EOF'
Goggles Pixi Tasks
══════════════════

Build Commands
  pixi run dev [-p PRESET]                Build the project
  pixi run build [-p PRESET]              Build 64-bit app
  pixi run build-all-presets              Build all CMake presets
  pixi run test [-p PRESET]               Run tests

Run Commands
  pixi run start [-p PRESET] [--] <APP> [APP_ARGS...]
                                          Launch app inside nested compositor
  pixi run profile [-p PRESET] [goggles_args...] -- <APP> [APP_ARGS...]
                                          Run dual-process Tracy profile session

Utilities
  pixi run format                         Format C/C++ and TOML files
  pixi run clean [-p PRESET]              Remove build directories
  pixi run distclean                      Remove all build directories
  pixi run shader-fetch                   Download RetroArch slang shaders
  pixi run init                           First-time project setup (pre-commit hook)

Options
  -p, --preset PRESET   Build preset (default: debug)
                        Valid: debug, release, relwithdebinfo, asan, ubsan, asan-ubsan, test, quality, profile

Examples
  pixi run build                          Build with default preset (debug)
  pixi run build -p release               Build with release preset
  pixi run start vkcube                   Run vkcube in the compositor
  pixi run start -p release vkcube        Run with release build
  pixi run profile -- vkcube              Capture + merge Tracy traces
EOF
