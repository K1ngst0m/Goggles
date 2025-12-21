# Goggles

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

## Documentation

See [docs/architecture.md](docs/architecture.md) for project architecture and design.

Topic-specific docs:
- [Threading](docs/threading.md) - Concurrency model and job system
- [DMA-BUF Sharing](docs/dmabuf_sharing.md) - Cross-process GPU buffer sharing
- [Filter Chain](docs/filter_chain_workflow.md) - RetroArch shader pipeline
- [RetroArch](docs/retroarch.md) - Shader preset compatibility
- [Project Policies](docs/project_policies.md) - Development rules and conventions
