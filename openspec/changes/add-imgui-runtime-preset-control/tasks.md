## 1. Implementation
- [ ] 1.1 Vendor or fetch the Dear ImGui docking branch, hook it into the SDL3/Vulkan render loop, and ensure it only links into the Goggles app (not the Vulkan capture layer).
- [ ] 1.2 Build an ImGui "Shader Controls" dock/panel that shows the active preset, lists presets discovered from the config/shaders directory, and lets the user request a different preset or passthrough mode.
- [ ] 1.3 Extend the render pipeline/filter chain to rebuild passes at runtime when a preset selection changes, with graceful fallback on failure.
- [ ] 1.4 Implement a passthrough flag in the pipeline that bypasses the chain and reuses the zero-effect OutputPass when enabled, and restore the previous preset when disabled.
- [ ] 1.5 Add validation/tests (unit or integration hooks) covering the reload/passthrough state machine and update docs/config defaults as needed.
