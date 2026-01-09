# Design: AppImage Packaging + Vulkan Layer Self-Install

## Goals

- Work on arbitrary distros without requiring root installation.
- Work on Steam Deck (Steam Runtime / pressure-vessel).
- Make Vulkan layer injection robust even when Steam sanitizes Vulkan-layer environment variables.
- Keep user UX simple:
  - Steam launch option: `goggles -- %command%`
  - No manual “copy manifest into implicit_layer.d” step.

## Key Observation (Gamescope Pattern)

Gamescope ships a Vulkan implicit layer manifest in the standard loader search path and gates activation behind a dedicated environment variable set by its launcher. This avoids needing to set `VK_LAYER_PATH` in the environment.

Goggles already uses the same gating strategy (`enable_environment` uses `GOGGLES_CAPTURE=1`). The missing piece is making the manifest+layer library reliably discoverable across Steam runtimes.

## Approach

### 1) Distribution Artifact: AppImage

The AppImage contains:
- `goggles` viewer executable (64-bit).
- Resources needed by the viewer (config defaults, shaders if bundled, etc.).

The AppImage entrypoint (AppRun) is the user-facing `goggles` command.

### 2) One-time user-level layer install (idempotent)

Because an AppImage’s mounted runtime path is not stable across runs, the Vulkan loader cannot reliably load a layer library from inside the AppImage via a manifest that points into the mounted image.

Therefore, on first run (and on version update), we install to stable user paths:

- Data root: `${XDG_DATA_HOME:-$HOME/.local/share}`
- Manifests:
  - `${data_root}/vulkan/implicit_layer.d/goggles_layer_x86_64.json`
  - `${data_root}/vulkan/implicit_layer.d/goggles_layer_i386.json`
- Layer libraries:
  - `${data_root}/goggles/vulkan-layers/<version>/x86_64/libgoggles_vklayer.so`
  - `${data_root}/goggles/vulkan-layers/<version>/i386/libgoggles_vklayer.so`
- Version marker:
  - `${data_root}/goggles/vulkan-layers/<version>/.installed`

Manifests use absolute `library_path` to point at the installed library path.

### 3) Steam-safe injection model

We do not rely on Vulkan-layer path environment variables.

Instead:
- The Vulkan loader discovers the implicit layer via the user manifest directory.
- The layer is activated only when `GOGGLES_CAPTURE=1` is set for the game process.
- The Goggles wrapper ensures `GOGGLES_CAPTURE=1` is set when it spawns the target command (including `%command%` in Steam).

### 4) Multi-arch behavior (Proton / 32-bit)

We install both 64-bit and 32-bit manifests + libraries.

Expected behavior:
- Native 64-bit Vulkan apps load `VK_LAYER_goggles_capture_64` when `GOGGLES_CAPTURE=1`.
- 32-bit apps (including Wine/DXVK) load `VK_LAYER_goggles_capture_32` when `GOGGLES_CAPTURE=1`.

The manifests must be valid for the Vulkan loader used inside Steam Runtime containers and must reference libraries on paths visible inside the container (home is typically bind-mounted).

## Failure Modes + Mitigations

- **Steam runtime strips `VK_LAYER_PATH`**: Not relevant; we rely on implicit layer discovery.
- **Steam runtime does not see the installed library path**: Choose install location under `$HOME`/`XDG_DATA_HOME`, which Steam generally bind-mounts.
- **Layer install races (two launches)**: Make self-install idempotent and atomic (write temp files then rename).
- **Updates leave stale manifests**: Install manifests as part of version update; optionally keep only one “active” version by rewriting manifests to the newest installed version.

## Alternatives Considered

### Alternative A: Run without self-install by setting `XDG_DATA_DIRS` from AppRun

This can work on some systems, but it is not guaranteed under Steam/pressure-vessel and still faces the “library inside AppImage mount” instability unless the manifest can reference a stable path.

Given the preference for a one-time self-install, we choose self-install as the primary path.

