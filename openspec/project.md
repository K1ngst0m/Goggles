# Project Context

## Purpose
Goggles is a real-time game capture and GPU post-processing application for Linux. Its primary goals are to achieve minimal capture-to-display latency and maximum fidelity, making it suitable for gaming. The post-processing pipeline is designed to be highly compatible with existing RetroArch shader presets.

## Tech Stack
- **Language:** C++20
- **Operating System:** Linux (Wayland primary, X11 secondary)
- **Graphics API:** Vulkan
- **Shading Language:** Slang (for SPIR-V generation)
- **Build System:** CMake
- **Dependency Management:** CPM.cmake (header-only libs and prebuilt binaries)
- **Core Libraries:**
    - `tl::expected` for error handling
    - `spdlog` for logging
    - `toml11` for configuration
    - `Catch2` v3 for testing
    - `goggles::util::SPSCQueue` for inter-thread communication (custom implementation)
    - `vulkan-hpp` for Vulkan C++ bindings (application code only, not capture layer)

## Project Structure
```
goggles/
├── src/
│   ├── app/                      # Main application executable
│   │   └── main.cpp
│   ├── render/
│   │   ├── backend/              # Vulkan API abstraction
│   │   │   ├── vulkan_backend.*  # Device, swapchain, queues
│   │   │   ├── vulkan_debug.*    # Debug utilities
│   │   │   └── vulkan_error.hpp  # Error handling
│   │   ├── shader/               # Shader compilation
│   │   │   ├── shader_runtime.*  # Slang → SPIR-V
│   │   │   ├── slang_reflect.*   # Reflection utilities
│   │   │   └── retroarch_preprocessor.* # Preset preprocessing
│   │   └── chain/                # Filter chain (RetroArch-style)
│   │       ├── filter_chain.*    # Chain orchestration
│   │       ├── filter_pass.*     # Individual filter passes
│   │       ├── output_pass.*     # Final output pass
│   │       ├── preset_parser.*   # .slangp preset parsing
│   │       └── framebuffer.*     # Framebuffer management
│   ├── capture/
│   │   ├── vk_layer/             # Vulkan layer for game interception
│   │   │   ├── vk_capture.*      # Frame capture implementation
│   │   │   ├── vk_hooks.*        # Vulkan function hooks
│   │   │   ├── vk_dispatch.*     # Dispatch table management
│   │   │   └── ipc_socket.*      # IPC communication
│   │   ├── compositor/           # Compositor capture (future)
│   │   ├── capture_receiver.*    # App-side frame receiver
│   │   └── capture_protocol.hpp  # IPC protocol definitions
│   └── util/                     # Utility functions
│       ├── error.hpp             # tl::expected error types
│       ├── logging.*             # spdlog wrapper
│       ├── config.*              # TOML configuration
│       ├── job_system.*          # BS::thread_pool wrapper
│       ├── queues.hpp            # Custom SPSCQueue
│       └── unique_fd.hpp         # RAII file descriptor
├── shaders/
│   ├── internal/                 # Built-in shaders
│   └── retroarch/                # RetroArch shader presets
├── config/
│   ├── goggles.toml              # App configuration
│   └── goggles_layer.json*       # Vulkan layer manifest
├── tests/                        # Catch2 unit tests (mirrors src/)
├── docs/                         # Architecture documentation
├── cmake/                        # CMake modules and config
└── scripts/                      # Helper scripts
```

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
- **Modular Design:** The system is split into clear boundaries: `capture`, `render` (with `chain/` for filter pipeline), and `app`.
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
    - `tl::expected` v0.8.0 (martinmoene/expected-lite) - error handling
    - `spdlog` v1.15.0 - logging
    - `toml11` v4.2.0 - configuration parsing
    - `Catch2` v3.8.0 - testing framework
    - `SDL3` release-3.2.0 (libsdl-org/SDL) - window creation and Vulkan surface support
    - `Slang` v2025.23.2 - shader compilation (downloaded as prebuilt binary)
- **System Provided:**
    - Vulkan SDK
- **Threading:**
    - `BS::thread_pool` v3.5.0 (Phase 1 job system, already integrated)
    - `Taskflow` v3.10.0 (Phase 2 upgrade path, dependency-aware, commented out)
- **Internal Implementations:**
    - `goggles::util::SPSCQueue` - custom wait-free SPSC queue in `src/util/queues.hpp`

## Documentation Policy

### What to Document
- **Architecture decisions** - The "why" behind design choices
- **Component boundaries** - What each module is responsible for
- **Data flow** - How data moves between components (IPC, queues)
- **Performance constraints** - Design trade-offs for real-time requirements
- **External API contracts** - Interfaces between subsystems

### What NOT to Document (AI Regenerates)
- Function signatures and parameters (read the code)
- Implementation details within a module
- Line-by-line code explanations
- Obvious patterns that follow project conventions

### Document Pattern
All topic-specific documentation SHALL follow:
1. **Purpose** (1-2 sentences) - What this doc covers
2. **Overview** (diagram if helpful) - High-level flow
3. **Key Components** - What pieces exist and why
4. **How They Connect** - Data/control flow
5. **Constraints/Decisions** - The "why"

### File Naming
- All docs in `docs/` use `snake_case.md`
- Entry point: `docs/architecture.md`

### Maintenance
- Keep docs high-level; AI can fill implementation details from code
- Review quarterly for staleness
- Update when architecture changes, not for code refactors