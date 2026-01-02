# Design: Factory Pattern Refactoring

## Context
The codebase currently uses two-phase initialization (constructor + `init()`) for 7+ major classes. This pattern was common in older C++ codebases but violates modern RAII principles and the project's error handling policies. The InputForwarder refactoring (completed) demonstrated the viability of using static factory methods with `ResultPtr<T>` to eliminate uninitialized states.

## Goals / Non-Goals

### Goals
- Eliminate possibility of using uninitialized objects
- Enforce single-phase initialization across render and capture subsystems
- Provide consistent error handling using `ResultPtr<T>` pattern
- Maintain or improve error message clarity
- Ensure proper cleanup on initialization failures

### Non-Goals
- Changing the internal implementation logic of initialization
- Refactoring classes that don't use two-phase initialization
- Refactoring optional/lazy-initialized components that can validly exist in inactive states
- Modifying Vulkan layer code (uses C API, exempt from policy)
- Performance optimization (maintain current performance characteristics)

**Scope Clarification**: This refactoring targets major subsystem classes (backend, chains, passes, runtimes) that must be fully initialized before use. Classes like FrameHistory that implement optional features and can validly exist in an "inactive" state (depth=0) are excluded from this refactoring, as their two-phase initialization is intentional and appropriate for their use case.

## Decisions

### Factory Method Signature
Use `static auto create(...) -> ResultPtr<T>` for all factory methods:
- Static method allows private constructor enforcement
- `ResultPtr<T>` is type alias for `Result<std::unique_ptr<T>>`
- Consistent with project's Result-based error handling
- `std::unique_ptr<T>` provides clear ownership semantics

**Alternative considered:** Pass-by-value factories returning `Result<T>`:
- Rejected: Large objects (VulkanBackend) expensive to copy
- Rejected: Doesn't prevent uninitialized state if caller forgets to check Result

### Initialization Order
Bottom-up refactoring following dependency chain:
1. Leaf dependencies first (ShaderRuntime, Framebuffer)
2. Mid-level components (FilterPass, OutputPass)
3. Composition classes (FilterChain)
4. Root dependencies last (VulkanBackend, CaptureReceiver)

**Rationale:** Each refactored class can be tested independently before refactoring its dependents.

### Error Propagation
Use `GOGGLES_TRY()` macro where applicable for clean error propagation:
```cpp
auto FilterChain::create() -> ResultPtr<FilterChain> {
    auto chain = std::unique_ptr<FilterChain>(new FilterChain());

    // Use GOGGLES_TRY for nested factory calls
    chain->m_runtime = GOGGLES_TRY(ShaderRuntime::create(...));

    // Use make_result_ptr_error for direct errors
    if (some_condition) {
        return make_result_ptr_error<FilterChain>(ErrorCode::..., "message");
    }

    return make_result_ptr(std::move(chain));
}
```

**Alternative considered:** Manual error checking and propagation:
- Rejected: More verbose, error-prone
- `GOGGLES_TRY` already established in project, familiar to contributors

### Handling CaptureReceiver's bool init()
Convert from `bool init()` to `Result<void>` internally, then wrap in factory:
```cpp
auto CaptureReceiver::create() -> ResultPtr<CaptureReceiver> {
    auto receiver = std::unique_ptr<CaptureReceiver>(new CaptureReceiver());

    if (!receiver->internal_init()) {
        return make_result_ptr_error<CaptureReceiver>(
            ErrorCode::capture_init_failed, "Failed to initialize capture");
    }

    return make_result_ptr(std::move(receiver));
}
```

**Alternative considered:** Refactor internal implementation to use Result:
- Deferred: Broader change, can be done in follow-up if needed
- Current approach minimizes diff size

### Constructor Visibility
All constructors moved to private section:
- Prevents direct construction, enforces factory usage
- Move/copy operations remain deleted (unchanged)
- Destructor remains public (required for unique_ptr)

## VulkanBackend Complexity

VulkanBackend has the most complex initialization:
- 128+ lines of init logic
- 15+ private create_* helper methods
- Dependencies on FilterChain and OutputPass
- Multiple failure points requiring cleanup

