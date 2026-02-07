## 1. UI State and Types

- [x] 1.1 Add `PreChainState` struct to `imgui_layer.hpp` with `target_width`, `target_height`, and `dirty` flag
- [x] 1.2 Add `PreChainState prechain` member to `ShaderControlState`
- [x] 1.3 Add `PreChainChangeCallback` type alias for `std::function<void(uint32_t width, uint32_t height)>`
- [x] 1.4 Add `set_prechain_change_callback()` method to `ImGuiLayer`
- [x] 1.5 Add `set_prechain_state()` method to initialize UI from current backend state

## 2. UI Drawing Refactor

- [x] 2.1 Extract effect stage widgets from `draw_shader_controls()` into `draw_effect_stage_controls()`
- [x] 2.2 Create `draw_prechain_stage_controls()` with resolution width/height inputs
- [x] 2.3 Create `draw_postchain_stage_controls()` as placeholder section (displays "Output Blit")
- [x] 2.4 Restructure `draw_shader_controls()` to use three `CollapsingHeader` sections
- [x] 2.5 Add "Apply" button in pre-chain section that invokes callback when resolution changes

## 3. FilterChain Runtime Update

- [x] 3.1 Add `set_prechain_resolution(uint32_t width, uint32_t height)` method to `FilterChain`
- [x] 3.2 Method clears existing pre-chain passes/framebuffers to force rebuild on next frame
- [x] 3.3 Update `m_source_resolution` member when method is called
- [x] 3.4 Add `get_prechain_resolution()` getter for UI initialization

## 4. VulkanBackend Exposure

- [x] 4.1 Add `set_prechain_resolution()` forwarding method to `VulkanBackend`
- [x] 4.2 Add `get_prechain_resolution()` forwarding method to `VulkanBackend`

## 5. Application Wiring

- [x] 5.1 In `Application::create()`, set `ImGuiLayer` pre-chain callback to forward to `VulkanBackend`
- [x] 5.2 Initialize `ImGuiLayer` pre-chain state from `VulkanBackend::get_prechain_resolution()`
- [x] 5.3 Update pre-chain UI state after filter chain operations that may change resolution

## 6. Validation

- [x] 6.1 Build with `pixi run dev -p quality` and verify no warnings
- [x] 6.2 Test UI displays three sections correctly
- [x] 6.3 Test pre-chain resolution change triggers downsample pass rebuild
- [x] 6.4 Verify effect stage controls work as before (no regression)
