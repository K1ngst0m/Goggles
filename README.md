# Goggles

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/K1ngst0m/Goggles)

A hooking-based frame streaming tool with support for RetroArch shader processing.

| zfast-crt |
| :---: |
| ![showcase_zfast_crt](showcase-zfast-crt.webp) |

| crt-royale |
| :---: |
| ![showcase_crt_royale](showcase-crt-royale.png)|

Goggles captures Vulkan application frames and shares them across processes using Linux DMA-BUF with DRM format modifiers and cross-process GPU synchronization.

```text
┌───────────────────────────────────────┐
│         Target Application            │
│  ┌─────────────┐                      │
│  │  Swapchain  │                      │
│  └──────┬──────┘                      │
│         │ vkQueuePresentKHR           │
│         ▼                             │
│  ┌─────────────────────────────────┐  │
│  │  Capture Layer                  │  │
│  │  Export DMA-BUF                 │  │
│  └──────────────┬──────────────────┘  │
└─────────────────┼─────────────────────┘
                  │
                  │ Unix Socket + Semaphore Sync
                  ▼
┌─────────────────┼─────────────────────┐
│  Goggles Viewer │                     │
│  ┌──────────────┴──────────────────┐  │
│  │  CaptureReceiver                │  │
│  └──────────────┬──────────────────┘  │
│                 ▼                     │
│  ┌─────────────────────────────────┐  │
│  │  VulkanBackend                  │  │
│  │  Import DMA-BUF ──► FilterChain │  │
│  └─────────────────────────────────┘  │
└───────────────────────────────────────┘
```

The filter chain transforms captured DMA-BUF images through a series of shader passes before presenting to the display. It supports RetroArch `.slangp` preset files which define multi-pass post-processing effects (CRT simulation, scanlines, etc.).

```text
                        Shader Preset (.slangp)
                                 │
  ┌──────────────────────────────┼─────────────────────────────┐
  │                              ▼                             │
  │  ┌────────┐     ┌────────┐     ┌────────┐     ┌────────┐   │
  │  │Original│───▶│ Pass 0 │───▶│ Pass 1 │───▶│ Pass N │   │
  │  └────────┘ ┌─▶└────────┘ ┌─▶└────────┘ ┌─▶└────────┘   │
  │       │     │       │      │       │      │       │        │
  │       └─────┴───────┴──────┴───────┴──────┘       ▼        │
  │                                                  Output    │
  └────────────────────────────────────────────────────────────┘
                                                       │
                                                       ▼
                                             ┌───────────────────┐
                                             │ Goggles Swapchain │
                                             └───────────────────┘
```

## Shader Preset Compatibility Database

### Status Key
* **Verified**: Manually inspected; visual output is perfect.
* **Partial**: Compiles and runs; full feature set or parameters pending review.
* **Untested**: Compiles successfully; requires human eyes for visual artifacts.

| Name | Build | Status | Platform | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **crt/crt-royale.slangp** | Pass | Partial | `Mesa: RDNA3` | Full verification pending after the shader parameter controlling support. |
| **crt/zfast-crt.slangp** | Pass | Verified | `Mesa: RDNA3`, `Proprietary: Ada` |  |

*More reports pending validation...*

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

Use `pixi run start [preset] <app> [app_args...]` to launch the viewer and
target together. The preset argument is optional and defaults to `debug`.

```bash
# Quick smoke test (build + manifests as needed)
pixi run start vkcube --wsi xcb           # preset=debug
pixi run start release vkcube --wsi xcb   # preset=release

# Standard flow
pixi run build                   # 1. Build the project
./build/debug/bin/goggles        # 2. Run goggles app (receiver)
GOGGLES_CAPTURE=1 vkcube         # 3. Run target app with capture enabled
```

For Steam games, set launch options:
```
GOGGLES_CAPTURE=1 %command%
```

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
- [Filter Chain](docs/filter_chain_workflow.md) - RetroArch shader pipeline
- [RetroArch](docs/retroarch.md) - Shader preset compatibility
- [Shader Compatibility Report](docs/shader_compatibility.md) - Full compilation status for all RetroArch presets
- [Project Policies](docs/project_policies.md) - Development rules and conventions
- [Roadmap](ROADMAP.md) - Development pending work
