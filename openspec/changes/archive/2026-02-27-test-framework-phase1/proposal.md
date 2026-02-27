## Why

Goggles has 27 unit tests and no automated visual validation. 30+ manual test scenarios exist (aspect ratio, shaders, surface composition, filter chain) that are exercised by hand on every change. The headless mode (PR #96) provides the execution primitive needed to drive a real GPU pipeline in CI without a display — Phase 1 builds the infrastructure layer that all visual tests will depend on.

## What Changes

- **New**: `tests/clients/` — four deterministic Wayland/`wl_shm` test client apps rendering known solid-color patterns (`solid_color_client`, `gradient_client`, `quadrant_client`, `multi_surface_client`)
- **New**: `tests/visual/image_compare.hpp/.cpp` — image comparison library with fuzzy per-channel tolerance, `CompareResult` struct, and diff-image generation
- **New**: `tests/visual/image_compare_cli.cpp` — standalone CLI comparison tool (`goggles_image_compare`)
- **New**: `headless_smoke` — CTest integration test running the full headless pipeline against `solid_color_client`
- **Modified**: `tests/CMakeLists.txt` — unconditional inclusion of `tests/clients/` and `tests/visual/`, headless smoke tests

## Capabilities

### New Capabilities

- `test-client-apps`: Deterministic Wayland test clients rendering known pixel patterns via `wl_shm`; used as target apps for headless visual tests
- `visual-regression`: Image comparison library and CLI tool providing fuzzy per-channel pixel comparison, configurable tolerance thresholds, and diff-image visualization

### Modified Capabilities

- `build-system`: Test clients and image comparison library build unconditionally alongside the main project — no separate option or preset needed since all dependencies (wayland-client, stb_image, Catch2) are already project requirements

## Impact

- **New code**: `tests/clients/` (~4 C++ files), `tests/visual/` (~3 C++ files + header)
- **CMake**: root `CMakeLists.txt`, `CMakePresets.json`, `tests/CMakeLists.txt`, new `tests/clients/CMakeLists.txt`, `tests/visual/CMakeLists.txt`
- **CTest labels**: adds `visual` and `integration` (smoke) test labels alongside existing `unit`
- **Dependencies**: `stb_image_write` and `stb_image` already vendored in `packages/stb` — no new external dependencies for Phase 1
- **Policy-sensitive**: image comparison library uses `tl::expected<T, Error>` for fallible I/O (PNG read/write); test client apps use RAII for `wl_display`/`wl_surface` handles; no threading in pipeline code
- **No breaking changes** to any existing code or APIs
