# Change: Add Shader Stage Controls to ImGui Window

## Why

The shader controls window currently only exposes RetroArch effect stage settings. The pre-chain stage (downsample pass) has hardcoded resolution that can only be set via CLI flags at startup. Runtime control over pre-chain resolution would enable interactive tuning without restarting the application.

## What Changes

- Restructure shader controls window into three collapsible sections: Pre-Chain, Effect, Post-Chain
- Add UI controls for pre-chain downsample resolution (width/height inputs)
- Add callback infrastructure to propagate pre-chain settings to FilterChain at runtime
- FilterChain exposes method to update pre-chain resolution dynamically
- Post-chain section shows placeholder (no controls for now)

## Impact

- Affected specs: `render-pipeline` (pre-chain runtime configuration)
- Affected code:
  - `src/ui/imgui_layer.hpp/cpp` - UI state and drawing
  - `src/render/chain/filter_chain.hpp/cpp` - runtime pre-chain update
  - `src/app/application.cpp` - callback wiring
