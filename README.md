# Goggles

A hooking-based frame streaming tool with support for RetroArch shader processing.

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
