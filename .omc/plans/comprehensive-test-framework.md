# Comprehensive Real-App Test Framework for Goggles

**Status:** Active — Prerequisite (headless mode) complete. Phase 1 ready to implement.
**Date:** 2026-02-27
**Complexity:** HIGH
**Scope:** ~40+ new files across `tests/`, `scripts/`, CMake, and CI

---

## Current State

Headless mode was merged in PR #96 (`feat(app,compositor): add headless mode`). The full pipeline is implemented:

| Component | File | Status |
|-----------|------|--------|
| CLI flags `--headless`, `--frames`, `--output` | `src/app/cli.hpp`, `cli.cpp` | ✅ Done |
| `Application::create_headless()` | `src/app/application.cpp:242` | ✅ Done |
| `VulkanBackend::create_headless()` | `src/render/backend/vulkan_backend.hpp:49` | ✅ Done |
| Offscreen `vk::Image` render target | `vulkan_backend.hpp:222–225` | ✅ Done |
| `readback_to_png()` via stb_image_write | `vulkan_backend.hpp:81` | ✅ Done |
| `run_headless()` frame loop + signalfd | `src/app/application.cpp:298` | ✅ Done |
| SIGTERM/SIGINT via `signalfd` | `src/app/main.cpp:349` | ✅ Done |
| `waitid(WNOWAIT)` child-exit detection | `src/app/application.cpp:332` | ✅ Done |
| CLI parsing tests (5 cases) | `tests/app/test_cli.cpp:103` | ✅ Done |
| `waitid(WNOWAIT)` integration test | `tests/app/test_headless_child_exit.cpp` | ✅ Done |
| Reaper death propagation test | `tests/app/test_child_death_signal.cpp` | ✅ Done |
| **Application::create_headless() smoke test** | — | ❌ Missing |

**Missing gap:** No test exercises `Application::create_headless()` end-to-end with a real Vulkan device. This is addressed in Phase 1 Step 1.1.1 below, using the test client infrastructure once it exists. It is not a blocker for starting Phase 1.2.

**Interface confirmed:**
```bash
goggles --headless --frames N --output /tmp/frame.png [--app-width W --app-height H] -- <test-client>
```
Exit code 0 on success, non-zero on failure. PNG written by `readback_to_png()` via stb_image_write.

---

## Context

Goggles has 27 unit tests (Catch2 v3) and 2 integration tests (input forwarding, disabled in CI). There are 30+ manual test scenarios from OpenSpec archives covering aspect ratio, shaders, surface composition, filter chain toggle, cursor/UI, and frame capture. None of these have automated visual or GPU validation.

Two research references are available:
- **rdc-cli** (MIT): RenderDoc CLI with headless capture, assert-pixel, assert-image, assert-state, assert-clean, and two-frame diff
- **Weston test infrastructure** (MIT): headless compositor fixtures, fuzzy image comparison, synthetic input injection, breakpoint synchronization, reference image workflow

---

## Work Objectives

1. Enable automated visual regression testing of goggles render output
2. Automate the 30+ manual test scenarios from OpenSpec archives
3. Integrate GPU-based tests into CTest with clear headless vs GPU tiers
4. Establish golden image management and diff artifact workflow
5. Keep local developer workflow fast and ergonomic

---

## Guardrails

### Must Have
- All new code follows existing project policies (`docs/project_policies.md`)
- `tl::expected<T, Error>` for fallible operations; no exceptions for expected failures
- RAII for all resources; no raw `new`/`delete`
- New test targets register with CTest and use existing CMake preset conventions
- Tests are tagged/labeled so `ctest -L unit`, `ctest -L visual`, `ctest -L gpu` work independently
- Golden images stored in-repo under `tests/golden/` with `.gitattributes` for LFS
- Existing unit and integration tests remain unbroken
- MIT attribution for any patterns adapted from Weston or rdc-cli

