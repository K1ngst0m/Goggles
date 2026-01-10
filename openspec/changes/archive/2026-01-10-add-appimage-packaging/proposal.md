# add-appimage-packaging

## Why

Goggles relies on an implicit Vulkan capture layer (32-bit + 64-bit) that must be discoverable by the Vulkan loader for Steam/Proton workloads. Steam runtimes commonly sanitize Vulkan layer environment variables (e.g., `VK_LAYER_PATH`), which makes “environment-only injection” unreliable.

We need a packaging approach that works on arbitrary Linux distros and Steam Deck, minimizes user setup, and supports the preferred Steam launch pattern: `goggles -- %command%`.

## What Changes

- Add an AppImage-based distribution for Goggles (viewer + resources).
- Add a one-time, user-level installation flow for Vulkan implicit layer manifests and layer shared libraries (both 64-bit and 32-bit).
- Ensure Steam/Proton compatibility by avoiding reliance on `VK_LAYER_PATH`/`VK_ADD_LAYER_PATH` for layer discovery.
- Provide a stable, documented wrapper behavior for Steam launch options (`goggles -- %command%`) that guarantees `GOGGLES_CAPTURE=1` is set for the launched game process.

## How (High-Level)

- Package the Goggles viewer and assets as an AppImage entrypoint.
- On first run (and on version change), the AppImage entrypoint performs an idempotent “self-install” into the user data directory:
  - Install Vulkan implicit layer manifests into `${XDG_DATA_HOME:-$HOME/.local/share}/vulkan/implicit_layer.d/`.
  - Install layer shared libraries into `${XDG_DATA_HOME:-$HOME/.local/share}/goggles/vulkan-layers/<version>/{x86_64,i386}/`.
  - Generate manifests whose `library_path` points at the installed libraries using absolute paths.
- Runtime injection uses the existing manifest gating approach:
  - The implicit layer exists system-wide/user-wide, but it only activates when `GOGGLES_CAPTURE=1` is present (analogous to how Gamescope gates its WSI layer behind `ENABLE_GAMESCOPE_WSI=1`).

## Impact

- New spec:
  - `packaging` (AppImage distribution + Vulkan layer self-install + Steam compatibility)
- Existing specs referenced (no functional changes required by this proposal):
  - `vk-layer-capture` (layer naming, multi-arch manifests, enable env behavior)
  - `build-system` / `dependency-management` (Pixi-driven builds and reproducibility)
- Expected affected code areas (implementation phase):
  - `scripts/` (new packaging tasks)
  - AppImage entrypoint/wrapper (install + launch orchestration)
  - Documentation (`README.md`, packaging/Steam guide)

## Non-Goals

- System-wide installation (root-required) of manifests or libraries.
- Depending on `VK_LAYER_PATH`, `VK_ADD_LAYER_PATH`, or Steam runtime overrides for layer discovery.
- Introducing a new Vulkan layer mechanism (explicit layers or manual `vkCreateInstance` interposition).

## Open Questions

- Installation location and versioning:
  - **Decision**: Use a versioned install root: `${XDG_DATA_HOME:-$HOME/.local/share}/goggles/vulkan-layers/<version>/...`.
- Uninstall UX:
  - **Decision**: Provide an AppImage wrapper flag `--uninstall-layer` to remove the installed layer for the current AppImage version.
- Updates:
  - **Decision**: Refresh manifests on launch to ensure they point at the active version directory.
