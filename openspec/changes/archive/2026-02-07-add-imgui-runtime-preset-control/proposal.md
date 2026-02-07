# Change: Runtime ImGui Shader Controls

## Why
Users currently must restart Goggles or pass CLI overrides to experiment with shader presets, and there is no UI to toggle passthrough processing. Integrating an ImGui-based control surface lets us reload presets instantly, inspect active settings, and switch between passthrough and filter chains without touching CLI flags.

## What Changes
- Integrate the latest Dear ImGui docking branch into the Goggles application UI layer (SDL3 window) without adding dependencies to the capture layer.
- Add a dockable "Shader Controls" panel that surfaces the active preset path, enumerates available presets, and emits reload requests for the render pipeline.
- Add a passthrough toggle in the UI that bypasses the filter chain and falls back to direct DMA-BUF blitting when enabled.
- Extend the render pipeline to rebuild the filter chain at runtime when a new preset or passthrough state is selected, handling errors gracefully and falling back to the previous preset when reload fails.

## Impact
- Affected specs: `app-window`, `render-pipeline`
- Affected code: `src/app` (SDL3 + ImGui integration), `src/render/chain`, `src/render/shader`, `config/` load + runtime wiring.