### Must NOT Have
- No wholesale copy of Weston or rdc-cli source code (adapt concepts, attribute)
- No `std::thread`/`std::jthread` in pipeline code; use `goggles::util::JobSystem`
- No `vk::Unique*` or `vk::raii::*` in app-side Vulkan code
- No narration comments or LLM-style verbosity in code

---

## Phase 1: Test Harness Infrastructure

**Goal:** Build the foundational test mode, test client apps, and image comparison library that all subsequent phases depend on.

### Step 1.1: Headless Mode — COMPLETE ✅

Headless mode is fully implemented and merged (PR #96). The interface this plan depends on is confirmed:

```bash
goggles --headless --frames 10 --output /tmp/frame.png -- <test-client>
```

**Delivered (all acceptance criteria met):**
- `--headless` flag skips SDL init, window, and ImGui entirely
- `--frames N`: render exactly N compositor frames then export and exit
- `--output <path>`: write final offscreen image as PNG via stb_image_write
- `VulkanBackend::create_headless()`: no surface, no swapchain — offscreen `vk::Image` render target
- PNG readback: staging buffer + `vkCmdCopyImageToBuffer` + `stb_image_write_png`
- Signal handling: SIGTERM/SIGINT via `signalfd` for clean shutdown
- Exit code 0 on success, non-zero on failure

### Step 1.1.1: Application::create_headless() Smoke Test

**Depends on:** Step 1.2 (needs a real test client to deliver compositor frames)

Add a CTest integration test that runs the full headless pipeline against `solid_color_client`:

```bash
goggles --headless --frames 5 --output /tmp/headless_smoke.png -- solid_color_client
```

**Files to create:**
- Smoke test registered directly in `tests/CMakeLists.txt` via `add_test()`

**Acceptance criteria:**
- Test passes with exit code 0
- `/tmp/headless_smoke.png` is a valid PNG with non-zero dimensions
- Test runs under the default `test` CTest preset with `integration` label

### Step 1.2: Test Client Applications

Build minimal Wayland client apps that render known, deterministic patterns. These replace vkcube as test targets because their output is predictable.

**Files to create:**
- `tests/clients/CMakeLists.txt` — build rules for test clients
- `tests/clients/solid_color_client.cpp` — renders a single solid color (configurable via env var `TEST_COLOR=R,G,B,A`); validates basic frame delivery
- `tests/clients/gradient_client.cpp` — renders a horizontal gradient from black to white; validates per-pixel accuracy
- `tests/clients/quadrant_client.cpp` — renders four colored quadrants (red/green/blue/white); validates spatial correctness for aspect ratio and composition tests
- `tests/clients/multi_surface_client.cpp` — creates main surface + popup surface with known colors; validates surface composition and z-order

**Implementation approach:**
- Use `wl_shm` (CPU-rendered buffers) for simplicity and determinism — no GPU variance in source content
- Connect to the compositor's Wayland display via `WAYLAND_DISPLAY` env var (set by headless mode before spawning)
- Each client exits after rendering N stable frames (configurable, default 30)
- Use `wl_shell` or `xdg-wm-base` depending on what the compositor exposes

**Acceptance criteria:**
- `solid_color_client` with `TEST_COLOR=255,0,0,255` produces a fully red surface
- `quadrant_client` produces four distinct colored quadrants at known pixel positions
- All clients compile and link via `tests/clients/CMakeLists.txt`
- All clients exit cleanly after frame count reached

### Step 1.3: Image Comparison Library

Build a C++ image comparison utility (inspired by Weston's approach) usable as a library in Catch2 tests and as a standalone CLI tool.

**Files to create:**
- `tests/visual/image_compare.hpp` — API: `compare_images(actual, reference, tolerance) -> CompareResult`
- `tests/visual/image_compare.cpp` — implementation: fuzzy per-channel pixel comparison, diff image generation
- `tests/visual/image_compare_cli.cpp` — standalone tool: `goggles_image_compare actual.png reference.png --tolerance 0.02 --diff diff.png`
- `tests/visual/CMakeLists.txt` — build rules

**CompareResult struct:**
```cpp
struct CompareResult {
    bool passed;
    double max_channel_diff;    // 0.0-1.0
    double mean_diff;           // average per-pixel difference
    uint32_t failing_pixels;    // count of pixels exceeding tolerance
    double failing_percentage;  // failing_pixels / total_pixels
    std::filesystem::path diff_image;  // path to generated diff visualization
};
```

**Tolerance model (adapted from Weston):**
- Per-channel tolerance: each R/G/B/A channel compared independently
- Fuzzy threshold: percentage of pixels allowed to exceed tolerance (handles anti-aliasing, driver variance)
- Per-test configurable: shader tests need wider tolerance than solid-color tests

**Acceptance criteria:**
- Identical images return `passed=true` with zero diff
- Images differing by 1 channel value with tolerance=0 return `passed=false`
- Diff image highlights failing pixels in red overlay
- CLI tool returns exit code 0 for pass, 1 for fail

### Step 1.4: CMake and CTest Integration

**Decision (revised):** No separate `GOGGLES_BUILD_VISUAL_TESTS` option or `visual-test` preset. All test clients and the image comparison library build unconditionally because all their dependencies (wayland-client, wayland-protocols, stb_image, Catch2) are already hard requirements of the main goggles build. CTest labels provide the filtering mechanism.

**Files to modify:**
- `tests/CMakeLists.txt` — unconditionally add `tests/clients/` and `tests/visual/` subdirectories, register headless smoke tests with `integration` label

**CTest labels:**
- `unit` — existing Catch2 tests + image_compare unit tests (fast, no GPU needed)
- `integration` — existing input forwarding tests + headless smoke test
- `visual` — visual regression tests (Phase 2+, need GPU, headless OK)
- `gpu` — tests requiring specific GPU features (RenderDoc capture)

**Acceptance criteria:**
- `ctest -L unit` runs only unit tests
- `ctest -L visual` runs only visual regression tests
- `ctest -L integration` runs headless smoke and input forwarding tests
- All test infrastructure builds with any preset (debug, test, quality, etc.)
- No separate option or preset required

---

## Phase 2: Core Visual Regression Tests

**Goal:** Implement the first batch of automated visual tests covering aspect ratio modes and basic shader application.

### Step 2.1: Aspect Ratio Mode Tests

**Files to create:**
- `tests/visual/test_aspect_ratio.cpp` — Catch2 test file with parametrized sections

**Test matrix (8 tests):**

| Test | ScaleMode | Source | Viewport | Validation |
|------|-----------|--------|----------|------------|
| Fit letterbox | `fit` | 640x480 | 1920x1080 | Black bars top/bottom, content centered |
| Fit pillarbox | `fit` | 1920x1080 | 800x600 | Black bars left/right, content centered |
| Fill crop | `fill` | 640x480 | 1920x1080 | No black bars, content fills viewport |
| Stretch | `stretch` | 640x480 | 1920x1080 | Content fills viewport, aspect distorted |
| Integer 1x | `integer` (scale=1) | 320x240 | 1920x1080 | 320x240 centered, rest black |
| Integer 2x | `integer` (scale=2) | 320x240 | 1920x1080 | 640x480 centered, rest black |
| Integer auto | `integer` (scale=0) | 320x240 | 1920x1080 | Largest integer multiple that fits |
| Dynamic | `dynamic` | 640x480 | 1920x1080 | Pre-chain resolution applied |

**Validation strategy:**
- Use `quadrant_client` as source (four known colors at known positions)
- For each mode, compute expected pixel regions mathematically:
  - Black bar regions: sample N pixels, assert all are black (0,0,0)
  - Content regions: sample pixels at expected positions, assert correct quadrant color
  - Boundary pixels: tolerance for filtering/interpolation
- Golden image comparison as secondary check

**Acceptance criteria:**
- All 8 tests pass with `quadrant_client` source
- Each test produces a diff image on failure
- Tests are parametrized (one test function, multiple `SECTION` blocks or `GENERATE`)
- Total execution time < 60s for all 8 tests

### Step 2.2: Basic Shader Application Tests

**Files to create:**
- `tests/visual/test_shader_basic.cpp` — validates shader preset applied vs bypass
- `tests/golden/shader_bypass_quadrant.png` — reference: unshaded quadrant output
- `tests/golden/shader_zfast_quadrant.png` — reference: zfast-crt applied to quadrant

**Tests (3):**
1. **No shader (bypass)**: Load no preset, render quadrant_client, compare to bypass golden
2. **zfast-crt applied**: Load zfast-crt preset, render quadrant_client, compare to shader golden (wider tolerance ~5% for CRT scanline effects)
3. **Filter chain toggle**: Load zfast-crt, render with filter ON, then toggle OFF, re-render. First output matches shader golden, second matches bypass golden

**Acceptance criteria:**
- Bypass test matches golden within 0.1% tolerance
- Shader test matches golden within 5% tolerance (CRT effects vary slightly)
- Toggle test demonstrates both states in a single test run
- Golden images are committed and documented

### Step 2.3: Golden Image Management

**Files to create:**
- `tests/golden/.gitattributes` — `*.png filter=lfs diff=lfs merge=lfs -text`
- `tests/golden/README.md` — documents the golden image workflow
- `scripts/task/update-golden.sh` — regenerates golden images from current build

**Workflow:**
1. Developer runs `pixi run update-golden` to regenerate all golden images
2. Script runs goggles in headless mode for each scenario, captures output
3. Images are committed via Git LFS
4. CI compares against committed golden images
5. On intentional visual change: re-run update-golden, review diffs, commit

**Update script logic:**
```bash
# For each test scenario config:
goggles --headless --frames 10 --output tests/golden/${name}.png \
    --config tests/visual/configs/${name}.toml -- tests/clients/quadrant_client
```

**Acceptance criteria:**
- `pixi run update-golden` regenerates all golden images
- Golden images are tracked via Git LFS
- README documents the update workflow clearly
- Script is idempotent (same input = same output within tolerance)

---

## Phase 3: rdc-cli Integration for GPU State Validation

**Goal:** Integrate RenderDoc capture and assertion capabilities for tests that need to validate GPU pipeline state beyond pixel output.

### Step 3.1: RenderDoc Build Integration

**Files to create/modify:**
- `packages/renderdoc/recipe.yaml` — pixi recipe to build renderdoc from source (needed for Python module + replay libraries)
- `pixi.toml` — add `renderdoc` local package dependency, add `rdc-cli` to Python environment
- `tests/gpu/CMakeLists.txt` — build rules for GPU-level tests
- `tests/gpu/conftest.py` — pytest fixtures for RenderDoc capture session management

**Acceptance criteria:**
- `pixi run python -c "import renderdoc"` succeeds
- `pixi run rdc --version` succeeds
- RenderDoc capture layer loads when goggles is launched with `RENDERDOC_HOOK=1`

### Step 3.2: Capture and Assert Pipeline

**Files to create:**
- `tests/gpu/test_validation_layers.py` — pytest: capture frame, `rdc assert-clean --min-severity HIGH`
- `tests/gpu/test_shader_state.py` — pytest: capture frame with shader, assert pipeline state
- `tests/gpu/test_pixel_accuracy.py` — pytest: capture frame, `rdc assert-pixel` at known locations
- `tests/gpu/capture_helper.py` — shared helper: launch goggles + test client, capture N-th frame, return .rdc path
- `scripts/task/test-gpu.sh` — wrapper: `pixi run pytest tests/gpu/ -v`

**Acceptance criteria:**
- `pixi run test-gpu` executes all GPU tests
- `test_validation_layers.py` passes with zero Vulkan validation errors
- `test_shader_state.py` validates correct pipeline topology for zfast-crt
- `test_pixel_accuracy.py` validates pixel values at known positions
- All tests produce clear error messages with RenderDoc capture path on failure

### Step 3.3: Two-Frame Diff Tests

**Files to create:**
- `tests/gpu/test_frame_diff.py` — validates visual changes between states using `rdc diff`

**Tests:**
1. **Shader toggle diff**: Capture frame without shader, capture with shader, diff shows change > threshold
2. **Parameter change diff**: Capture with default params, change a shader param, capture again, diff shows localized change
3. **Stability test**: Capture two consecutive frames of static scene, diff shows zero change (determinism validation)

**Acceptance criteria:**
- Shader toggle produces measurable diff above 1% threshold
- Stability test shows diff below 0.01% threshold
- Parameter change diff is non-zero but localized

---

## Phase 4: Compositor and Surface Composition Tests

**Goal:** Automate the compositor-level test scenarios: surface composition, layer shell stacking, popup rendering, and input routing.

### Step 4.1: Synthetic Input Injection

**Files to create:**
- `tests/compositor/input_injector.hpp` / `.cpp` — programmatic input injection

**Approach:**
- Use the existing `CompositorServer::forward_key()` / `forward_mouse_button()` / `forward_mouse_motion()` APIs directly in test code (already available — simpler than a test Wayland client)

**Acceptance criteria:**
- Input injector can send key press/release events
- Input injector can send pointer motion and button events
- Events are received by the target client running inside the compositor

### Step 4.2: Surface Composition Tests

**Files to create:**
- `tests/visual/test_surface_composition.cpp` — Catch2 test file
- `tests/clients/layer_shell_client.cpp` — creates layer shell surfaces at known z-layers with known colors

**Tests (6+):**
1. Wayland popup rendering: Main surface (blue) + xdg_popup (red) → red popup visible on top
2. XWayland popup: Same via X11 client → popup visible on top
3. Layer shell background: background layer (green) + toplevel (blue) → blue on top of green
4. Layer shell bottom: layer at bottom + toplevel → correct stacking
5. Layer shell top: layer at top + toplevel → layer on top
6. Layer shell overlay: layer at overlay + toplevel → overlay on top of everything

**Validation strategy:**
- Each surface renders a distinct solid color
- Sample pixels at known positions to verify z-order (mathematical, not golden image)

**Acceptance criteria:**
- All 6 stacking tests pass
- Each test verifies correct z-order via pixel sampling
- Tests use the compositor's headless backend (no display needed)

### Step 4.3: Filter Chain Per-Surface Tests

**Files to create:**
- `tests/visual/test_filter_chain_toggle.cpp`

**Tests (3):**
1. Per-surface bypass: Two surfaces, enable filter on one, disable on other → filtered/unfiltered verify
2. Global toggle: Two surfaces with filters, toggle global off → both show raw
3. Popup inheritance: Main surface has filter, popup does not → popup is unfiltered

**Acceptance criteria:**
- Per-surface filter state is correctly reflected in render output
- Global toggle overrides per-surface state
- Popup filter inheritance works as documented

---

## Phase 5: Remaining Manual Test Scenarios

**Goal:** Convert all remaining manual test scenarios to automated tests.

### Step 5.1: Cursor and UI Tests

**Files to create:**
- `tests/visual/test_cursor.cpp`
- `tests/visual/test_debug_overlay.cpp`

**Tests:** Software cursor renders at expected position; cursor lock/visibility state changes; FPS overlay renders in expected screen region.

### Step 5.2: Surface Selector UI Tests

**Files to create:**
- `tests/visual/test_surface_selector.cpp`

**Tests (6):** Multi-surface list renders correctly; selection highlights active surface; input routes to selected surface; UI appears/disappears on toggle; thumbnails correct; selection persists across frames.

### Step 5.3: Frame Capture Robustness Tests

**Files to create:**
- `tests/visual/test_frame_capture.cpp`

**Tests (5):**
1. Async capture: N frames delivered asynchronously, verify all delivered
2. Sync capture: synchronous path produces identical output
3. Rapid resize: resize window 10 times, verify no crash and final frame valid
4. Prolonged stability: 1000 frames, checksum of frame 1 vs frame 1000 for static scene
5. Format transitions: change surface format mid-stream, verify recovery

### Step 5.4: Shader Effect Tests

**Files to create:**
- `tests/visual/test_shader_effects.cpp`
- `tests/golden/crt_royale_lut.png`, `tests/golden/frame_history.png`, `tests/golden/hsm_afterglow.png`

**Tests:** CRT Royale with LUT textures; frame history/feedback textures; HSM Afterglow phosphor persistence; runtime parameter change produces visible diff.

---

## Phase 6: CI Integration

**Goal:** Integrate all test tiers into the CI pipeline with appropriate hardware gating.

### Step 6.1: CTest Preset and Pixi Task Updates

**Files to modify:**
- `pixi.toml` — add tasks:
  - `test-gpu`: run GPU/RenderDoc tests (`pytest tests/gpu/`)
  - `test-all`: run all test tiers
  - `update-golden`: regenerate golden images

Note: No separate `visual-test` preset or `test-visual` pixi task needed. All test infrastructure builds unconditionally with any preset. Use `ctest -L visual` to filter visual regression tests.

### Step 6.2: CI Workflow Updates

**Files to modify:**
- `.github/workflows/ci.yml` — add visual test job (requires GPU runner or SwiftShader)
- `.github/workflows/visual-tests.yml` (new) — dedicated visual test workflow

**CI strategy:**
- **Tier 1 (every PR)**: unit tests + static analysis (existing, no GPU)
- **Tier 2 (every PR, headless GPU)**: visual regression tests using SwiftShader (software Vulkan)
- **Tier 3 (nightly/manual)**: GPU tests with RenderDoc on a GPU-enabled runner

**SwiftShader approach:**
- Add SwiftShader as a pixi dependency or build from source
- Set `VK_ICD_FILENAMES` to SwiftShader's ICD JSON for reproducible software rendering
- Gives deterministic rendering across CI environments (no GPU driver variance)

### Step 6.3: Failure Artifact Collection

**Files to create:**
- `scripts/task/collect-test-artifacts.sh` — collects diff images, .rdc files, logs on failure

**CI integration:**
```yaml
- name: Upload test artifacts
  if: failure()
  uses: actions/upload-artifact@v4
  with:
    name: visual-test-diffs
    path: build/visual-test/tests/visual/diffs/
```

**Acceptance criteria:**
- Failed visual tests produce diff images in a known directory
- CI uploads diff artifacts on failure
- Diff images clearly show expected vs actual vs difference

---

## Determinism Strategy

**Challenge:** GPU rendering is inherently non-deterministic across drivers, hardware, and even runs.

**Mitigations:**
1. **wl_shm test clients**: CPU-rendered source content — no GPU variance in what enters the pipeline
2. **SwiftShader for CI**: Software Vulkan renderer produces bit-exact output across runs
3. **Per-test tolerance thresholds**: Solid color tests ~0.1%, shader tests ~5%, complex effects ~10%
4. **Mathematical assertions over golden images where possible**: Aspect ratio tests use computed pixel regions, not image comparison
5. **Stable test clients**: Known-color clients produce deterministic content regardless of GPU

---

## Dependency Summary

| Dependency | Purpose | Integration |
|-----------|---------|------------|
| SwiftShader | Deterministic software Vulkan for CI | pixi package or vendored build |
| stb_image_write | PNG export from framebuffer readback | Already vendored in `packages/stb` ✅ |
| RenderDoc | GPU state capture and assertion | New pixi package (`packages/renderdoc`) |
| rdc-cli | CLI interface to RenderDoc | pip install in pixi Python env |
| pytest | Python test runner for GPU tests | pixi Python dependency |

---

## Test Execution Time Budget

| Tier | Test Count | Target Time | Runs |
|------|-----------|-------------|------|
| Unit | ~27 existing | < 5s | Every PR |
| Integration | 3 existing + smoke | < 30s | Every PR (where available) |
| Visual (aspect ratio) | 8 | < 60s | Every PR |
| Visual (shader) | 6 | < 90s | Every PR |
| Visual (compositor) | 12 | < 120s | Every PR |
| Visual (remaining) | 15+ | < 180s | Every PR |
| GPU (RenderDoc) | 10 | < 300s | Nightly |
| **Total** | **80+** | **< 12 min** | |

---

## Risk Assessment

| Risk | Severity | Likelihood | Mitigation |
|------|----------|-----------|------------|
| wl_shm client frame delivery too slow for headless loop | LOW | LOW | Headless loop polls at 1ms intervals; wl_shm is synchronous — first commit arrives well within frame budget |
| Offscreen VkImage readback produces wrong format/layout | MEDIUM | LOW | Confirmed: `readback_to_png()` implemented and used in `run_headless()`; format/layout transition is internal |
| SwiftShader Vulkan version too old for goggles features | MEDIUM | LOW | Pin SwiftShader version; test locally before CI integration |
| Golden image churn on driver updates | MEDIUM | HIGH | Use SwiftShader for CI; mathematical assertions where possible; per-driver golden sets |
| RenderDoc build complexity | MEDIUM | MEDIUM | Make GPU tests optional (Phase 3 is independent); provide Docker-based build |
| Test execution time exceeds budget | LOW | MEDIUM | Parallelize independent tests; use CTest parallel execution |

---

## Implementation Order and Dependencies

```
Phase 1 (foundation)
  1.1 Headless mode ✅ COMPLETE
  1.2 Test clients ──────────────────────────────┐
  1.3 Image compare ─────────────────────────────┤
  1.4 CMake integration ─────────────────────────┤
  1.1.1 Headless smoke test (needs 1.2) ─────────┘

Phase 2 (first visual tests) -- depends on Phase 1
  2.1 Aspect ratio tests
  2.2 Shader tests
  2.3 Golden image management

Phase 3 (rdc-cli) -- independent of Phase 2, depends on Phase 1
  3.1 RenderDoc build
  3.2 Capture pipeline
  3.3 Two-frame diff tests

Phase 4 (compositor tests) -- depends on Phase 1 + Phase 2 patterns
  4.1 Input injection
  4.2 Surface composition
  4.3 Filter chain toggle

Phase 5 (remaining scenarios) -- depends on Phase 2 + Phase 4
  5.1 Cursor/UI
  5.2 Surface selector
  5.3 Frame capture robustness
  5.4 Shader effects

Phase 6 (CI) -- depends on Phase 2 minimum, ideally all phases
  6.1 CTest presets
  6.2 CI workflows
  6.3 Artifact collection
```

**Next immediate action:** Phase 1.2 — implement `solid_color_client` and `quadrant_client`. These are required to unblock everything else.

---

## Local Developer Workflow

```bash
# Build (all test infrastructure included by default)
pixi run build -p debug

# Run all tests
pixi run test -p test

# Run specific test category
ctest --preset test -L visual -R "aspect_ratio"

# Update golden images after intentional visual change
pixi run update-golden

# Run GPU tests (requires RenderDoc)
pixi run test-gpu

# Run everything
pixi run test-all
```

---

## Legal Notes

- **Weston (MIT)**: Adapt image comparison algorithm concept and headless fixture pattern. Do NOT copy source files. Add attribution comment in `tests/visual/image_compare.hpp` header.
- **rdc-cli (MIT)**: Used as an external tool dependency (pip install). No source code copied. Attribution in `tests/gpu/README.md`.
- **RenderDoc (MIT)**: Built from source for Python module. Standard open-source dependency. Attribution in `packages/renderdoc/recipe.yaml`.

---

## Changelog

- **2026-02-27 (update 2)**: Removed `GOGGLES_BUILD_VISUAL_TESTS` option and `visual-test` preset. All test clients and image comparison library now build unconditionally — all deps (wayland-client, stb_image, Catch2) are already project requirements. CTest labels provide the filtering mechanism. Updated Phase 1.4, Phase 6.1, Local Developer Workflow, and Guardrails accordingly.
