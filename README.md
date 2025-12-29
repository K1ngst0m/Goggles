# Goggles

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/K1ngst0m/Goggles)

A hooking-based frame streaming tool with support for RetroArch shader processing.

| zfast-crt |
| :---: |
| ![showcase_zfast_crt](showcase-zfast-crt.webp) |

| crt-royale |
| :---: |
| ![showcase_crt_royale](showcase-crt-royale.png)|

Goggles captures Vulkan application frames and shares them across processes using Linux DMA-BUF with DRM format modifiers. 

```
┌────────────────────────────────────────────────────────────────────────┐
│                         Target Application                             │
│  ┌─────────────┐                                                       │
│  │  Swapchain  │ ◄── Application renders frames here                   │
│  │   Images    │                                                       │
│  └──────┬──────┘                                                       │
│         │ vkQueuePresentKHR (intercepted)                              │
│         ▼                                                              │
│  ┌─────────────────────────────────────────────────────────────────┐   │
│  │                    Capture Layer (vk_capture)                   │   │
│  │  ┌────────────┐    ┌────────────┐    ┌────────────────────────┐ │   │
│  │  │  Export    │    │  Copy to   │    │  Send via Unix Socket  │ │   │
│  │  │  Image     │◄───│  Export    │───►│  (SCM_RIGHTS)          │ │   │
│  │  │  (DMA-BUF) │    │  Image     │    │  fd + metadata         │ │   │
│  │  └────────────┘    └────────────┘    └────────────────────────┘ │   │
│  └─────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────┘
                                    │
                                    │ Unix Domain Socket
                                    │ (DMA-BUF fd via SCM_RIGHTS)
                                    ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                         Goggles Viewer                                  │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    CaptureReceiver                               │   │
│  │  Receives fd + format + stride + modifier                        │   │
│  └──────────────────────────┬───────────────────────────────────────┘   │
│                             │                                           │
│                             ▼                                           │
│  ┌──────────────────────────────────────────────────────────────────┐   │
│  │                    VulkanBackend (import_dmabuf)                 │   │
│  │  ┌────────────┐    ┌────────────┐    ┌────────────────────────┐  │   │
│  │  │  Import    │    │  Create    │    │  Sample in             │  │   │
│  │  │  Memory    │───►│  VkImage   │───►│  Fragment Shader       │  │   │
│  │  │  (DMA-BUF) │    │            │    │                        │  │   │
│  │  └────────────┘    └────────────┘    └────────────────────────┘  │   │
│  └──────────────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────────────┘
```


The filter chain transforms captured DMA-BUF images through a series of shader passes before presenting to the display. It supports RetroArch `.slangp` preset files which define multi-pass post-processing effects (CRT simulation, scanlines, etc.).

```
┌─────────────┐     ┌──────────────┐     ┌──────────────┐     ┌───────────┐
│  Captured   │────▶│  FilterPass  │────▶│  FilterPass  │────▶│ Swapchain │
│  DMA-BUF    │     │   (Pass 0)   │     │   (Pass N)   │     │  Output   │
└─────────────┘     └──────────────┘     └──────────────┘     └───────────┘
                           │                    │
                           ▼                    ▼
                    ┌─────────────┐      ┌─────────────┐
                    │ Framebuffer │      │   (final)   │
                    │    (0)      │      │  no buffer  │
                    └─────────────┘      └─────────────┘
```

## Shader Preset Compatibility Database

### Status Key
* **Verified**: Manually inspected; visual output is perfect.
* **Partial**: Compiles and runs; full feature set or parameters pending review.
* **Untested**: Compiles successfully; requires human eyes for visual artifacts.

| Name | Build | Status | Platform | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **crt/crt-royale.slangp** | Pass | Partial | `Mesa: RDNA3` | Full verification pending after the shader parameter controlling support. |
| **crt/zfast-crt.slangp** | Pass | Verified | `Mesa: RDNA3`, `Proprietary: Ada` | Verified; visual output matches expected quality on RDNA3. |

*More reports pending validation...*

## Build

```bash
cmake --list-presets    # List available presets
make app                # Build (debug by default)
make help               # See all targets
```

Build output:
```
build/<preset>/
├── bin/goggles
├── lib/x86_64/libgoggles_vklayer.so
└── share/vulkan/implicit_layer.d/
```

## Usage

```bash
# 1. Build and install layer manifests
make dev

# 2. Run goggles app (receiver)
./build/debug/bin/goggles

# 3. Run target app with capture enabled
GOGGLES_CAPTURE=1 vkcube
```

For Steam games, set launch options:
```
GOGGLES_CAPTURE=1 %command%
```

### RetroArch Shaders

The repository tracks minimal zfast-crt shaders. For the full shader collection:

```bash
./scripts/download_retroarch_shaders.sh
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
