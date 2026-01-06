## 1. Implementation
- [x] 1.1 Vendor or fetch the Dear ImGui docking branch, hook it into the SDL3/Vulkan render loop, and ensure it only links into the Goggles app (not the Vulkan capture layer).
- [x] 1.2 Build an ImGui "Shader Controls" dock/panel that shows the active preset, lists presets discovered from the config/shaders directory, and lets the user request a different preset or passthrough mode.
- [x] 1.3 Extend the render pipeline/filter chain to rebuild passes at runtime when a preset selection changes, with graceful fallback on failure.
- [x] 1.4 Implement a passthrough flag in the pipeline that bypasses the chain and reuses the zero-effect OutputPass when enabled, and restore the previous preset when disabled.
- [ ] 1.5 Expose shader parameter access via FilterChain (get_all_parameters, set_parameter, clear_parameter_overrides) and render parameter sliders in the UI.
- [ ] 1.6 Add validation/tests (unit or integration hooks) covering the reload/passthrough state machine and update docs/config defaults as needed.
