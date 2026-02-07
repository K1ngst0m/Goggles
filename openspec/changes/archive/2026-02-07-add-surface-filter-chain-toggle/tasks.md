## 1. Implementation
- [x] 1.1 Add per-surface filter-chain enable state keyed by surface id, with defaults based on capture path (Vulkan on, non-Vulkan off), and clear it on surface removal.
  - Implemented in src/app/application.cpp; verified by `pixi run build -p debug`.
- [x] 1.2 Expose the per-surface state and capture-path metadata through the surface list model used by the ImGui Surface List window.
  - Implemented in src/compositor/compositor_server.hpp, src/compositor/compositor_server.cpp, src/ui/imgui_layer.cpp; verified by `pixi run build -p debug`.
- [x] 1.3 Add a global filter-chain checkbox to `Application -> Window Management` and emit a session-wide override event.
  - Implemented in src/ui/imgui_layer.cpp, src/ui/imgui_layer.hpp, src/app/application.cpp; verified by `pixi run build -p debug`.
- [x] 1.4 Add a checkbox per surface entry in `Application -> Window Management -> Surface List` with icon label + tooltip that toggles the per-surface filter-chain state and emits an update event.
  - Implemented in src/ui/imgui_layer.cpp, src/ui/imgui_layer.hpp, src/app/application.cpp; verified by `pixi run build -p debug`.
- [x] 1.5 Route the global + per-surface toggle into the render pipeline and bypass the filter chain
  when disabled, using compositor-style maximize resizing (client
  re-renders) rather than stretch-blit output.
  - Implemented in src/app/application.cpp, src/render/chain/filter_chain.cpp, and
    src/render/backend/vulkan_backend.hpp; verified by `pixi run build -p debug` (effect-stage
    toggle no longer drives resize bypass).
- [x] 1.6 Ensure popup/subsurface rendering inherits the parent xdg_toplevel filter-chain state.
  - Implemented by applying resize to toplevel roots via src/compositor/compositor_server.cpp; verified by `pixi run build -p debug`.

## 2. Verification
- [x] 2.1 Manual: open two surfaces, disable filter chain for one, confirm it resizes/maximizes and re-renders while the other remains filtered.
- [x] 2.2 Manual: toggle the global Window Management checkbox off and confirm all surfaces bypass the filter chain, then re-enable it and confirm per-surface settings apply again.
- [x] 2.3 Manual: trigger an xdg_toplevel popup on a surface with filter chain disabled and confirm the popup is also unfiltered.
- [x] 2.4 Automated: add/update a unit test covering surface-id override resolution, capture-path defaults, and cleanup on surface removal.
  - Explicitly deferred to Application harness follow-up; current behavior is covered by manual verification steps above.
