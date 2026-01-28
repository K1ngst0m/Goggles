# Change: Add Runtime Scale Mode Switching

## Why

Scale mode is currently split between application state and the Vulkan backend, which prevents runtime switching and makes dynamic resolution requests depend on stale state. Consolidating scale mode ownership in the backend and exposing runtime controls in the Pre-Chain UI enables live tuning without restarting the viewer and ensures dynamic mode requests always reflect the active scale mode.

## What Changes

- Make the Vulkan backend the single source of truth for the active render scale mode (and integer scale) with read/write accessors
- Route application dynamic-resolution requests through the backend’s current scale mode
- Add a scale mode selector to the existing Pre-Chain stage controls in the ImGui shader controls window
- Apply scale mode changes immediately to subsequent frames without restarting the viewer

## Impact

- Affected specs: `render-pipeline`
- Affected code:
  - `src/app/application.hpp/cpp` (dynamic resolution request logic)
  - `src/render/backend/vulkan_backend.hpp/cpp` (scale mode ownership + accessors)
  - `src/ui/imgui_layer.hpp/cpp` (Pre-Chain UI controls + callbacks)
  - `src/render/chain/filter_chain.hpp/cpp` (runtime scale mode propagation)
