## 1. Implementation
- [x] 1.1 Vendor or fetch the Dear ImGui docking branch, hook it into the SDL3/Vulkan render loop, and ensure it only links into the Goggles app (not the Vulkan capture layer).
- [x] 1.2 Build an ImGui "Shader Controls" dock/panel that shows the active preset, lists presets discovered from the config/shaders directory, and lets the user request a different preset or passthrough mode.
- [x] 1.3 Extend the render pipeline/filter chain to rebuild passes at runtime when a preset selection changes, with graceful fallback on failure.
- [x] 1.4 Implement a passthrough flag in the pipeline that bypasses the chain and reuses the zero-effect OutputPass when enabled, and restore the previous preset when disabled.
- [x] 1.5 Expose shader parameter access via FilterChain (get_all_parameters, set_parameter, clear_parameter_overrides) and render parameter sliders in the UI.
- [ ] 1.6 Add validation/tests (unit or integration hooks) covering the reload/passthrough state machine and update docs/config defaults as needed.

## 2. Async Shader Reload (Added)
- [x] 2.1 Implement async shader compilation using JobSystem to avoid blocking the render loop during preset changes.
- [x] 2.2 Add deferred destruction mechanism (fixed-size array) for old FilterChain/ShaderRuntime to prevent use-after-free.
- [x] 2.3 Add consume_chain_swapped() notification to update UI parameters after async chain swap completes.
- [x] 2.4 Refactor render methods (record_render_commands, record_clear_commands) to accept optional UI callback, reducing code duplication.

## 3. Parameter UI Improvements (Added)
- [x] 3.1 Skip dummy/separator parameters (min == max) in UI, display as disabled text.
- [x] 3.2 Add debug logging for parameter update tracing.