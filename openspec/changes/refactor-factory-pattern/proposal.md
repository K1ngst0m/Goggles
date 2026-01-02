# Change: Refactor Two-Phase Initialization to Factory Pattern

## Why
Multiple classes use two-phase initialization (constructor + `init()`), which allows objects to exist in uninitialized states. This pattern is error-prone and violates RAII principles for classes that must be fully initialized before use. The InputForwarder refactoring demonstrated that using static factory methods with `ResultPtr<T>` provides better type safety and prevents usage of uninitialized objects.

**Note**: This refactoring targets major subsystem classes that require full initialization. Classes with optional/lazy initialization semantics (like FrameHistory, which can validly exist with depth=0) may retain two-phase init where appropriate.

## What Changes
- Refactor VulkanBackend to use factory pattern
- Refactor FilterChain to use factory pattern
- Refactor FilterPass to use factory pattern
- Refactor ShaderRuntime to use factory pattern
- Refactor Framebuffer to use factory pattern
- Refactor OutputPass to use factory pattern
- Refactor CaptureReceiver to use factory pattern

Each class will:
- Add `static auto create() -> ResultPtr<ClassName>;` factory method
- Move constructor to private section
- Remove public `init()` method
- Inline initialization logic into factory method
- Update all usage sites to use factory pattern

## Impact
- Affected code:
  - `src/render/backend/vulkan_backend.{hpp,cpp}`
  - `src/render/chain/filter_chain.{hpp,cpp}`
  - `src/render/chain/filter_pass.{hpp,cpp}`
  - `src/render/shader/shader_runtime.{hpp,cpp}`
  - `src/render/chain/framebuffer.{hpp,cpp}`
  - `src/render/chain/output_pass.{hpp,cpp}`
  - `src/capture/capture_receiver.{hpp,cpp}`
  - `src/app/main.cpp` and other usage sites
- Benefits:
  - Eliminates possibility of uninitialized object usage
  - Enforces single-phase initialization
  - Consistent error handling pattern across codebase
  - Better alignment with RAII and project policies
- Risks:
  - Breaking API changes require updating all call sites
  - Complex initialization in VulkanBackend may require careful refactoring
  - Some classes may have interdependencies requiring initialization order changes
