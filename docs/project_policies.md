# Goggles Project Development Policies

**Version:** 1.2  
**Status:** Active  
**Last Updated:** 2026-03-09

This document is the authoritative, normative policy for the Goggles codebase.

## 1. Document Status and Precedence

### 1.1 Scope

These policies apply to all contributors (humans and coding agents) and all code in this repository unless a section explicitly narrows scope.

### 1.2 Normative language

The keywords **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT**, and **MAY** are used as defined in RFC 2119/8174.

### 1.3 Precedence rules

If rules conflict:

1. This document takes precedence over all other guidance files.
2. Section text takes precedence over summaries/checklists.
3. Narrower-scope rules (for example, compositor) take precedence within that scope.

### 1.4 Allowed exceptions

Exceptions MUST be explicit in code review and MUST include:

- reason,
- scope,
- rollback/remediation plan.

Implicit exceptions MUST NOT be used.

---

## 2. Core Non-Negotiables (Quick Checklist)

1. Fallible operations MUST return `tl::expected<T, Error>` (or project alias).
2. Expected runtime failures MUST NOT use exceptions.
3. Errors MUST be handled or propagated; silent failure MUST NOT occur.
4. Errors SHOULD be logged once at subsystem boundaries; duplicate cascading logs MUST NOT occur.
5. App-side Vulkan code MUST use `vk::` (vulkan-hpp), not raw `Vk*` handles.
6. Vulkan `vk::Result` returns MUST be checked explicitly (no `static_cast<void>(...)`).
7. Owned file descriptors MUST use `goggles::util::UniqueFd`.
8. Code comments MUST explain non-obvious why; narration comments MUST NOT be used.
9. Build/test workflows MUST use Pixi tasks and CMake/CTest presets.
10. Real-time render-path work MUST use `goggles::util::JobSystem` for concurrent pipeline/render work.

---

## 3. Error Handling Policy

**Applies to:** app, utilities, tests  
**Enforced by:** review, clang-tidy (partial)

### 3.1 Result model

- Fallible operations MUST return `tl::expected<T, Error>` (or project alias).
- APIs returning results SHOULD be marked `[[nodiscard]]`.
- Returning `bool`, sentinel values (`-1`, `nullptr`, `0`) for multi-cause failures MUST NOT be used.

### 3.2 Exceptions

- Expected runtime failures (I/O, parsing, resource creation, device/runtime states) MUST NOT use exceptions.
- Exceptions MAY be used for programming errors/invariant violations.

### 3.3 Error propagation and logging boundary

- Each error MUST be either handled or propagated.
- Subsystem boundaries SHOULD log once with actionable context.
- Re-logging the same error at multiple stack layers MUST NOT occur.
- Monadic composition (`and_then`, `or_else`, `transform`) SHOULD be used.

### 3.4 Error type

`Error` MUST remain lightweight and include an error code plus human-readable message. Source location metadata MAY be included when useful.

---

## 4. Logging Policy

**Applies to:** app
**Enforced by:** semgrep, review, runtime behavior

### 4.1 Logging backend

- Application logging MUST use project logging macros backed by `spdlog`.

### 4.2 Levels

Valid levels are: `trace`, `debug`, `info`, `warn`, `error`, `critical`.

### 4.3 Initialization

- The app MUST initialize one global logger at startup.
- App `[logging].*` config applies only to the app logger.

---

## 5. Naming, Layout, and Documentation Policy

**Applies to:** all C/C++ source and headers  
**Enforced by:** clang-format, clang-tidy (partial), semgrep, review

### 5.1 Naming rules

- C++ identifier naming MUST follow the repository `clang-tidy` naming configuration.
- Files MUST use `snake_case.hpp` / `snake_case.cpp`.
- Scoped `clang-tidy` overrides MAY narrow naming enforcement for generated or C-interop surfaces.

### 5.2 Header and include rules

- Headers MUST use `#pragma once`.
- Include order in `.cpp` MUST be:
  1) corresponding header, 2) C++ standard headers, 3) third-party headers,
  4) project headers from other modules, 5) project headers from same module.
- Includes within each group MUST be alphabetized.

### 5.3 Commenting rules

- Comments MUST explain non-obvious why, constraints, workarounds, or invariants.
- Comments that narrate obvious what MUST NOT be used.
- Tutorial/step-by-step comments for straightforward code MUST NOT be used.
- Trailing comments that restate the current line MUST NOT be used.
- For files larger than 200 lines, section dividers SHOULD be used when they improve navigation.

