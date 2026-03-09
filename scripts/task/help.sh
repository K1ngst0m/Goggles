#!/bin/bash
cat << 'EOF'
Goggles Pixi Tasks
══════════════════

Build Commands
  pixi run build [-p PRESET]              Build app
  pixi run build-all-presets              Build all CMake presets
  pixi run test [-p PRESET]               Run tests
  pixi run smoke-filter-chain             Run local ABI smoke matrix (shared/static x clang/gcc)

Run Commands
  pixi run start [-p PRESET] [--] <APP> [APP_ARGS...]
                                           Launch app inside nested compositor
  pixi run profile [-p PRESET] [goggles_args...] -- <APP> [APP_ARGS...]
                                           Capture a Goggles Tracy profile session

Utilities
  pixi run format                         Format C/C++ and TOML files
  pixi run clean [-p PRESET]              Remove build directories
  pixi run distclean                      Remove all build directories
  pixi run shader-fetch                   Download RetroArch slang shaders
  pixi run init                           Install or repair managed pre-commit hook

Options
  -p, --preset PRESET   Build preset (default: debug)
                        Valid: debug, release, relwithdebinfo, asan, ubsan, asan-ubsan, test, quality, profile,
                               smoke-static-clang, smoke-shared-clang, smoke-static-gcc, smoke-shared-gcc

Examples
  pixi run build                          Build with default preset (debug)
  pixi run build -p release               Build with release preset
  pixi run start -- vkcube                Run vkcube in the compositor
  pixi run start -p release -- vkcube     Run with release build
  pixi run profile -- vkcube              Capture a viewer Tracy trace
EOF
