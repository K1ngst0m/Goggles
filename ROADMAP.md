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
- vkQueuePresentKHR interception overhead
- DMA-BUF export and copy operations
- Unix socket transmission
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

- [ ] Validate shader compilation for all implemented presets
- [ ] Catch SPIR-V compilation errors early
- [ ] Report shader compilation failures with diagnostics
- [ ] Golden image generation for reference outputs
- [ ] Comparison against golden images (pixel-by-pixel or perceptual)
- [ ] Automated regression detection for various shader presets
- [ ] Automated test runner for shader validation
- [ ] Integration with existing test infrastructure (Catch2)

### 2. Tracy Profiling Improvements

- [ ] Add Tracy GPU profiling support (Vulkan)
- [ ] Multiple processes with single timeline profiling (capture layer + viewer app), [context](https://github.com/wolfpld/tracy/issues/822)

### 3. Error Traceback Integration

- [ ] Integrate cpptrace for stack traces on errors
- [ ] Hook into existing error handling (`tl::expected`)
- [ ] Configure for debug/release builds

### 4. Shader Parameter Control

Runtime control of shader parameters and preset selection

- [ ] Config file based parameter overrides
- [ ] CLI arguments for preset and parameter selection
- [ ] GUI integration (ImGui) - pending in [#12](https://github.com/K1ngst0m/Goggles/pull/12)
  - Runtime preset reload without restart
  - Passthrough toggle to bypass filter chain
  - Direct DMA-BUF blitting for passthrough mode

---

## === Phase 2: Network Streaming ===

Extend local IPC streaming to network-capable streaming with encoding and transport.

**Potential Approaches:**
- GStreamer pipeline integration for encoding (H.264/H.265/AV1)
- Streaming protocols: RTSP, WebRTC, or custom UDP-based protocol
- Hardware encoding via VAAPI/NVENC
- Investigate Moonlight/Sunshine protocol compatibility

---
