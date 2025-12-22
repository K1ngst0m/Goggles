# Change: Centralized Version Management System

## Why

Currently, version information is partially scattered:
- `CMakeLists.txt` defines `VERSION 0.1.0` in `project()` directive
- Compile definitions expose `GOGGLES_VERSION` string
- `vulkan_backend.cpp:160-162` hardcodes `VK_MAKE_VERSION(0, 1, 0)` independently

This creates a maintenance risk: when updating the version, developers must remember to update hardcoded values in `vulkan_backend.cpp`, with no mechanism to prevent inconsistency.

## What Changes

- Add three new compile definitions: `GOGGLES_VERSION_MAJOR`, `GOGGLES_VERSION_MINOR`, `GOGGLES_VERSION_PATCH`
- Update `vulkan_backend.cpp` to use macros instead of hardcoded `0, 1, 0`
- Maintain `project(goggles VERSION x.y.z)` as single source of truth

This establishes version propagation using CMake's built-in versioning with compile definitions, following the existing pattern already used for `GOGGLES_VERSION` and `GOGGLES_PROJECT_NAME`.

## Impact

- Affected specs: build-system (new capability)
- Affected code:
  - `CMakeLists.txt` - Add 3 compile definitions for version components
  - `src/render/backend/vulkan_backend.cpp` - Replace hardcoded `0, 1, 0` with macros
- Build impact: None (follows existing compile definition pattern)

## Policy Compliance

This change adheres to project policies:
- **Section C.7**: No new files, no comments needed (follows existing pattern)
- **Section D.2**: Vulkan version usage via `VK_MAKE_VERSION(GOGGLES_VERSION_MAJOR, ...)` macros
- **Section B.1**: No changes to logging (already using macros)
