# Change: Add Tracy Profiler Integration

## Why

Real-time performance analysis is critical for a frame-capture application targeting sub-16ms latency. Tracy provides frame-level visibility into CPU/GPU timing, helping identify bottlenecks in the render loop, filter chain execution, and capture pipeline without the overhead of heavyweight profilers.

## What Changes

- **New dependency:** Tracy profiler via CPM.cmake with `ENABLE_PROFILING` CMake option
- **New header:** `src/util/profiling.hpp` with abstracted macros that wrap Tracy (or fallback to no-op/chrono)
- **Instrumentation:** Add profiling zones to performance-critical code paths across all modules
- **clang-tidy:** Suppress macro-related warnings only for profiling macros

## Impact

- Affected specs: New `profiling` capability
- Affected code:
  - `cmake/Dependencies.cmake` - Tracy dependency
  - `cmake/CompilerConfig.cmake` - `ENABLE_PROFILING` option
  - `src/util/profiling.hpp` - Profiling macro header (new file)
  - `src/util/CMakeLists.txt` - Link Tracy
  - `src/app/main.cpp` - Frame marks, main loop profiling
  - `src/render/backend/vulkan_backend.cpp` - Render pipeline profiling
  - `src/render/chain/filter_chain.cpp` - Filter chain profiling
  - `src/render/chain/filter_pass.cpp` - Per-pass profiling
  - `src/render/shader/shader_runtime.cpp` - Shader compilation profiling
  - `src/capture/capture_receiver.cpp` - Capture receive profiling
  - `src/capture/vk_layer/vk_capture.cpp` - Capture layer profiling (minimal, performance-critical)
