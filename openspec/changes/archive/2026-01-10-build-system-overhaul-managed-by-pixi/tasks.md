# Tasks

## 1. Dependency Management Strategy

- [x] Pin `sysroot_linux-64` to `2.28.*` in `pixi.toml` for Glibc 2.28 compatibility
- [x] Relax all other package constraints to `*` to leverage Pixi's solver
- [x] Update `.gitignore` to better handle Pixi artifacts and Conda packages

## 2. Build Environment Enforcement

- [x] Implement `CONDA_PREFIX` check in `cmake/Dependencies.cmake` to ban non-Pixi builds
- [x] Fix implicit threading assumptions by adding `find_package(Threads REQUIRED)`
- [x] Explicitly link `Threads::Threads` to `goggles_util`
- [x] Make missing sysroot a fatal error in 32-bit toolchain configuration

## 3. Package Integrity & Security (Sysroot-i686)

- [x] Implement SHA256 checksum verification for all 10+ upstream Debian packages
- [x] Add Git commit hash verification for Tracy source download
- [x] Implement automated fix for broken GCC development symlinks in `usr/lib`
- [x] Increment package build number

## 4. Verification

- [x] Verify complete dependency resolution (`pixi install`) with new constraints
- [x] Verify successful compilation and linkage of unit tests (`pixi run test`)

## 5. Environment Isolation & Robustness (Refinement)

- [x] Remove conflicting `bash -lc` login shell usage from `pixi.toml` to fix PATH priority
- [x] Add `pkg-config` to dependencies and set `PKG_CONFIG_PATH` for isolated build configuration
- [x] Remove redundant `scripts/pixi-env-clean.sh` wrapper script
- [x] Verify `vkcube` and `vkcube32` binary resolution (Pixi vs System)
- [x] Verify Vulkan Layer loading isolation (Pixi Manifest vs System Manifest)