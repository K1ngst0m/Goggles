## 1. Runtime Policy Consolidation

- [x] 1.1 Keep a centralized filter-chain policy resolver in `src/app/application.cpp` / `src/app/application.hpp` that combines global, per-surface, and effect-stage toggles.
- [x] 1.2 Remove remaining split stage-update paths and enforce a single backend policy update API in `src/render/backend/vulkan_backend.hpp` / `src/render/backend/vulkan_backend.cpp`.
- [x] 1.3 Ensure UI callbacks in `src/ui/imgui_layer.cpp` only mutate the three control inputs and never apply stage policy directly.

## 2. Deterministic Startup Behavior

- [x] 2.1 Add stable session capture-mode state in `Application` and stop inferring per-surface defaults from transient frame-arrival timing.
- [x] 2.2 In direct Vulkan capture sessions, initialize default prechain target from viewer swapchain extent when no explicit prechain target is configured.
- [x] 2.3 Preserve per-surface user override state so auto-default logic never overwrites manual toggle selections.
- [x] 2.4 Gate compositor maximize/restore requests by effective policy transitions (and surface topology changes) to prevent startup resize oscillation.

## 3. Backend and FilterChain Correctness

- [x] 3.1 Persist active filter-chain policy in backend runtime state and reapply it after `init_filter_chain()` and async chain swap paths.
- [x] 3.2 Keep requested vs resolved prechain resolution state separated in `src/render/chain/filter_chain.cpp` / `src/render/chain/filter_chain.hpp` and rebuild resources only on basis changes.
- [x] 3.3 Ensure frame-history source selection tracks the actually executed stage path to avoid stale prechain downsampling artifacts.

## 4. Verification

- [x] 4.1 Automated: `pixi run build -p debug && pixi run build -p test && ctest --preset test -R "filter_chain|application" --output-on-failure`.
- [x] 4.2 Manual: global OFF bypasses prechain/effect for all surfaces while postchain output still presents.
- [x] 4.3 Manual: global ON + per-surface OFF bypasses only selected surface; global ON + effect OFF bypasses effect stage only.
- [x] 4.4 Manual: run `pixi run start -p debug -- vkcube` repeatedly (>=10 launches); startup behavior is deterministic with consistent app extent/prechain target pairing.
- [x] 4.5 Manual: toggle during async preset reload and resize transitions does not reapply default stage state or produce prechain downsampling artifacts.
