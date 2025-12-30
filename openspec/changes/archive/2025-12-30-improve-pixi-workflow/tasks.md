## 1. Pre-commit Hook Fix (Completed)
- [x] 1.1 Update `scripts/install-pre-commit-hook.sh` to use `git rev-parse`
- [x] 1.2 Update `scripts/ensure-init.sh` to use `git rev-parse`
- [x] 1.3 Update `scripts/pre-commit-format.sh` to use `pixi run` for tools
- [x] 1.4 Remove `scripts/setup-ide.sh`
- [x] 1.5 Remove `scripts/check-ide-setup.sh`
- [x] 1.6 Simplify `scripts/init-format-setup.sh`
- [x] 1.7 Remove `setup-ide` task from `pixi.toml`

## 2. Replace vulkansdk with conda-forge
- [x] 2.1 Identify required components from vulkansdk
- [x] 2.2 Add conda-forge packages to pixi.toml dependencies
- [x] 2.3 Add activation script for VK_ADD_LAYER_PATH in pixi.toml
- [x] 2.4 Remove `packages/vulkansdk/` directory
- [x] 2.5 Update pixi.toml to remove vulkansdk path dependency
- [x] 2.6 Test build and validation layers work

## 3. Evaluate slang-shaders
- [x] 3.1 Check if Slang is available on conda-forge
  - Result: NOT available. conda-forge "slang" is S-Lang (GPL interpreter), not Slang shader compiler
- [x] 3.2 Keep local package, document workaround (symlink `.pixi` for worktrees)

## 4. Documentation
- [x] 4.1 Worktree limitation documented in proposal.md
- [x] 4.2 Slang not on conda-forge - local package remains
