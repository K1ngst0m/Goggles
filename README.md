# Goggles

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/K1ngst0m/Goggles)

A hooking-based frame streaming tool with support for RetroArch shader processing.

| zfast-crt |
| :---: |
| ![showcase_zfast_crt](showcase-zfast-crt.webp) |

| crt-royale |
| :---: |
| ![showcase_crt_royale](showcase-crt-royale.png)|

Goggles captures Vulkan frames, runs a shader filter chain, and can optionally forward input via a
nested compositor.

## Shader Preset Compatibility Database

### Status Key
* **Verified**: Manually inspected; visual output is perfect.
* **Partial**: Compiles and runs; full feature set or parameters pending review.
* **Untested**: Compiles successfully; requires human eyes for visual artifacts.

| Name | Build | Status | Platform | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **crt/crt-royale.slangp** | Pass | Partial | `Mesa: RDNA3` | Full verification pending after the shader parameter controlling support. |
| **crt/zfast-crt.slangp** | Pass | Verified | `Mesa: RDNA3`, `Proprietary: Ada` |  |

- [Shader Compatibility Report](docs/shader_compatibility.md) - Full compilation status for all RetroArch presets

## Build

This project uses [Pixi](https://pixi.sh) for dependency management and build tasks.

```bash
pixi run help # view all available tasks and their descriptions
pixi run <task-name> [args]... # run a task
```

Build output:
```
build/<preset>/
├── bin/goggles
├── lib/x86_64/libgoggles_vklayer.so
└── share/vulkan/implicit_layer.d/
```

## Usage

Use `pixi run start [-p preset] [goggles_args...] -- <app> [app_args...]` to launch the viewer and
target together. The `--` separator is required so app arguments (like `--config`) don't get parsed
as Goggles options. The preset defaults to `debug`.

```bash
# Quick smoke tests (build + manifests as needed)
pixi run start -- vkcube --wsi xcb                            # preset=debug
pixi run start -p release -- vkcube --wsi xcb                 # preset=release
pixi run start -p profile --app-width 480 --app-height 240 -- vkcube --wsi xcb

# Viewer-only mode (manual target launch)
pixi run dev                                 # 1. Build the project
./build/debug/bin/goggles --detach           # 2. Run goggles viewer
GOGGLES_CAPTURE=1 vkcube --wsi xcb           # 3. Run target app with capture enabled
```

In default mode, Goggles exits when the target app exits. If the viewer window is closed early,
Goggles terminates the launched target process.

For Steam games, prefer a wrapper that launches the game through Goggles:
`goggles -- %command%`.

### RetroArch Shaders

```bash
pixi run shader-fetch            # Download/refresh full RetroArch shaders into shaders/retroarch
```

This downloads from [libretro/slang-shaders](https://github.com/libretro/slang-shaders). All shaders except zfast-crt are gitignored.

## Documentation

See [docs/architecture.md](docs/architecture.md) for project architecture and design.

Topic-specific docs:
- [Threading](docs/threading.md) - Concurrency model and job system
- [DMA-BUF Sharing](docs/dmabuf_sharing.md) - Cross-process GPU buffer sharing
- [Input Forwarding](docs/input_forwarding.md) - Forward keyboard/mouse to captured apps
- [Filter Chain](docs/filter_chain_workflow.md) - RetroArch shader pipeline
- [RetroArch](docs/retroarch.md) - Shader preset compatibility
- [Shader Compatibility Report](docs/shader_compatibility.md) - Full compilation status for all RetroArch presets
- [Project Policies](docs/project_policies.md) - Development rules and conventions
- [Roadmap](ROADMAP.md) - Development pending work
