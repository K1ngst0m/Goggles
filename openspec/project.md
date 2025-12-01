# Project Context

## Purpose
Goggles is a real-time game capture and GPU post-processing application for Linux. Its primary goals are to achieve minimal capture-to-display latency and maximum fidelity, making it suitable for gaming. The post-processing pipeline is designed to be highly compatible with existing RetroArch shader presets.

## Tech Stack
- **Language:** C++20
- **Operating System:** Linux (Wayland primary, X11 secondary)
- **Graphics API:** Vulkan
- **Shading Language:** Slang (for SPIR-V generation)
- **Build System:** CMake
- **Dependency Management:** CPM.cmake (for header-only/simple libs) and conan (for complex libs)
- **Core Libraries:**
    - `tl::expected` for error handling
    - `spdlog` for logging
    - `toml11` for configuration
    - `Catch2` v3 for testing
    - `rigtorp::SPSCQueue` for inter-thread communication
    - `vulkan-hpp` for Vulkan C++ bindings (application code only, not capture layer)

## Project Conventions

### Code Style
- **Namespaces:** `goggles`, with sub-modules like `goggles::capture`, `goggles::render`. No `using namespace` in headers.
- **Type Names:** `PascalCase` for classes, structs, and enums (e.g., `VulkanBackend`).
- **Function/Variable Names:** `snake_case` (e.g., `submit_frame()`, `frame_count`).
- **Constants:** `UPPER_SNAKE_CASE` (e.g., `MAX_FRAMES_IN_FLIGHT`).
- **Private Members:** `m_` prefix (e.g., `m_device`).
- **File Names:** `snake_case.hpp` and `snake_case.cpp`.
- **Headers:** Use `#pragma once`. Include order: self, C++ std, third-party, project.
- **Formatting (clang-format):**
    - 4-space indentation, no tabs
    - 100-column line limit
    - Brace style: Attach
    - Pointer alignment: Left (`int* ptr`)

### Architecture Patterns
- **Modular Design:** The system is split into clear boundaries: `capture`, `pipeline`, `render`, and `app/control`.
- **Error Handling:** All fallible operations MUST return `tl::expected<T, Error>`. Exceptions are only for unrecoverable programming errors. Errors are logged once at subsystem boundaries.
- **Ownership:** Strict RAII is mandated for all resources. `std::unique_ptr` is the default for exclusive ownership; `std::shared_ptr` is used sparingly. No raw `new`/`delete`.
- **Vulkan API:** Application code MUST use vulkan-hpp with `VULKAN_HPP_NO_EXCEPTIONS` and dynamic dispatch. Use `vk::Unique*` for long-lived resources, plain handles for per-frame/GPU-async resources. Exception: capture layer uses raw C API.
- **Shader Pipeline:** A multi-pass pipeline model designed for compatibility with RetroArch shader semantics (passes, LUTs, parameters).
- **Threading Model:** Defaults to a single-threaded render loop on the main thread. A central job system (`goggles::util::JobSystem`) will be used for any future task parallelism, with inter-thread communication handled by lock-free SPSC queues.

### Testing Strategy
- **Framework:** Catch2 v3.
- **Scope (Initial):** Focus on testing non-GPU logic, including utility functions, configuration parsing, and pipeline graph logic. GPU-dependent code is tested manually for now.
- **Organization:** The `tests/` directory structure mirrors the `src/` directory.

### Git Workflow
- **Branching:** Feature-branch workflow with PRs to `main`.
- **CI Checks:** All PRs must pass:
    - Build with ASAN preset
    - Unit tests via CTest
    - `clang-tidy` static analysis
- **Formatting:** CI auto-formats code with `clang-format` and commits changes automatically. No local formatting required.

## Domain Context
- **Real-Time Latency:** A core goal is minimizing the time from frame capture to presentation on screen, often referred to as "glass-to-glass" latency. This is critical for a responsive gaming experience.
- **Vulkan Capture Layer:** The primary capture method involves injecting a Vulkan layer into a target application. This layer intercepts rendered frames directly from the GPU before they are presented, aiming for zero-copy access.
- **RetroArch Compatibility:** The project aims to support the widely-used RetroArch shader format. This involves a multi-pass rendering graph where the output of one shader pass can be the input to another, allowing for complex, chained post-processing effects.

## Important Constraints
- **Capture Layer Performance:** Code running inside the Vulkan capture layer (`src/capture/vk_layer/`) is highly constrained. It must not perform blocking operations, file I/O, or verbose logging in performance-critical paths like `vkQueuePresentKHR`.
- **Real-Time Thread Safety:** Per-frame code paths on the main render loop must not perform dynamic memory allocations or use blocking synchronization primitives.
- **Main Thread Ownership:** The main thread owns all global Vulkan objects (Instance, Device) and is solely responsible for queue submissions. It must not be blocked by I/O or heavy computation.
- **Configuration Format:** All configuration MUST be in TOML format.

## External Dependencies
- **CPM.cmake Managed:**
    - `tl::expected` (martinmoene/expected-lite)
    - `spdlog`
    - `toml11`
    - `Catch2`
    - `SDL3` (libsdl-org/SDL, release-3.2.0) - window creation and Vulkan surface support
- **Conan Managed:**
    - `Slang`
- **System Provided:**
    - Vulkan SDK
- **Threading (when needed):**
    - `BS::thread_pool` v3.5.0 (Phase 1 job system, already integrated)
    - `Taskflow` v3.10.0 (Phase 2 upgrade path, dependency-aware)
    - `rigtorp::SPSCQueue` v1.2.0 (wait-free inter-thread communication)