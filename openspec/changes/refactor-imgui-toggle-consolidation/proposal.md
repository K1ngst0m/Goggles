# Change: Consolidate ImGui Toggle Keys to Single Hotkey

## Why

The current design uses multiple function keys (F1-F4) for ImGui layer controls:
- F1: Toggle shader controls window
- F2: Toggle debug overlay
- F3: Toggle pointer lock override
- F4: Toggle application management window

This creates conflicts with games that use F1-F4 for their own functions (save/load, menus, quicksave, etc.). Since goggles forwards input to the target application, every hotkey we consume is a potential conflict.

## What Changes

- **BREAKING**: Remove all F1-F4 shortcuts
- Single Ctrl+Alt+Shift+Q toggles all Goggles Overlay visibility (master switch)
- Two windows (dockable, can be tabbed together by user):
  - **Shader Controls**: Pre-chain, effect, and post-chain shader settings only
  - **Application**: Performance stats, pointer lock, surface selector
- Debug overlay content (FPS graphs, frame times) moved INTO Application as "Performance" section
- "Force Enable Pointer Lock" checkbox with hint showing toggle shortcut when enabled
- Surface selector remains in Application window (under "Input" section)

## Impact

- Affected specs: `input-forwarding` (hotkey behavior)
- Affected code: `src/app/application.cpp`, `src/ui/imgui_layer.cpp`, `src/ui/imgui_layer.hpp`
- User workflow: Press Ctrl+Alt+Shift+Q to show/hide Goggles Overlay
- Migration: Users learn new modifier-key workflow instead of F1/F2/F3/F4

## UI Structure

```
Ctrl+Alt+Shift+Q toggles Goggles Overlay
├── Shader Controls (window, dockable)
│   ├── Pre-Chain Stage
│   ├── Effect Stage
│   └── Post-Chain Stage
└── Application (window, dockable)
    ├── Performance (collapsible)
    │   ├── Render FPS graph
    │   └── Source FPS graph
    └── Input (collapsible)
        ├── [x] Force Enable Pointer Lock
        │   └── (hint: "Press Ctrl+Alt+Shift+Q to toggle overlay")
        └── Surface list + Reset to Auto
```

## Alternatives Considered

1. **Single F1 key**: Still conflicts with many games (F1 = help is common)
2. **Mouse chord (middle+right click)**: No keyboard conflict but less discoverable
3. **Edge hotspot (move to corner)**: Works but can interfere with gameplay
