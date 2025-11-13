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

### C.1 Namespaces

- **Top-level:** `goggles`
- **Modules:** `goggles::<module>` (e.g., `goggles::capture`, `goggles::render`, `goggles::pipeline`, `goggles::util`)
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

---

## D) Ownership & Resource Lifetime Rules

### D.1 RAII Mandate

**All resources must be managed via RAII.**

- **Vulkan objects:** Wrap in RAII types (custom or from libraries like `vk-bootstrap`, `vulkan-hpp`).
- **File handles, sockets, etc.:** Use RAII wrappers (`std::fstream`, custom types).
- **No manual cleanup** in destructors unless encapsulated in RAII.

### D.2 Dynamic Memory Management

- **No raw `new`/`delete`** in application code.
- **Prefer `std::unique_ptr`** for exclusive ownership.
- **Use `std::shared_ptr`** only when:
  - Multiple owners genuinely needed (document why).
  - Object lifetime extends beyond single scope.
- **Prefer `std::make_unique`/`std::make_shared`** over explicit `new`.

### D.3 Ownership Transfer

- **Mark factory functions** with `[[nodiscard]]`:
  ```cpp
  [[nodiscard]] auto create_device() -> tl::expected<std::unique_ptr<VulkanDevice>, Error>;
  ```
- **Document ownership** in function comments when non-obvious.
- **Use move semantics** for transferring ownership (prefer `std::move` over copying).

### D.4 Vulkan Resource Management

- **Create wrapper types** for Vulkan handles (e.g., `VkDeviceRAII`, `VkImageRAII`).
- **Store creation info** with resources for debugging/recreation.
- **Never leak Vulkan objects:** Ensure proper destruction order (devices before instances, etc.).

---

## E) Threading Model Policy

### E.1 Default Threading Model

**Single-threaded render loop on the main thread.**

- **Render backend** (`goggles::render`) runs on main thread.
- **Pipeline execution** runs on main thread.
- **No thread pool** in initial prototypes.

### E.2 Capture Layer Threading

**Vulkan layer runs in host application threads (no control).**

- **Hooks execute** in the calling thread (usually render thread).
- **No blocking** in hooks (especially `vkQueuePresentKHR`).
- **Thread-safe state:** Use atomics or locks for shared layer state.

### E.3 Optional Threading

**Dedicated capture processing thread may be added later:**

- **Decision deferred** until Prototype 4+ (IPC implementation).
- **If added:** Must use `std::jthread` (RAII, auto-join).
- **No detached threads** (impossible to reason about lifetime).

### E.4 Cross-Thread Communication

**When threading is introduced:**

- **Use explicit message queues** (lock-free SPSC/MPSC preferred).
- **No shared mutable state** across threads (prefer message passing).
- **Synchronization primitives:** `std::mutex`, `std::condition_variable`, or lock-free atomics only.

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

**Use a hybrid approach combining CPM.cmake and vcpkg.**

- **CPM.cmake** for simple and header-only dependencies
- **vcpkg** for complex compiled dependencies
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

**Benefits:**
- Zero setup for developers (included in repo)
- Perfect sanitizer compatibility (inherits project flags)
- Fast for small/header-only libraries
- Cached in `~/.cache/CPM`

### G.3 vcpkg Usage

**For complex dependencies with build requirements:**

1. **Add to `vcpkg.json` manifest:**
   ```json
   {
     "dependencies": ["slang"]
   }
   ```

2. **Find in CMake:**
   ```cmake
   find_package(slang CONFIG REQUIRED)
   ```

**Managed via vcpkg:**
- Slang

**Setup requirement:**
- Developers must install vcpkg and set `VCPKG_ROOT` environment variable
- CMakePresets.json automatically configures toolchain file

### G.4 Version Pinning

**All dependencies must have explicit versions:**

- **CPM:** Specify `VERSION` parameter (Git tag or commit hash)
- **vcpkg:** Use versioning in vcpkg.json (builtin-baseline or version>=)
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
- Choose vcpkg for complex dependencies (requires Vulkan, LLVM, etc.)
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
├── pipeline/
│   └── graph/
│       └── test_pipeline_graph.cpp
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
3. **Naming:** `snake_case` functions/files, `PascalCase` types, `m_` private members.
4. **Ownership:** RAII, `std::unique_ptr` default, no raw `new`/`delete`.
5. **Threading:** Single-threaded default, `std::jthread` when needed, no detached threads.
6. **Configuration:** TOML format, `config/goggles.toml` for development.
7. **Dependencies:** Hybrid CPM.cmake + vcpkg, version pinning required, justify additions.
8. **Testing:** Catch2 v3, non-GPU logic only initially, structure mirrors `src/`.

All contributors must read and follow these policies. Questions or proposed changes should be discussed with the team.
