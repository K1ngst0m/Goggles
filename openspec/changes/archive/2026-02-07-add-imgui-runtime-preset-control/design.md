## Context
Goggles currently loads a shader preset once at startup from `config/goggles.toml` or via the CLI and cannot change it until restart. There is no user interface beyond CLI flags, and passthrough requires editing the config to clear the preset path. We want to add a Dear ImGui-powered dockable UI so users can browse presets, trigger live reloads, and toggle passthrough without impacting the capture layer.

## Goals / Non-Goals
- Goals:
  - Integrate the Dear ImGui docking branch into the SDL3/Vulkan app UI so we can render control panels.
  - Provide an ImGui "Shader Controls" dock that enumerates presets, shows the active selection, and tells the render pipeline to reload or bypass the filter chain.
  - Allow passthrough toggling at runtime with zero shader passes and safe fallback to the previous preset.
- Non-Goals:
  - No functionality inside the Vulkan capture layer; it must remain dependency-free.
  - No persistence UX beyond reusing the existing config defaults (persisting selections is optional future work).
  - No generalized file picker; we only need preset discovery from known directories for now.

## Decisions
1. **Dear ImGui dependency scope** – Use the latest docking tag (`v1.91+ docking`) vendored or fetched via CPM, but link it solely with the Goggles app target. The capture layer build remains unchanged and never links ImGui symbols.
2. **UI integration** – Hook ImGui into the SDL3 event pump and Vulkan swapchain path used by the app window. ImGui draw data is rendered after the filter chain output pass each frame so it overlays results without altering the captured frame texture.
3. **Preset discovery** – Walk the `shader` section of `config/goggles.toml` plus the `shaders/` directory tree at startup to create a preset catalog (relative + absolute paths). Expose this catalog to the UI and refresh it on demand via a "Rescan" action.
4. **Reload protocol** – Introduce a `PresetSelection` channel between the UI thread (main thread) and the render pipeline that carries `{path, passthrough_flag}`. When a new selection arrives, the filter chain waits until the current frame completes, destroys existing passes, and builds the new chain using the requested path (if passthrough is false). Failures leave the previous chain intact and surface an ImGui error toast.
5. **Passthrough handling** – Store the last successful preset metadata so we can restore it when passthrough is toggled off. When passthrough is on (or preset path empty), the filter chain routes DMA-BUFs directly to `OutputPass` without instantiating intermediate passes.

## Risks / Trade-offs
- **UI perf impact** – ImGui adds CPU/GPU overhead. Mitigate by drawing once per frame with docking disabled when hidden.
- **Reload latency** – Rebuilding filter chains can stutter. Solution: perform rebuild between frames and double-buffer chain state so the previous chain finishes before swapping pointers.
- **Preset parsing errors** – Invalid presets triggered via UI could leave the system without a working chain. We keep the previous preset alive until the new one succeeds and surface errors to the UI/log.

## Async Shader Reload (Implemented)
9. **Async compilation** – `reload_shader_preset()` spawns a background task via `JobSystem::submit()` to create a new ShaderRuntime and FilterChain, compile shaders, and load the preset. The main thread continues rendering with the old chain.
10. **Pending chain swap** – When the async task completes, it sets `m_pending_chain_ready` (atomic). The render loop calls `check_pending_chain_swap()` each frame to detect completion and swap in the new chain.
11. **Deferred destruction** – Old chains are queued for deferred destruction (`m_deferred_destroys` fixed-size array) to ensure in-flight frames complete before resources are freed. Cleanup happens in `cleanup_deferred_destroys()` called each frame.
12. **UI notification** – `consume_chain_swapped()` (atomic exchange) signals when a swap occurred so the main loop can update UI parameters with the new chain's parameter list.
13. **Shutdown safety** – `shutdown()` waits for pending async tasks with a 3-second timeout before proceeding with resource cleanup.

## Migration Plan
1. Vendor/fetch ImGui docking sources and wire them into the app build.
2. Add ImGui initialization, frame begin/end hooks, and docking layout persisted under `~/.config/goggles/` (optional) or session memory.
3. Implement the preset catalog + UI controls and connect them to a `PresetSelection` dispatcher.
4. Update the render pipeline to support runtime reload/passthrough toggles and add error reporting hooks consumed by the UI.

## Shader Parameter Editing
6. **Parameter access** – Expose `FilterChain::get_all_parameters()` returning a flat list of `{pass_index, ShaderParameter, current_value}` tuples. The UI iterates this list and renders sliders/inputs per parameter.
7. **Parameter modification** – `FilterChain::set_parameter(pass_index, name, value)` delegates to `FilterPass::set_parameter_override()` and triggers `update_ubo_parameters()`. Changes apply on the next frame without reload.
8. **Parameter reset** – `FilterChain::clear_parameter_overrides()` clears all pass overrides and restores preset defaults.

## Open Questions
- Should preset selections persist back to `config/goggles.toml`, or remain session-only?
- Do we constrain the preset catalog to curated directories, or expose a file picker for arbitrary paths?
