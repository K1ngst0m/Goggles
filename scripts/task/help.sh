#!/bin/bash
cat << 'EOF'
Goggles Pixi Tasks
══════════════════

Build Commands
  pixi run build [-p PRESET]              Build app
  pixi run build-all-presets              Build all CMake presets
  pixi run test [-p PRESET]               Run tests
  pixi run ci [--lane LANE] [--runner RUNNER] [--cache-mode MODE] [--base-ref REF]
                                            Run CI lanes locally on host or in a container
                                            Lanes: format, build-test, static-analysis,
                                                   static-analysis-semgrep, static-analysis-quality,
                                                   static-analysis-quality-pr
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
  --base-ref REF        Git ref used by `static-analysis-quality-pr` changed-file selection

Examples
  pixi run build                          Build with default preset (debug)
  pixi run build -p release               Build with release preset
  pixi run ci --lane build-test           Run the CI build-and-test lane on host
  pixi run ci --lane static-analysis      Run both static-analysis lanes sequentially
  pixi run ci --lane static-analysis-semgrep
                                            Run only the Semgrep static-analysis lane
  pixi run ci --lane static-analysis-quality
                                             Run only the quality-build static-analysis lane
  pixi run ci --lane static-analysis-quality-pr --base-ref main
                                             Run clang-tidy only on changed src files relative to main
  pixi run ci --runner container --cache-mode cold --lane build-test
                                              Run the build-and-test lane in a fresh CI container
  pixi run start -- vkcube                Run vkcube in the compositor
  pixi run start -p release -- vkcube     Run with release build
  pixi run profile -- vkcube              Capture a viewer Tracy trace
EOF
