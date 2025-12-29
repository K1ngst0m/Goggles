# build-system-overhaul-managed-by-pixi

## Why

1.  **Strict Environment Control**: We are transitioning the project to be fully managed by Pixi, ensuring that all builds happen within a deterministic, reproducible, and isolated environment.
2.  **Cross-Distro Compatibility**: To guarantee that binaries built on modern machines run on older LTS Linux distributions (e.g., RHEL 8, Ubuntu 20.04), we must anchor the build environment to an older Glibc version.
3.  **Build System Integrity**: The previous build system had gaps:
    *   Missing integrity checks (checksums) for downloaded dependencies.
    *   Implicit reliance on system libraries (like `pthread`), causing failures in strict environments.
    *   Broken symlinks in the 32-bit sysroot package.
    *   Ability to accidentally run builds outside the Pixi environment.

## What

1.  **Pixi Environment Anchor**:
    *   Pin `sysroot_linux-64` to `2.28.*` (Glibc 2.28).
    *   Relax all other package versions to `*` to allow Pixi's SAT solver to find the optimal compatible set of dependencies.
2.  **Build System Hardening**:
    *   Enforce `CONDA_PREFIX` check in CMake to prevent non-Pixi builds.
    *   Upgrade sysroot detection warnings to fatal errors.
    *   Explicitly handle threading library linkage (`pthread`) which is no longer implicit in strict cross-compilation environments.
3.  **Package Recipe Security**:
    *   Add SHA256 checksum verification for all upstream `.deb` assets in `sysroot-i686`.
    *   Add Git commit hash verification for source dependencies (Tracy).
    *   Implement self-repair logic for broken upstream symlinks in the sysroot.

## How

1.  **`pixi.toml`**:
    *   Set `sysroot_linux-64 = "2.28.*"`.
    *   Set build dependencies (CMake, Ninja, Clang, etc.) and libraries (SDL3, Vulkan, etc.) to wildcard `*`.
    *   Refine `.gitignore` to exclude Pixi artifacts properly.
2.  **CMake Configuration**:
    *   `cmake/Dependencies.cmake`: Add `find_package(Threads REQUIRED)` and `ENV{CONDA_PREFIX}` check.
    *   `src/util/CMakeLists.txt`: Link `Threads::Threads` to `goggles_util`.
    *   `cmake/toolchain-i686.cmake`: Change `message(WARNING ...)` to `message(FATAL_ERROR ...)` for missing sysroot.
3.  **`packages/sysroot-i686/recipe.yaml`**:
    *   Implement a `download_extract` function that verifies SHA256 hashes before extraction.
    *   Add a script block to detect and re-link broken absolute symlinks (like `libstdc++.so`) to relative paths within the sysroot.
    *   Pin and verify the Tracy git commit hash.

## Impact

*   **Reproducibility**: `pixi.lock` now fully dictates the build state; `sysroot` anchors it to a stable baseline.
*   **Security**: Supply chain attacks via compromised download mirrors are mitigated by checksum verification.
*   **Stability**: The 32-bit cross-compilation layer is robust against upstream packaging errors (broken symlinks).
*   **Developer Experience**: "It works on my machine" issues are eliminated by enforcing the Pixi environment.

## Refinement: Environment Isolation (Post-Implementation)

### Why
During verification, we discovered that `bash -lc` (login shell) commands in `pixi.toml` caused Pixi's environment variables (PATH) to be shadowed by system initialization scripts (e.g., Nix profiles), leading to binary version mismatches and Vulkan Loader errors (`libvulkan.so` version conflict). Additionally, the wrapper script `scripts/pixi-env-clean.sh` was aggressively unsetting variables, preventing mixed-environment testing.

### What
1.  **Strict Pixi Priority**: Ensure Pixi binaries and libraries always take precedence over system/Nix paths.
2.  **Robust 32-bit Execution**: Ensure `vkcube32` runs within the Pixi environment without manual `LD_LIBRARY_PATH` hacks that cause glibc conflicts.
3.  **Simplified Configuration**: Remove redundant wrapper scripts.

### How
1.  **`pixi.toml`**:
    *   Replaced all `bash -lc` with `bash -c` to prevent loading user shell profiles.
    *   Defined global `PKG_CONFIG_PATH` in `[activation.env]` to prioritize Pixi libraries during build.
    *   Removed `scripts/pixi-env-clean.sh` usage and deleted the file.
    *   Simplified `start-i686` command to rely on Pixi's RPATH/environment for dependencies, only setting `VK_LAYER_PATH` for the layer manifest.
2.  **Dependencies**: Added `pkg-config` to Pixi dependencies to ensure isolated build configuration discovery.