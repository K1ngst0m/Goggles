# Goggles Project Development Policies

**Version:** 1.0
**Status:** Active
**Last Updated:** 2025-11-13

This document establishes mandatory project-wide development policies for the Goggles codebase. All contributors must follow these rules to ensure consistency, maintainability, and quality.

---

## A) Error Handling Policy

### A.1 Standard Result Type

**All fallible operations must return `tl::expected<T, Error>`.**

- **Library:** Use `tl::expected` from [martinmoene/expected-lite](https://github.com/martinmoene/expected-lite) or similar.
- **No exceptions** for expected failures (file I/O, resource creation, parsing).
- **Exceptions allowed** only for programming errors (assertions, contract violations).

### A.2 Error Type Definition

Define a lightweight `Error` struct:

```cpp
namespace goggles {
struct Error {
    ErrorCode code;           // Enum identifying error category
    std::string message;      // Human-readable description
    std::source_location loc; // C++20 source location (optional)
};

enum class ErrorCode {
    ok,
    file_not_found,
    vulkan_init_failed,
    shader_compile_failed,
    // ... extend as needed
};
}
```

### A.3 Error Propagation Rules

- **No silent failures:** Every error must be either handled or propagated upward.
- **Log once at subsystem boundaries:** When an error crosses module boundaries (e.g., capture → pipeline), log it exactly once.
- **No cascading logs:** Don't log the same error at every level of the call stack.
- **Use monadic operations:** Prefer `.and_then()`, `.or_else()`, `.transform()` for chaining.

### A.4 Banned Patterns

- **No `bool` returns** for operations that can fail in multiple ways.
- **No `-1`/`0`/`nullptr` as error indicators** (use `expected<T*, Error>` instead).
- **No ignored return values:** Mark all result-returning functions with `[[nodiscard]]`.

---

## B) Logging Policy

### B.1 Unified Logging Backend

**Use `spdlog` as the project-wide logging library.**

- **Rationale:** Fast, header-only option available, widely adopted, supports structured logging.
- **No `std::cout`, `std::cerr`, or `printf`** for subsystem logging (only for main app output).

### B.2 Severity Levels

Standard levels (in increasing severity):

1. **trace** - Fine-grained debug info (disabled in release builds)
2. **debug** - Debugging information (disabled in release builds)
3. **info** - Informational messages (initialization, state changes)
4. **warn** - Warnings that don't prevent operation
5. **error** - Recoverable errors
6. **critical** - Unrecoverable errors (application should terminate)

### B.3 Logging Macros

Define project-specific macros wrapping spdlog:

```cpp
#define GOGGLES_LOG_TRACE(...)    /* ... */
#define GOGGLES_LOG_DEBUG(...)    /* ... */
#define GOGGLES_LOG_INFO(...)     /* ... */
#define GOGGLES_LOG_WARN(...)     /* ... */
#define GOGGLES_LOG_ERROR(...)    /* ... */
#define GOGGLES_LOG_CRITICAL(...) /* ... */
```

### B.4 Capture Layer Logging Constraints

**Vulkan layer code (`src/capture/vk_layer/`) has strict logging limits:**

- **Use only `error` and `critical` levels** (to avoid polluting host app output).
- **No file I/O** or blocking operations in hot paths.
- **Prefix all logs** with `[goggles_vklayer]` for easy filtering.
- **Never log in `vkQueuePresentKHR`** hot path (queue for async logging if needed).

### B.5 Log Initialization

- **One global logger** initialized at application startup.
- **Console output** for development; file output optional.
- **Capture layer:** Initialize logger on first `vkCreateInstance` hook (lazily).

---

## C) Naming & Layout Conventions

**PRIORITY:** Section C.7 (Commenting Policy) takes precedence over all other style considerations. Violations will block code review approval.

### C.1 Namespaces

- **Top-level:** `goggles`
- **Modules:** `goggles::<module>` (e.g., `goggles::capture`, `goggles::render`, `goggles::util`)
- **Sub-modules:** `goggles::<module>::<submodule>` (e.g., `goggles::capture::vk_layer`)
- **Never** use `using namespace` in headers.
- **Prefer** explicit namespace qualification or scoped `using` declarations.

### C.2 Type Names

- **Classes/Structs:** `PascalCase` (e.g., `VulkanBackend`, `ShaderPass`, `FrameMetadata`)
- **Enums:** `PascalCase` for enum type, `snake_case` for enumerators (e.g., `enum class ErrorCode { ok, file_not_found }`)
- **Type aliases:** `PascalCase` (e.g., `using ShaderHandle = uint32_t;`)

### C.3 Function and Variable Names

- **Functions:** `snake_case` (e.g., `initialize_device()`, `submit_frame()`)
- **Variables:** `snake_case` (e.g., `frame_count`, `swapchain_images`)
- **Constants:** `UPPER_SNAKE_CASE` (e.g., `MAX_FRAMES_IN_FLIGHT`, `DEFAULT_WIDTH`)
- **Private members:** `m_` prefix (e.g., `m_device`, `m_swapchain`)

### C.4 File Names

- **Source files:** `snake_case.cpp` (e.g., `vulkan_backend.cpp`, `shader_loader.cpp`)
- **Header files:** `snake_case.hpp` (e.g., `vulkan_backend.hpp`, `shader_loader.hpp`)
- **One class per file** (exceptions for small helper types).

### C.5 Header Guidelines

- **Use `#pragma once`** in all headers (no include guards).
- **Minimal includes:** Forward-declare types when possible.
- **Include order:**
  1. Corresponding header (for `.cpp` files)
  2. C++ standard library headers
  3. Third-party library headers
  4. Project headers (other modules)
  5. Project headers (same module)
- **Sort alphabetically** within each group.

### C.6 Comments (Summary)

- **Minimal comments:** Code should be self-explanatory.
- **No obvious comments:** Don't state what the code clearly does.
- **See C.7** for comprehensive commenting policy.

### C.7 Code Commenting Policy

#### C.7.1 Philosophy

- Code explains **what**; comments explain **why** (only when non-obvious)
- Self-documenting code > commented code
- No comment is better than a bad comment
- Comments rot faster than code

#### C.7.2 Absolutely Forbidden Patterns

**1. Narration comments** - restating what code does:

```cpp
// BAD
// Create command pool
vk::CommandPoolCreateInfo pool_info{};

// GOOD - no comment needed
vk::CommandPoolCreateInfo pool_info{};
```

**2. LLM-style justifications** - verbose explanations of standard practices:

```cpp
// BAD
// We use std::vector here because we need dynamic sizing and
// the number of images is not known at compile time
std::vector<vk::Image> images;

// GOOD - no comment
std::vector<vk::Image> images;
```

**3. Step-by-step tutorials**:

```cpp
// BAD
// 1. Get memory requirements
auto mem_reqs = device.getImageMemoryRequirements(image);
// 2. Find suitable memory type
uint32_t type_index = find_memory_type(mem_reqs);
// 3. Allocate memory
auto memory = device.allocateMemory(alloc_info);

// GOOD - let code speak
auto mem_reqs = device.getImageMemoryRequirements(image);
uint32_t type_index = find_memory_type(mem_reqs);
auto memory = device.allocateMemory(alloc_info);
```

**4. Inline explanations of current line**:

```cpp
// BAD
auto result = device.waitIdle();  // Wait for GPU to finish

// GOOD
auto result = device.waitIdle();
```

#### C.7.3 Required Comments

**1. Non-obvious constraints** - external API requirements:

```cpp
// Vulkan takes ownership of fd on success; caller must dup() to retain
import_info.fd = dup(dmabuf_fd);
```

**2. Workarounds** - explain why unusual approach needed:

```cpp
// vkGetMemoryFdPropertiesKHR not in static export table, requires dynamic dispatch
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
```

**3. Invariants** - constraints not enforceable by types:

```cpp
// Must be power-of-2 for lock-free modulo (bitwise AND)
static_assert((CAPACITY & (CAPACITY - 1)) == 0);
```

**4. Section dividers** - in files >200 lines:

```cpp
// =============================================================================
// DMA-BUF Import
// =============================================================================
```

#### C.7.4 Optional Comments (Prefer Refactoring)

1. **Complex algorithms** - but first try extracting to named function
2. **File headers** - one-line purpose for interface files
3. **Public API docs** - brief purpose for exported functions

#### C.7.5 Format Rules

- Section dividers: blank line before and after
- Constraint comments: immediately above relevant code
- No trailing comments explaining current line
- No `/* */` block comments for code documentation

#### C.7.6 Review Checklist

Before committing, delete any comment where:

- [ ] Removing it wouldn't affect understanding in 6 months
- [ ] It explains **what** instead of **why**
- [ ] The code could be refactored to be self-explanatory
- [ ] It's longer than the code it describes

#### C.7.7 Rationale

Over-commenting causes:

1. **Maintenance burden** - comments drift from code
2. **Visual noise** - harder to scan actual logic
3. **False confidence** - outdated comments mislead
4. **Crutch for bad code** - encourages unclear implementations

---

## D) Ownership & Resource Lifetime Rules

### D.1 RAII Mandate

**All resources must be managed via RAII.**

- **Vulkan objects:** Wrap in RAII types (custom or from libraries like `vk-bootstrap`, `vulkan-hpp`).
- **File handles, sockets, etc.:** Use RAII wrappers (`std::fstream`, `UniqueFd`, custom types).
- **No manual cleanup** in destructors unless encapsulated in RAII.

### D.1.1 File Descriptor Management

**Never use raw `int` file descriptors when possible.**

- **Use `goggles::util::UniqueFd`** for all owned file descriptors.
- **Raw `int` is acceptable only** at uncontrollable boundaries (e.g., syscall returns, IPC receive).
- **Wrap immediately:** Convert raw fd to `UniqueFd` as soon as it's received.
- **API boundaries:** Non-owning references may use `int` with clear documentation.

```cpp
// GOOD: Wrap immediately after receive
int raw_fd = receive_fd_from_ipc();
UniqueFd owned{raw_fd};

// GOOD: Factory for duplication
auto copy = UniqueFd::dup_from(other_fd.get());

// BAD: Storing raw fd
int m_fd = some_fd;  // Who closes this?
```

### D.2 Vulkan API Usage

**Application code MUST use vulkan-hpp (C++ bindings), NOT the raw Vulkan C API.**

- **Library:** `vulkan-hpp` from Vulkan SDK (header-only, ships with SDK)
- **Namespace:** Use `vk::` types (e.g., `vk::Instance`, `vk::Device`, `vk::Image`)
- **No raw C handles:** Do not use `VkInstance`, `VkDevice`, etc. in application code

**Required Configuration:**

```cpp
#define VULKAN_HPP_NO_EXCEPTIONS           // Use vk::ResultValue returns, not exceptions
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1  // Dynamic dispatch for extension functions
#include <vulkan/vulkan.hpp>
```

- **No exceptions:** Vulkan-hpp exceptions are disabled. Functions return `vk::ResultValue<T>` which contains both result code and value. Convert to `nonstd::expected` at public API boundaries.
- **Dynamic dispatch:** Required for Vulkan extension functions (DMA-BUF, external memory). Initialize dispatcher with `vkGetInstanceProcAddr`, then instance-level and device-level functions after creation.

**RAII Handle Guidelines:**

Due to the async nature of GPU execution, RAII handles (`vk::Unique*`) cannot automatically handle synchronization. Use them selectively:

| Resource Type | Handle Type | Rationale |
|---------------|-------------|-----------|
| Instance, Device, Surface | `vk::Unique*` | Long-lived singletons, destroyed at shutdown when GPU idle |
| Swapchain, Pipelines, Layouts | `vk::Unique*` | Created once, destroyed with explicit sync or at shutdown |
| Command buffers from pools | `vk::CommandBuffer` (plain) | Pooled lifetime, reused across frames |
| Per-frame sync primitives | `vk::Fence`, `vk::Semaphore` (plain) | Reused every frame, explicit lifetime |
| Imported external images | `vk::Image` (plain) | Requires explicit sync before destruction |

**Exception - Capture Layer Code:**

The Vulkan capture layer (`src/capture/vk_layer/`) is exempt and MUST use the raw Vulkan C API because:
- Layer dispatch tables require C function pointers
- Layer must intercept C API calls from host applications
- Minimal dependencies required for injection into arbitrary applications

**Rationale:**
- Type safety: C++ types prevent handle misuse
- Zero overhead: Static dispatch compiles to identical code as raw C API
- Cleaner code: Method syntax, no manual struct initialization
- Selective RAII: Automatic cleanup where safe, explicit control where GPU async matters

### D.3 Dynamic Memory Management

- **No raw `new`/`delete`** in application code.
- **Prefer `std::unique_ptr`** for exclusive ownership.
- **Use `std::shared_ptr`** only when:
  - Multiple owners genuinely needed (document why).
  - Object lifetime extends beyond single scope.
- **Prefer `std::make_unique`/`std::make_shared`** over explicit `new`.

### D.4 Ownership Transfer

- **Mark factory functions** with `[[nodiscard]]`:
  ```cpp
  [[nodiscard]] auto create_device() -> tl::expected<std::unique_ptr<VulkanDevice>, Error>;
  ```
- **Document ownership** in function comments when non-obvious.
- **Use move semantics** for transferring ownership (prefer `std::move` over copying).

### D.5 Vulkan Resource Management

- **Follow RAII guidelines in D.2:** Use `vk::Unique*` only for appropriate resource types (see table in D.2).
- **Explicit sync before destruction:** For resources with GPU-async lifetime, call `device.waitIdle()` or wait on appropriate fences before destroying.
- **Store creation info** with resources for debugging/recreation.
- **Never leak Vulkan objects:** Ensure proper destruction order (devices before instances, etc.).
- **Member ordering:** When using `vk::Unique*` members, declare in reverse destruction order (device before instance, etc.) so destructors run correctly.

### D.6 Vulkan Error Handling Rules

**All vulkan-hpp calls returning `vk::Result` must be explicitly checked.**

#### D.6.1 Prohibited Pattern

```cpp
// PROHIBITED - masks device loss, GPU hangs, driver bugs
static_cast<void>(device.waitIdle());
static_cast<void>(cmd.begin(begin_info));
```

#### D.6.2 Error Handling Macros

Use the provided macros for concise, consistent error handling:

**`VK_TRY` - For vk::Result early return:**

```cpp
#include <render/backend/vulkan_error.hpp>

// Usage: VK_TRY(call, ErrorCode, "message")
VK_TRY(cmd.reset(), ErrorCode::VULKAN_DEVICE_LOST, "Command buffer reset failed");
VK_TRY(cmd.begin(begin_info), ErrorCode::VULKAN_DEVICE_LOST, "Command buffer begin failed");
VK_TRY(m_device->waitIdle(), ErrorCode::VULKAN_DEVICE_LOST, "waitIdle failed");
```

**`GOGGLES_TRY` - For Result<T> propagation:**

```cpp
#include <util/error.hpp>

// Usage: GOGGLES_TRY(result_returning_call())
GOGGLES_TRY(create_instance());
GOGGLES_TRY(create_surface(window));
GOGGLES_TRY(create_device());
```

#### D.6.3 Manual Pattern (When Macros Don't Apply)

For complex error handling with cleanup or special logic:

```cpp
auto [result, value] = some_vulkan_call();
if (result != vk::Result::eSuccess) {
    cleanup_partial_resources();
    return make_error<T>(ErrorCode::VULKAN_INIT_FAILED,
                         "Operation failed: " + vk::to_string(result));
}
```

#### D.6.4 Shutdown/Cleanup Pattern

For destructors and cleanup functions where propagation is impossible:

```cpp
auto result = m_device->waitIdle();
if (result != vk::Result::eSuccess) {
    GOGGLES_LOG_WARN("waitIdle failed during shutdown: {}", vk::to_string(result));
}
// Continue cleanup regardless - resources must be freed
```

#### D.6.5 Error Classification

| vk::Result | Severity | Action |
|------------|----------|--------|
| `eErrorDeviceLost` | Fatal | Propagate error, trigger shutdown |
| `eErrorOutOfDeviceMemory` | Fatal | Propagate error |
| `eErrorOutOfDateKHR` | Rebuild | Recreate swapchain |
| `eSuboptimalKHR` | Warning | Log, continue, queue resize |
| `eTimeout` | Recoverable | Log warning, skip frame |

#### D.6.6 Rationale

- Silent failures mask device loss events that require application restart
- GPU hangs become undebuggable without error context
- Production applications (DOOM, major game engines) check all Vulkan returns
- `static_cast<void>` provides zero information for post-mortem debugging
- Macros reduce boilerplate while maintaining consistent error handling

---

## E) Threading Model Policy

### E.1 Default Threading Model

**Single-threaded render loop on the main thread.**

- **Render backend** (`goggles::render`) runs on main thread.
- **Pipeline execution** runs on main thread.
- **No thread pool** in initial prototypes (Prototype 1-3).
- **Phased approach:** Threading introduced only when profiling justifies it (see `docs/threading_architecture.md`).

### E.2 Capture Layer Threading

**Vulkan layer runs in host application threads (no control).**

- **Hooks execute** in the calling thread (usually render thread).
- **No blocking** in hooks (especially `vkQueuePresentKHR`).
- **Thread-safe state:** Use atomics or locks for shared layer state.

### E.3 Multi-Threading Implementation (Phase 1+)

**When threading is introduced (Prototype 4+):**

- **Trigger:** Main thread CPU time consistently exceeds 8ms per frame.
- **Phased rollout:**
  - Phase 1: Offload blocking tasks (encode, I/O) to worker threads
  - Phase 2: Parallelize Vulkan command buffer generation
- **See:** `docs/threading_architecture.md` for complete implementation roadmap.

### E.4 Main Thread Responsibilities

**The main thread owns:**

- Vulkan instance, device, swapchain lifecycle
- Queue submission (`vkQueueSubmit` - NOT thread-safe)
- Window events and user input
- Job coordination (submit, wait, synchronize)

**The main thread MUST NOT:**

- Block on I/O operations
- Perform heavy computation (>1ms CPU time)
- Allocate memory in per-frame code paths

### E.5 Job System & Task Parallelism

**All concurrent processing MUST use the project's central job system.**

- **Phase 1 library:** BS::thread_pool (simple job submission)
- **Phase 2 library:** Taskflow (dependency-aware task scheduling)
- **Wrapper interface:** `goggles::util::JobSystem` in `src/util/job_system.hpp`

**Direct use of `std::thread` or `std::jthread` for pipeline work is PROHIBITED.**

**Exception:** External integration code (networking, IPC) outside the real-time path may use `std::jthread` with RAII guarantees.

### E.6 Real-Time Code Constraints

**Per-frame code paths (main thread render loop) MUST NOT:**

- Perform dynamic memory allocations (`new`, `malloc`, `std::make_shared`)
- Use blocking synchronization primitives (`std::mutex`, `std::condition_variable`)
- Exceed 8ms CPU time budget (excluding GPU sync)

**Worker thread code paths SHOULD:**

- Pre-allocate resources during initialization
- Use lock-free/wait-free primitives for coordination
- Bound worst-case execution time

### E.7 Inter-Thread Communication

**All real-time inter-thread communication MUST use:**

- **rigtorp::SPSCQueue** for fixed producer-consumer pairs (wait-free guarantee)
- **Never MPMC queues** in latency-critical paths (variable latency under contention)

**Communication pattern:**

- Use pointer passing with shared, pre-allocated buffers
- Producer pushes pointer to frame metadata into queue
- Consumer pops pointer, processes data, releases buffer
- No data copying across thread boundaries

**Performance requirements:**

- SPSC queue operations: <100ns push/pop
- Queue sizing: Based on worst-case latency × frame rate
- No blocking: All queue operations must be non-blocking (`try_push`, `try_pop`)

---

## F) Configuration Policy

### F.1 Configuration Format

**Use TOML for all configuration files.**

- **Rationale:** Human-readable, supports comments, typed values, widely used.
- **Library:** `toml11` or `tomlplusplus`.
- **No JSON** (no comments, less readable) or YAML (complex, ambiguous).

### F.2 Default Configuration Location

**Development/testing:** `config/goggles.toml` (in repository root).

Example structure:
```toml
[capture]
backend = "vulkan_layer"  # or "compositor"

[pipeline]
shader_preset = "presets/crt-royale.toml"

[render]
vsync = true
target_fps = 60

[logging]
level = "info"
file = ""  # empty = console only
```

### F.3 User Configuration Paths

**Deferred to post-prototype phase.**

- **Future:** Follow XDG Base Directory Specification on Linux:
  - `$XDG_CONFIG_HOME/goggles/goggles.toml` (usually `~/.config/goggles/goggles.toml`)
- **Not implemented yet:** Use repo config only for now.

### F.4 Configuration Loading

- **Read once at startup** (no hot-reload initially).
- **Validate all values** after parsing.
- **Provide defaults** for all optional values.
- **Fail fast** on invalid config (don't silently ignore).

---

## G) Dependency Management Policy

### G.1 Package Manager Strategy

**Use a hybrid approach combining CPM.cmake and Conan.**

- **CPM.cmake** for simple and header-only dependencies
- **Conan** for complex compiled dependencies
- **System packages** for platform-specific libraries (Vulkan SDK)

### G.2 CPM.cmake Usage

**For header-only and simple libraries:**

```cmake
CPMAddPackage(
    NAME library-name
    GITHUB_REPOSITORY org/repo
    VERSION x.y.z
    OPTIONS
        "BUILD_TESTS OFF"
        "BUILD_EXAMPLES OFF"
)
```

**Managed via CPM.cmake:**
- tl::expected (martinmoene/expected-lite)
- spdlog
- toml11
- Catch2
- SDL3 (window creation and Vulkan surface support)
- Slang (shader-slang prebuilt binaries for shader compilation)

**Benefits:**
- Zero setup for developers (included in repo)
- Perfect sanitizer compatibility (inherits project flags)
- Fast for small/header-only libraries
- Cached in `~/.cache/CPM`

### G.3 Conan Usage

**For complex dependencies with build requirements:**

1. **Add to `conanfile.txt` or `conanfile.py`:**
   ```ini
   [requires]
   slang/2024.x
   ```

2. **Find in CMake:**
   ```cmake
   find_package(slang CONFIG REQUIRED)
   ```

**Managed via Conan:**
- (none currently)

**Setup requirement:**
- Developers must install Conan (`pip install conan`)
- Run `conan install` before CMake configure

### G.4 Version Pinning

**All dependencies must have explicit versions:**

- **CPM:** Specify `VERSION` parameter (Git tag or commit hash)
- **Conan:** Use versioning in conanfile (explicit version specifiers)
- **Document rationale** for version changes in commit messages

### G.5 Adding New Dependencies

**Before adding a dependency:**

1. **Justify necessity** - Can functionality be implemented in-project?
2. **Check license compatibility** - Must be compatible with project license
3. **Assess maintenance status** - Active development? Recent releases?
4. **Consider alternatives** - Compare multiple options
5. **Discuss with team** - No dependencies added without consensus

**Selection criteria:**
- Choose CPM for header-only or simple build systems
- Choose Conan for complex dependencies (requires Vulkan, LLVM, etc.)
- Use system packages for OS/platform libraries (Vulkan SDK, X11, Wayland)

### G.6 Dependency Update Policy

- **Patch updates:** Update freely (bug fixes, no API changes)
- **Minor updates:** Review changelog, test thoroughly
- **Major updates:** Requires discussion, may need code changes

**Update frequency:**
- Review dependency versions quarterly
- Security patches applied immediately

---

## H) Testing Policy

### H.1 Testing Framework

**Use Catch2 v3 as the project-wide testing framework.**

- **Rationale:** Modern C++, header-only option, good diagnostics, widely adopted.
- **Repository:** [catchorg/Catch2](https://github.com/catchorg/Catch2)
- **Integration:** Via CMake FetchContent or system package.

### H.2 Test Scope (Current Phase)

**Only non-GPU logic is tested initially:**

- **In scope:** Utility functions, configuration parsing, error handling, pipeline graph logic.
- **Out of scope:** Vulkan initialization, GPU resource management, rendering, capture layer.

**Rationale:** GPU/Vulkan testing requires complex mocking or real hardware; defer until necessary.

### H.3 Test Organization

**Test directory structure mirrors `src/`:**

```
tests/
├── util/
│   ├── test_logging.cpp
│   └── test_config.cpp
├── render/
│   └── chain/
│       └── test_filter_chain.cpp
└── CMakeLists.txt
```

### H.4 Test Naming Conventions

- **Test files:** `test_<module>.cpp` (e.g., `test_config.cpp`)
- **Test cases:** Descriptive names (e.g., `"Config parser handles missing file gracefully"`)
- **Use sections** for organizing related assertions.

### H.5 Integration and GPU Tests

**Deferred until Prototype 3+:**

- **Integration tests:** Test interactions between modules (e.g., capture → pipeline).
- **GPU tests:** Require Vulkan instance/device, not practical for unit tests.
- **Manual testing** used for Vulkan code until automated GPU tests are justified.

### H.6 Build Preset Requirement

**All builds and tests MUST use CMake presets.**

- **Never** run `cmake -B build` or `cmake --build build` directly without a preset.
- **Use** `cmake --preset <name>` for configuration.
- **Use** `cmake --build --preset <name>` for building.
- **Use** `ctest --preset <name>` for testing.

**Available presets:**
- `asan` - Debug build with AddressSanitizer (default for development)
- `ubsan` - Debug build with UndefinedBehaviorSanitizer
- `debug` - Debug build without sanitizers
- `release` - Optimized release build
- `relwithdebinfo` - Release with debug symbols

**Rationale:** Presets ensure consistent compiler flags, sanitizer settings, and build directories across all contributors and CI.

---

## Policy Enforcement

### Compliance

- **All new code** must follow these policies.
- **Existing code** should be updated incrementally when modified.
- **Pull requests** violating policies may be rejected or require revision.

### Tooling

- **clang-format:** Enforces naming and layout (automated).
- **clang-tidy:** Enforces some rules (e.g., `[[nodiscard]]`, naming conventions).
- **Code review:** Manual enforcement of policies not covered by tools.

### Amendments

- **Policies are living documents:** May be updated as the project evolves.
- **Changes require:** Discussion and consensus (document rationale).
- **Version history:** Track changes in git commits.

---

## Summary

These policies establish:

1. **Error handling:** `tl::expected<T, Error>`, no silent failures, log at boundaries.
2. **Logging:** `spdlog`, standard levels, minimal logging in capture layer.
3. **Naming & Comments:** `snake_case` functions/files, `PascalCase` types, `m_` private members. Comments explain **why**, never **what**. No narration. No LLM-style verbosity.
4. **Ownership:** RAII, `std::unique_ptr` default, no raw `new`/`delete`. Vulkan errors must be checked (no `static_cast<void>`).
5. **Threading:** Single-threaded default, `std::jthread` when needed, no detached threads.
6. **Configuration:** TOML format, `config/goggles.toml` for development.
7. **Dependencies:** Hybrid CPM.cmake + conan, version pinning required, justify additions.
8. **Testing:** Catch2 v3, non-GPU logic only initially, structure mirrors `src/`.

All contributors must read and follow these policies. Questions or proposed changes should be discussed with the team.
