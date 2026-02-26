# Goggles Development Roadmap

## === LONG TERM ====

### 1. RetroArch Shader Support
Verify compatibility with RetroArch shader presets

**Rule**: Fix issues in our filter chain system (`src/render/filter_chain/`), not in shader files themselves

**Reference**: https://github.com/libretro/slang-shaders/blob/master/spec/SHADER_SPEC.md

- [ ] (Known issues to be populated as discovered)

### 2. Latency Optimization
Minimize end-to-end latency while maintaining code quality

**Example Areas** (tasks uncertain, use Tracy for measurement):
- Compositor frame delivery latency
- DMA-BUF import overhead
- Shader pass execution time
- Uniform buffer updates
- Render pass barriers/transitions
- Queue submission overhead
- CPU-GPU synchronization points

## === Phase 1: Fundamental Infrastructure & IPC Level Streaming ===

This roadmap covers core infrastructure work focused on establishing robust frame capture, IPC streaming, and shader processing capabilities.

---

### 1. Shader Validation & Testing
Prevent regressions in filter chain when adding new features

- [x] Catch SPIR-V compilation errors early
- [x] Report shader compilation failures with diagnostics
- [ ] Golden image generation for reference outputs
- [ ] Comparison against golden images (pixel-by-pixel or perceptual)
- [ ] Automated regression detection for various shader presets
- [ ] Automated test runner for shader validation

### 2. Tracy Profiling Improvements

- [ ] Add Tracy GPU profiling support (Vulkan)
- [x] Multiple processes with single timeline profiling (capture layer + viewer app), [context](https://github.com/wolfpld/tracy/issues/822)

### 3. Error Traceback Integration

- [ ] Integrate cpptrace for stack traces on errors
- [ ] Hook into existing error handling (`tl::expected`)
- [ ] Configure for debug/release builds

---

### 4. Compositor Protocol Completeness
Extend nested compositor to support broader app compatibility

**Current State**: Headless wlroots compositor with XDG Shell, XWayland, basic input

**Missing Capabilities** (blocking specific app types):

- [ ] **Layer Shell** (`wlr_layer_shell_v1`) - Game launcher overlays (Steam, Epic), desktop panels
- [ ] **Presentation Time** (`wlr_presentation_time`) - Frame pacing, tear-free presentation, VRR
- [ ] **Data Device** (`wl_data_device_manager`) - Clipboard and drag-and-drop for launchers
- [ ] **DRM Lease** (`wlr_drm_lease_v1`) - VR applications (SteamVR)
- [ ] **Idle Inhibit** (`zwlr_idle_inhibit_v1`) - Prevent screensaver during video playback
- [ ] **Touch Input** (`wlr_touch`) - Mobile/touchscreen game ports
- [ ] **Text Input** (`zwlr_text_input_v3`) - IME support for CJK languages

**Nice-to-Have Enhancements**:

- [ ] **Primary Selection** (`zwlr_primary_selection_v1`) - Middle-click paste
- [ ] **Output Management** (`wlr_output_manager_v1`) - Multi-monitor display configuration
- [ ] **Fractional Scaling** (`wp_fractional_scale_v1`) - HiDPI text rendering
- [ ] **Tablet/Stylus** (`wlr_tablet_tool`) - Drawing applications
- [ ] **Session Lock** (`ext_session_lock_manager_v1`) - Screen locker support
- [ ] **Gamma Control** (`wlr_gamma_control_manager`) - Color management
- [ ] **xdg-activation** - Window focus tokens for multi-window launchers
- [ ] **Keyboard Shortcuts Inhibit** (`zwp_keyboard_shortcuts_inhibit_v1`) - Global hotkeys
- [ ] **Tearing Control** (`wp_tearing_control_v1`) - Reduced latency mode
- [ ] **Cursor Shape** (`wp_cursor_shape_v1`) - Custom cursor themes

---

## === Phase 2: Network Streaming ===

Extend local frame capture to network-capable streaming with encoding and transport.

**Potential Approaches:**
- GStreamer pipeline integration for encoding (H.264/H.265/AV1)
- Streaming protocols: RTSP, WebRTC, or custom UDP-based protocol
- Hardware encoding via VAAPI/NVENC
- Investigate Moonlight/Sunshine protocol compatibility

---
