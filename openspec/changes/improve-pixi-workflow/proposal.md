# Change: Improve Pixi Workflow for Worktrees

## Why
Local pixi-build packages require 300MB+ source downloads per worktree because pixi-build caches sources in `.pixi/build/` (per-workspace), not globally. The `vulkansdk` package was unnecessarily large and could be replaced with minimal conda-forge packages. The `slang-shaders` package is intentionally kept local for independent version control.

Additionally, pre-commit hook installation failed in worktrees due to scripts checking `[[ -d .git ]]` which fails when `.git` is a file (worktree behavior).

## What Changes

### 1. Replace local `vulkansdk` with conda-forge packages
- **BREAKING**: Remove `packages/vulkansdk/` local package
- Add conda-forge dependency: `vulkan-validation-layers` (headers already present via `libvulkan-headers`)
- Add activation environment variables: `VULKAN_SDK` and `VK_ADD_LAYER_PATH` in pixi.toml
- **Note**: Shader compilation uses Slang, not glslang/shaderc, so those packages are not needed

### 2. Keep `slang-shaders` as local package
- Slang shader compiler is intentionally managed as a local package for independent version control
- **Note**: Slang is not available on conda-forge (the "slang" package there is S-Lang interpreter, GPL)
- This allows the project to control Slang updates independently from other dependencies
- Workaround for worktrees: symlink `.pixi` directory or use `detached-environments = true` in user config

### 3. Fix pre-commit hook for worktrees (already committed)
- Use `git rev-parse --is-inside-work-tree` instead of `[[ -d .git ]]`
- Use `git rev-parse --git-path hooks` to find hooks directory
- Handle `core.hooksPath` config

### 4. Remove IDE setup scripts (already committed)
- Removed `scripts/setup-ide.sh`
- Removed `scripts/check-ide-setup.sh`
- Pre-commit hook is the enforcement mechanism

## Impact
- Affected specs: `dependency-management`
- Affected code: `pixi.toml`, `packages/vulkansdk/`, `scripts/`
- Benefits:
  - Faster worktree setup (reduced downloads from 313MB to 6MB for Vulkan components)
  - Slang remains independently managed for version control flexibility
  - Improved pre-commit hook compatibility with worktrees
