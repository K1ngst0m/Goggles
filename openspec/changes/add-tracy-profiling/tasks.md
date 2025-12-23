## 1. CMake Integration

- [ ] 1.1 Add `ENABLE_PROFILING` option to `cmake/CompilerConfig.cmake` (OFF by default)
- [ ] 1.2 Add Tracy dependency to `cmake/Dependencies.cmake` via CPM.cmake
- [ ] 1.3 Configure Tracy with `TRACY_ENABLE` definition when `ENABLE_PROFILING=ON`
- [ ] 1.4 Create `goggles_profiling_options` interface library for propagating Tracy settings
- [ ] 1.5 Add `profile` preset to `CMakePresets.json` (Release with profiling enabled)

## 2. Profiling Header

- [ ] 2.1 Create `src/util/profiling.hpp` with macro definitions
- [ ] 2.2 Implement `GOGGLES_PROFILE_SCOPE(name)` - Named scoped zone
- [ ] 2.3 Implement `GOGGLES_PROFILE_FUNCTION()` - Auto-named function zone
- [ ] 2.4 Implement `GOGGLES_PROFILE_FRAME(name)` - Frame boundary marker
- [ ] 2.5 Implement `GOGGLES_PROFILE_BEGIN(name)` / `GOGGLES_PROFILE_END()` - Manual zone pair
- [ ] 2.6 Implement `GOGGLES_PROFILE_TAG(text)` - Zone text annotation
- [ ] 2.7 Implement `GOGGLES_PROFILE_VALUE(name, value)` - Numeric plot value
- [ ] 2.8 Add clang-tidy NOLINT suppressions for macro definitions
- [ ] 2.9 Ensure all macros expand to no-op when `ENABLE_PROFILING=OFF`
- [ ] 2.10 Update `src/util/CMakeLists.txt` to link Tracy conditionally

## 3. Main Application Instrumentation

- [ ] 3.1 Add `GOGGLES_PROFILE_FRAME("Main")` at main loop start in `src/app/main.cpp`
- [ ] 3.2 Add `GOGGLES_PROFILE_SCOPE("EventProcessing")` around SDL event polling
- [ ] 3.3 Add `GOGGLES_PROFILE_SCOPE("CaptureReceive")` around `capture_receiver.poll_frame()`
- [ ] 3.4 Add `GOGGLES_PROFILE_SCOPE("RenderFrame")` / `GOGGLES_PROFILE_SCOPE("RenderClear")` around render calls
- [ ] 3.5 Add `GOGGLES_PROFILE_SCOPE("HandleResize")` around resize handling

## 4. Vulkan Backend Instrumentation

- [ ] 4.1 Add `GOGGLES_PROFILE_FUNCTION()` to `VulkanBackend::render_frame()`
- [ ] 4.2 Add `GOGGLES_PROFILE_FUNCTION()` to `VulkanBackend::render_clear()`
- [ ] 4.3 Add `GOGGLES_PROFILE_SCOPE("AcquireImage")` in `acquire_next_image()`
- [ ] 4.4 Add `GOGGLES_PROFILE_SCOPE("RecordCommands")` in `record_render_commands()`
- [ ] 4.5 Add `GOGGLES_PROFILE_SCOPE("SubmitPresent")` in `submit_and_present()`
- [ ] 4.6 Add `GOGGLES_PROFILE_FUNCTION()` to `import_dmabuf()`
- [ ] 4.7 Add `GOGGLES_PROFILE_FUNCTION()` to `create_swapchain()`
- [ ] 4.8 Add `GOGGLES_PROFILE_FUNCTION()` to `recreate_swapchain()`
- [ ] 4.9 Add `GOGGLES_PROFILE_FUNCTION()` to `init()`
- [ ] 4.10 Add `GOGGLES_PROFILE_FUNCTION()` to `init_filter_chain()`
- [ ] 4.11 Add `GOGGLES_PROFILE_FUNCTION()` to `load_shader_preset()`

## 5. Filter Chain Instrumentation