### 5.4 Public API docstrings

- Public/exported declarations SHOULD use Doxygen line comments (`///` / `///<`).
- `@brief` SHOULD be 1-2 sentences, active voice, verb-first.
- `@param` and `@return` SHOULD describe behavior/constraints, not restate types.

---

## 6. Ownership, Lifetime, and Vulkan Policy

**Applies to:** app code
**Enforced by:** semgrep, review, clang-tidy (partial)

### 6.1 Ownership model

- C++ owned resources MUST use RAII.
- `std::unique_ptr` SHOULD be default owning pointer.
- `std::shared_ptr` SHOULD be used only for true shared ownership and SHOULD include rationale when non-obvious.

### 6.2 File descriptors

- Owned file descriptors MUST use `goggles::util::UniqueFd`.
- Raw `int` fds MAY be used only at unavoidable API boundaries and SHOULD be wrapped immediately.

### 6.3 Vulkan API split

- Application code MUST use vulkan-hpp `vk::` APIs.
- Application code MUST NOT use raw `Vk*` handles directly.

### 6.4 Vulkan lifetime model

- App code MUST use plain `vk::` handles with explicit destroy/free calls.
- Destruction order MUST respect dependency order and synchronization requirements.

### 6.5 Vulkan error checks

- All `vk::Result` returns MUST be checked.
- `static_cast<void>(vulkan_call())` MUST NOT be used for result-returning Vulkan calls.
- `VK_TRY`, `GOGGLES_TRY`, and `GOGGLES_MUST` SHOULD be used where applicable.
- Cleanup/destructor paths MAY log-and-continue when propagation is impossible.

### 6.6 Object initialization

- Two-phase initialization (`constructor` + `init()`) for manager-like/resource-heavy types MUST NOT be used.
- Such types MUST use factory creation returning result types (for example `ResultPtr<T>`).
- Existing legacy code MAY be migrated incrementally when modified.

---

## 7. Threading and Real-Time Policy

**Applies to:** render path, background work
**Enforced by:** semgrep, review, profiling, architecture checks

### 7.1 Default model

- Render backend and pipeline execution MUST remain single-threaded on main thread by default.
- Threading in render path SHOULD be introduced only when profiling justifies it.

### 7.2 Render-path constraints

Per-frame real-time code paths MUST NOT:

- block on I/O,
- rely on blocking synchronization in hot paths,
- introduce unpredictable latency work that violates frame budget goals.

### 7.3 Job system requirement

- Concurrent pipeline/render work MUST use `goggles::util::JobSystem`.
- External integration code outside real-time path MAY use `std::jthread` with RAII-safe lifecycle.

---

## 8. Configuration Policy

**Applies to:** app configuration  
**Enforced by:** review, runtime validation

### 8.1 Format and source

- Config files MUST use TOML.
- Repository-shipped default config template MUST be `config/goggles.template.toml`.

### 8.2 Loading behavior

- Config MUST be read at startup.
- Default runtime config resolution MUST target `${XDG_CONFIG_HOME:-$HOME/.config}/goggles/goggles.toml`
  unless an explicit config path is provided.
- If no user config exists at the resolved runtime path, the application MAY materialize the
  shipped template there.
- Values MUST be validated.
- Optional values MUST have defaults.
- Invalid config MUST fail fast.

### 8.3 User config path

User-level XDG config loading is canonical.

---

## 9. Dependency Management Policy

**Applies to:** build and third-party dependencies  
**Enforced by:** review, lockfile checks

### 9.1 Source of truth

- Pixi (`pixi.toml` + `pixi.lock`) MUST be the primary dependency source of truth.
- `pixi.lock` MUST remain in sync with `pixi.toml`.

### 9.2 Dependency sources

- Pixi packages SHOULD be used.
- Local `packages/` MAY be used for pinned/forked/special builds.
- System packages MAY be used only when not practical through Pixi/local packages and MUST be documented.

### 9.3 Version and update policy

- Dependency versions MUST be explicit.
- Security updates SHOULD be applied immediately.
- Minor/major upgrades SHOULD include changelog review and validation.
- New dependencies MUST include rationale, license compatibility check, maintenance assessment, and team agreement.

---

## 10. Build and Testing Policy

