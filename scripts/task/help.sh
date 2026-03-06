#!/bin/bash
cat << 'EOF'
Goggles Pixi Tasks
══════════════════

Build Commands
  pixi run dev [-p PRESET]                Build the project
  pixi run build [-p PRESET]              Build 64-bit app
  pixi run build-all-presets              Build all CMake presets
  pixi run test [-p PRESET]               Run tests
  pixi run smoke-filter-chain             Run local ABI smoke matrix (shared/static x clang/gcc)
  pixi run verify-filter-chain-standalone-graph
                                          Validate standalone target graph boundaries
  pixi run verify-filter-chain-standalone-deps
                                          Validate standalone dependency pinning constraints
  pixi run filter-chain-standalone-release-dry-run
                                          Generate standalone release dry-run artifacts
  pixi run verify-filter-chain-standalone-status-checks
                                           Validate standalone workflow status-check contract
  pixi run verify-policy-artifacts [-- --util-core-policy <path> --channel-allowlist <path> --provenance <dir>]
                                           Validate util-core policy and channel allowlist trust artifacts
  pixi run verify-filter-chain-host-migration
                                           Ensure host stage/order contracts stay aligned with the filter chain
  pixi run verify-filter-chain-host-dual-path
                                          Validate standalone + fallback host build paths before flipping

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
                        Valid: debug, release, relwithdebinfo, asan, ubsan, asan-ubsan, test, quality, profile,
                               smoke-static-clang, smoke-shared-clang, smoke-static-gcc, smoke-shared-gcc

Examples
  pixi run build                          Build with default preset (debug)
  pixi run build -p release               Build with release preset
  pixi run start vkcube                   Run vkcube in the compositor
  pixi run start -p release vkcube        Run with release build
  pixi run profile -- vkcube              Capture + merge Tracy traces
EOF
