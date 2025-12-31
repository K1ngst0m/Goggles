# Change: Enforce Pixi Toolchain Version Consistency

## Why

Several dev tools in pixi.toml use `*` (unpinned), risking inconsistent builds across machines and CI. System tools can leak in when versions aren't controlled. Need consistent, reproducible dev environment.

## What Changes

- Pin all unpinned dev tools to specific versions in pixi.toml
- Ensure build tools (cmake, ninja, ccache, lld) have consistent versions
- Libraries remain as-is (already pinned or intentionally flexible)

## Tool Audit

### Currently Pinned (OK)
| Tool | Version | Purpose |
|------|---------|---------|
| clang-tools | ==21.1.6 | clang-tidy, clang-format |
| sysroot_linux-64 | 2.28.* | glibc compat |
| pkg-config | >=0.29.2,<0.30 | Dependency discovery |
| spdlog | 1.15.* | Logging lib |
| catch2 | 3.8.* | Testing lib |
| toml11 | 4.4.* | TOML parsing lib |
| libvulkan-headers | 1.4.328.* | Vulkan API |
| vulkan-validation-layers | 1.4.328.* | Vulkan debug |
| cli11 | 2.6.* | CLI parsing lib |
| taplo (lint env) | ==0.9.3 | TOML formatter |

### Unpinned - Needs Pinning
| Tool | Current | Recommend | Reasoning |
|------|---------|-----------|-----------|
| cmake | * | 3.31.* | Latest 3.x LTS; 4.x too new, breaks third-party cmake_minimum_required(3.x) |
| ninja | * | 1.12.* | Proven stable; 1.13 available but no benefit |
| clang_linux-64 | * | 21.* | Must match clang-tools (21.1.6) for consistent diagnostics |
| clangxx_linux-64 | * | 21.* | Must match clang-tools |
| lld | * | 21.* | Must match clang for ABI compatibility |
| ccache | * | 4.* | Stable cache format within major version |
| taplo (default) | * | ==0.9.3 | Match lint env for consistent formatting |
| sdl3 | * | 3.2.* | SDL3 is new (2024), pin minor to avoid API breaks |

### Intentionally Unpinned (Exception)
| Tool | Reason |
|------|--------|
| c-compiler / cxx-compiler | Meta-packages, version via clang_* |
| xorg-* / wayland / audio libs | System compat, low churn |

## Impact

- Affected specs: build-system
- Affected files: pixi.toml