Strategy:
1. Refactor dependencies first (FilterChain, OutputPass)
2. Convert VulkanBackend's create_* methods to return Result types
3. Use GOGGLES_TRY for each create_* call in factory method
4. Rely on RAII (vk::Unique*) for automatic cleanup on early return

Example transformation:
```cpp
// Before
auto VulkanBackend::init(...) -> Result<void> {
    auto device_result = create_device();
    if (!device_result) return device_result;

    auto swapchain_result = create_swapchain();
    if (!swapchain_result) return swapchain_result;

    // ... 10+ more calls
    return {};
}

// After
auto VulkanBackend::create(...) -> ResultPtr<VulkanBackend> {
    auto backend = std::unique_ptr<VulkanBackend>(new VulkanBackend());

    GOGGLES_TRY(backend->create_device());
    GOGGLES_TRY(backend->create_swapchain());
    // ... 10+ more calls

    return make_result_ptr(std::move(backend));
}
```

## Risks / Trade-offs

### Risk: Breaking API Changes
- **Mitigation:** All usage sites must be updated in same change
- **Mitigation:** Compilation errors will catch missed sites
- **Trade-off:** Short-term churn for long-term type safety

### Risk: Complex Initialization Failures
VulkanBackend and FilterChain have complex initialization with multiple failure points.
- **Mitigation:** RAII handles cleanup automatically (vk::Unique* types)
- **Mitigation:** Test failure paths with AddressSanitizer
- **Trade-off:** More complex factory methods, but safer than current pattern

### Risk: Initialization Order Dependencies
Some classes may have hidden dependencies on initialization order.
- **Mitigation:** Bottom-up refactoring exposes dependencies early
- **Mitigation:** Comprehensive testing after each class refactoring
- **Trade-off:** Slower incremental approach, but reduces risk

### Risk: Error Message Quality
Factory failures need clear error messages for debugging.
- **Mitigation:** Preserve existing error messages during refactoring
- **Mitigation:** Test error paths to verify messages are actionable
- **Trade-off:** None - factory pattern doesn't affect error messages

## Migration Plan

### Phase 1: Leaf Dependencies (ShaderRuntime, Framebuffer)
- Low risk, simple initialization logic
- Test independently before proceeding
- Establishes pattern for subsequent classes

### Phase 2: Mid-Level Components (FilterPass, OutputPass)
- Medium complexity, well-encapsulated
- Update FilterChain usage of ShaderRuntime
- Verify error propagation works correctly

### Phase 3: Composition Classes (FilterChain)
- Higher complexity due to dynamic FilterPass creation
- Depends on Phase 2 completeness
- Critical for VulkanBackend refactoring

### Phase 4: Root Dependencies (VulkanBackend, CaptureReceiver)
- Highest complexity and risk
- Depends on Phases 1-3 completeness
- Most impactful change for main.cpp

### Rollback Strategy
Each phase can be committed independently. If issues arise:
1. Revert most recent commit
2. Fix identified issues
3. Re-apply with additional testing

## Open Questions

### Q: Should we refactor all classes in single PR or multiple PRs?
**Recommendation:** Single PR per phase (4 PRs total)
- Phase 1 PR: ShaderRuntime + Framebuffer
- Phase 2 PR: FilterPass + OutputPass
- Phase 3 PR: FilterChain
- Phase 4 PR: VulkanBackend + CaptureReceiver

**Rationale:** Smaller PRs easier to review, can pause if issues found

### Q: Do we need backward compatibility shims?
**Answer:** No - this is internal API, no external consumers
- All usage sites are in-tree
- Compilation errors will catch all usages
- No runtime compatibility needed

### Q: Should we add tests for factory failures?
**Answer:** Yes - add unit tests for each factory's error paths
- Test invalid parameters
- Test resource allocation failures (where mockable)
- Test cleanup on partial initialization
- Use AddressSanitizer to verify no leaks

### Q: What about classes with optional initialization?
Some classes (like CaptureReceiver) allow init to fail without aborting.
**Answer:** Factory returns ResultPtr, caller checks and handles gracefully:
```cpp
auto receiver_result = CaptureReceiver::create();
if (!receiver_result) {
    GOGGLES_LOG_WARN("Capture disabled: {}", receiver_result.error().message);
    // Continue without capture
}
```
Pattern already used in main.cpp for InputForwarder (see lines 226-233).
