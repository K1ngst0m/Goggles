# Vulkan Layer Capture Usage

This document describes how to use the Goggles Vulkan capture layer.

## Overview

The Goggles capture layer intercepts Vulkan applications and captures their rendered frames via DMA-BUF export. The captured frames are sent to the Goggles application via Unix domain socket for display.

## Quick Start

### 1. Build

```bash
cmake --preset=debug
cmake --build build/debug
```

### 2. Run Test Script

```bash
./scripts/test_vkcube_capture.sh
```

This script:
1. Starts the Goggles application (listens for captured frames)
2. Runs vkcube with the capture layer enabled
3. You should see vkcube's output mirrored in the Goggles window

### 3. Manual Usage

**Terminal 1 - Start Goggles:**
```bash
./build/debug/src/app/goggles
```

**Terminal 2 - Run target application with layer:**
```bash
export VK_LAYER_PATH="$(pwd)/build/debug"
export VK_INSTANCE_LAYERS="VK_LAYER_goggles_capture"
export GOGGLES_CAPTURE=1

vkcube
```

## Environment Variables

| Variable | Description |
|----------|-------------|
| `VK_LAYER_PATH` | Directory containing `goggles_layer.json` and `goggles_vklayer.so` |
| `VK_INSTANCE_LAYERS` | Set to `VK_LAYER_goggles_capture` to enable the layer |
| `GOGGLES_CAPTURE` | Set to `1` to enable capture (layer checks this) |
| `GOGGLES_CAPTURE_DISABLE` | Set to `1` to force-disable capture |

## Architecture

```
┌─────────────────────────────┐     Unix Socket      ┌─────────────────────────────┐
│ Target App (e.g., vkcube)   │    (SCM_RIGHTS)     │ Goggles App                 │
│                             │                      │                             │
│  Vulkan API calls           │                      │  Socket Server              │
│       ↓                     │                      │       ↓                     │
│  goggles_vklayer.so         │ ──────────────────→ │  Receive DMA-BUF fd         │
│       ↓                     │    DMA-BUF fd +     │       ↓                     │
│  GPU copy to export image   │    metadata         │  mmap for CPU access        │
│       ↓                     │                      │       ↓                     │
│  DMA-BUF export             │                      │  SDL_UpdateTexture          │
│                             │                      │       ↓                     │
└─────────────────────────────┘                      │  Render to window           │
                                                     └─────────────────────────────┘
```

## Files

| File | Description |
|------|-------------|
| `config/goggles_layer.json` | Vulkan layer manifest |
| `build/debug/goggles_vklayer.so` | Layer shared library |
| `build/debug/src/app/goggles` | Goggles application |

## Troubleshooting

### Layer not loading

1. Check `VK_LAYER_PATH` points to directory with `goggles_layer.json`
2. Verify `VK_INSTANCE_LAYERS=VK_LAYER_goggles_capture`
3. Check layer with `vulkaninfo --summary`

### No frames captured

1. Ensure Goggles app is started first (socket must be listening)
2. Check `GOGGLES_CAPTURE=1` is set
3. Look for connection messages in app output

### Black screen in Goggles

1. DMA-BUF export may have failed (check driver support)
2. Format mismatch between swapchain and SDL texture
3. Check app logs for mmap errors

## Limitations (Current Prototype)

- Single swapchain only
- LINEAR tiling required for mmap (no optimal tiling)
- CPU copy on app side (not zero-copy)
- No format conversion
- No DRM modifier support
