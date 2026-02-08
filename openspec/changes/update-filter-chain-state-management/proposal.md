# Change: Update filter chain state management and stage gating

## Why
Filter chain enablement is currently controlled from multiple entry points across `Application`, `VulkanBackend`, and `FilterChain`. This allows stage state to drift during async chain swaps and resize transitions, which causes incorrect behavior: global OFF still applying parts of the chain, re-enable paths reapplying unexpectedly, and unstable prechain downsampling results.

Startup behavior is also non-deterministic in Vulkan-layer mode. The per-surface default, compositor maximize requests, and prechain target initialization currently depend on transient frame arrival order, which can produce inconsistent outcomes across identical launches (for example `500x500` vs `1920x1080` source/prechain combinations and scratched output).

## What Changes
- Introduce a single resolved runtime policy for filter chain gating that combines the three GUI switches:
  - `Application -> Window Management -> Filter Chain (All Surfaces)` (global)
  - `Application -> Window Management -> Surface List` per-surface toggle
  - `Shader Controls -> Effect Stage -> Enable Shader` (effect stage only)
- Route policy application through one backend API so prechain/effect state updates are atomic and consistent per frame.
- Reapply active policy after filter-chain reinitialization and async chain swaps so newly created chain instances never fall back to default stage state.
- Harden prechain resource/resolution management to avoid stale downsampling resources when toggling or resizing.
- Make startup deterministic by resolving capture mode/session policy before applying per-surface defaults and resize policy.
- In direct Vulkan capture sessions, initialize default prechain target from viewer swapchain extent (option `1`) instead of first observed source-frame extent.
- Gate compositor resize requests to policy transitions so maximize/restore does not oscillate during startup.
- Keep postchain output blit as the mandatory presentation path even when global filter chain is OFF; global OFF bypasses prechain and effect stage only.

## Impact
- Affected specs: render-pipeline, app-window
- Affected code: `src/app/application.cpp`, `src/app/application.hpp`, `src/render/backend/vulkan_backend.cpp`, `src/render/backend/vulkan_backend.hpp`, `src/render/chain/filter_chain.cpp`, `src/render/chain/filter_chain.hpp`, `src/ui/imgui_layer.cpp`, `src/ui/imgui_layer.hpp`