**Applies to:** local development and CI  
**Enforced by:** CI, review

### 10.1 Build/test workflow

- Contributors MUST use Pixi tasks for routine workflows.
- Build/test invocations MUST use CMake/CTest presets.
- Ad-hoc non-preset build directories MUST NOT be used.

### 10.2 Framework and current scope

- Unit testing framework MUST be Catch2 v3.
- Test structure SHOULD mirror `src/` under `tests/`.
- Current automated scope is primarily non-GPU logic.
- GPU/Vulkan behavior MAY be validated manually/integration-first until justified for automation.

### 10.3 Required commands

Before opening/merging significant code changes, contributors SHOULD run relevant preset builds/tests (via Pixi tasks) appropriate to touched code.

---

## 11. Enforcement and Change Management

### 11.1 Compliance

- New code MUST comply with this document.
- Existing code SHOULD be migrated when touched.
- Non-compliant pull requests MAY be blocked.

### 11.2 Enforcement matrix

- `clang-format`: formatting/layout.
- `clang-tidy`: static checks (partial policy coverage).
- `semgrep`: repository policy bans that are practical to express as structural/static rules.
- CI presets/tests: build and test gates.
- Code review: semantic policy requirements not covered by tools.

### 11.3 Policy updates

- Policy changes MUST include rationale and team agreement.
- Version field and changelog section MUST be updated with policy edits.

---

## 12. Changelog

### 12.1 v1.1 -> v1.2

- Removed policy text that is now directly gated by Semgrep (`using namespace` in headers, raw `new`/`delete`, banned Vulkan RAII wrappers, render-path thread bans, std-stream/`printf` logging bans).
- Moved identifier naming specifics to the repository `clang-tidy` configuration and kept only the high-level policy contract plus scoped-override allowance.
- Expanded Semgrep logging coverage to include `printf` so the removed overlap remains CI-gated.
- Updated enforcement tags and matrix to call out Semgrep-owned policy checks explicitly.

### 12.2 v1.0 -> v1.1

- Rewrote document to RFC-style normative format.
- Added explicit precedence and exception process.
- Replaced long prose with canonical rules and scope/enforcement tags.
- Consolidated duplicated constraints and aligned summary/checklist to body rules.
- Moved examples into appendices for readability.

---

## Appendix A: Good/Bad Examples

### A.1 Comment narration (MUST NOT)

```cpp
// BAD
// Create command pool
vk::CommandPoolCreateInfo pool_info{};

// GOOD
vk::CommandPoolCreateInfo pool_info{};
```

### A.2 LLM-style verbose justification (MUST NOT)

```cpp
// BAD
// We use std::vector here because we need dynamic sizing and
// the number of images is not known at compile time
std::vector<vk::Image> images;

// GOOD
std::vector<vk::Image> images;
```

### A.3 Step-by-step tutorial comments (MUST NOT)

```cpp
// BAD
// 1. Get memory requirements
auto mem_reqs = device.getImageMemoryRequirements(image);
// 2. Find suitable memory type
uint32_t type_index = find_memory_type(mem_reqs);

// GOOD
auto mem_reqs = device.getImageMemoryRequirements(image);
uint32_t type_index = find_memory_type(mem_reqs);
```

### A.4 Constraint/workaround comments (required when non-obvious)

```cpp
// Vulkan takes ownership of fd on success; caller must dup() to retain
import_info.fd = dup(dmabuf_fd);

// vkGetMemoryFdPropertiesKHR requires dynamic dispatch on this path
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
```

### A.5 Vulkan result handling

```cpp
// BAD
static_cast<void>(device.waitIdle());

// GOOD
VK_TRY(device.waitIdle(), ErrorCode::VULKAN_DEVICE_LOST, "waitIdle failed");
```

### A.6 Two-phase init (MUST NOT for manager/resource-heavy types)

```cpp
// BAD
VulkanBackend backend;
auto result = backend.init(...);

// GOOD
auto backend = GOGGLES_TRY(VulkanBackend::create(...));
```

---

## Appendix B: Rationale (Compact)

- Compact normative rules reduce review ambiguity and improve agent behavior.
- Explicit Vulkan lifetime rules prevent incorrect API/lifetime usage.
- Strong error handling policy improves diagnosability and operational safety.
- Minimal comments and naming consistency improve long-term maintainability.
- Preset-based builds and pinned dependencies improve reproducibility.