- [ ] 5.1 Add `GOGGLES_PROFILE_FUNCTION()` to `FilterChain::record()`
- [ ] 5.2 Add `GOGGLES_PROFILE_SCOPE("EnsureFramebuffers")` in `ensure_framebuffers()`
- [ ] 5.3 Add `GOGGLES_PROFILE_FUNCTION()` to `load_preset()`
- [ ] 5.4 Add `GOGGLES_PROFILE_FUNCTION()` to `handle_resize()`
- [ ] 5.5 Add `GOGGLES_PROFILE_FUNCTION()` to `init()`
- [ ] 5.6 Add `GOGGLES_PROFILE_SCOPE("LoadPresetTextures")` in `load_preset_textures()`

## 6. Filter Pass Instrumentation

- [ ] 6.1 Add `GOGGLES_PROFILE_FUNCTION()` to `FilterPass::record()`
- [ ] 6.2 Add pass index tag using `GOGGLES_PROFILE_TAG()` in `record()`
- [ ] 6.3 Add `GOGGLES_PROFILE_FUNCTION()` to `init()`
- [ ] 6.4 Add `GOGGLES_PROFILE_SCOPE("CreatePipeline")` in `create_pipeline()`
- [ ] 6.5 Add `GOGGLES_PROFILE_SCOPE("UpdateDescriptor")` in `update_descriptor()`
- [ ] 6.6 Add `GOGGLES_PROFILE_SCOPE("BuildPushConstants")` in `build_push_constants()`

## 7. Shader Runtime Instrumentation

- [ ] 7.1 Add `GOGGLES_PROFILE_FUNCTION()` to `ShaderRuntime::compile_shader()`
- [ ] 7.2 Add `GOGGLES_PROFILE_FUNCTION()` to `compile_retroarch_shader()`
- [ ] 7.3 Add `GOGGLES_PROFILE_SCOPE("CompileGLSL")` in `compile_glsl_with_reflection()`
- [ ] 7.4 Add `GOGGLES_PROFILE_SCOPE("LoadCachedSpirv")` in `load_cached_spirv()`
- [ ] 7.5 Add `GOGGLES_PROFILE_SCOPE("SaveCachedSpirv")` in `save_cached_spirv()`
- [ ] 7.6 Add `GOGGLES_PROFILE_FUNCTION()` to `init()`

## 8. Capture Receiver Instrumentation

- [ ] 8.1 Add `GOGGLES_PROFILE_FUNCTION()` to `CaptureReceiver::poll_frame()`
- [ ] 8.2 Add `GOGGLES_PROFILE_FUNCTION()` to `init()`

## 9. Capture Layer Instrumentation (Minimal)

- [ ] 9.1 Add `GOGGLES_PROFILE_SCOPE("OnPresent")` to `CaptureManager::on_present()` (entry only)
- [ ] 9.2 Add `GOGGLES_PROFILE_FUNCTION()` to `capture_frame()`
- [ ] 9.3 Ensure no profiling in hot paths per project policy B.4
- [ ] 9.4 Update `src/capture/vk_layer/CMakeLists.txt` to optionally link Tracy

## 10. Output Pass Instrumentation

- [ ] 10.1 Add `GOGGLES_PROFILE_FUNCTION()` to `OutputPass::record()`
- [ ] 10.2 Add `GOGGLES_PROFILE_FUNCTION()` to `init()`

## 11. Preset Parser Instrumentation

- [ ] 11.1 Add `GOGGLES_PROFILE_FUNCTION()` to `PresetParser::load()`

## 12. Texture Loader Instrumentation

- [ ] 12.1 Add `GOGGLES_PROFILE_FUNCTION()` to texture loading functions if they exist

## 13. Verification

- [ ] 13.1 Build with `ENABLE_PROFILING=OFF` - Verify zero overhead (no Tracy symbols)
- [ ] 13.2 Build with `ENABLE_PROFILING=ON` - Verify Tracy links correctly
- [ ] 13.3 Run with Tracy server connected - Verify zones appear
- [ ] 13.4 Run clang-tidy - Verify no new warnings from profiling code
- [ ] 13.5 Run test suite - Verify no regressions

## 14. Documentation

- [ ] 14.1 Update `openspec/project.md` dependencies section with Tracy
- [ ] 14.2 Add profiling usage notes to docs/ if appropriate
