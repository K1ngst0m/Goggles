# Design: Profiling GPU Timestamp Unavailable Testability Seam

## Technical Approach

The change adds a diagnostics-scoped testability seam for the one profiling branch that still lacks deterministic evidence: `GPU timestamps unavailable`. The default runtime path in `src/render/chain/chain_runtime.cpp` continues to auto-detect timestamp support from the active `vk::PhysicalDevice` and create a normal `GpuTimestampPool` exactly as it does today.

The new seam has two parts:
- a policy-level override that can request `force_unavailable` when a diagnostic session is created, and
- an explicit `GpuTimestampPool` unavailable-construction path that produces the same no-op pool state currently reached only when `timestampPeriod <= 0`.

This keeps the change narrow: tests can deterministically force the unavailable branch without stubbing `vk::PhysicalDevice::getProperties()`, without introducing a broad Vulkan capability abstraction, and without changing user-visible profiling behavior.

## Architecture Decisions

### Decision: Put the override on `DiagnosticPolicy`, not on Vulkan-device wrappers

**Choice**: Extend `goggles::diagnostics::DiagnosticPolicy` with a small defaulted enum that controls timestamp availability resolution for the session, with `auto_detect` as the production default and `force_unavailable` as the deterministic test mode.
**Alternatives considered**: (a) Stub or wrap `vk::PhysicalDevice::getProperties()`; (b) add a repo-wide Vulkan capability abstraction; (c) add a mutable test hook or global override in `ChainRuntime`.
**Rationale**: `DiagnosticPolicy` already defines profiling/session behavior at the boundary where tests create Tier 1 sessions. Adding one defaulted diagnostics field keeps the seam local to profiling setup, preserves all existing call sites, and avoids widening the render stack into a general mocking framework.

### Decision: Add an explicit unavailable `GpuTimestampPool` factory

**Choice**: Add a dedicated `GpuTimestampPool` construction path for the unavailable state, implemented in `src/util/diagnostics/gpu_timestamp_pool.*`, and reuse it both for forced-unavailable tests and for the existing `timestampPeriod <= 0` branch.
**Alternatives considered**: (a) Overload `create()` with raw timestamp-period injection; (b) expose mutable internal state or subclassing hooks; (c) leave the unavailable path hardware-gated.
**Rationale**: The class already models unavailable timestamps as a safe no-op pool (`is_available() == false`, reset/write calls return immediately, `read_results()` returns an empty vector). Making that state constructible directly is the smallest possible production seam and gives unit tests deterministic coverage without changing the normal available-path allocation logic.

### Decision: Reuse the existing runtime event path instead of synthesizing test-only events

**Choice**: Keep `ChainRuntime::sync_gpu_timestamp_pool()` as the single place that creates the pool and emits the existing runtime info event when the resulting pool is unavailable.
**Alternatives considered**: (a) Emit a synthetic unavailable event directly from tests; (b) add a second test-only runtime branch just for event emission.
**Rationale**: Verification needs evidence for the real Tier 1 runtime path, not a parallel test harness. Reusing the existing session setup path means the deterministic test still proves the same event emission and `timestamps_active()` gating used in production.

## Data Flow

### Default production path

```text
DiagnosticPolicy(auto_detect)
        |
        v
ChainRuntime::create_diagnostic_session()
        |
        v
ChainRuntime::sync_gpu_timestamp_pool()
        |
        +--> GpuTimestampPool::create(device, physical_device, max_passes, frames)
                 |
                 +--> physical_device.getProperties().limits.timestampPeriod
                 +--> timestampPeriod > 0  -> allocate query pool, available = true
                 \--> timestampPeriod <= 0 -> create unavailable pool
        |
        \--> if !pool->is_available() emit runtime info event
```

### Deterministic unavailable test path

```text
DiagnosticPolicy(force_unavailable)
        |
        v
ChainRuntime::create_diagnostic_session()
        |
        v
ChainRuntime::sync_gpu_timestamp_pool()
        |
        +--> GpuTimestampPool::create_unavailable()
        |
        \--> emit "GPU timestamps are unavailable on this device"
                    |
                    +--> ChainExecutor timestamps remain inactive
                    \--> frame recording still succeeds
```

### Unit-level unavailable pool coverage

```text
test_gpu_timestamp_pool.cpp
        |
        +--> available-path test uses real device + normal create()
        \--> unavailable-path test uses create_unavailable()
                -> is_available() == false
                -> reset/write calls are safe no-ops
                -> read_results() returns empty samples
```

## File Changes

| File | Action | Description |
|------|--------|-------------|
| `src/util/diagnostics/diagnostic_policy.hpp` | Modify | Add the defaulted timestamp-availability override enum used only by diagnostics session setup. |
| `src/util/diagnostics/gpu_timestamp_pool.hpp` | Modify | Declare the explicit unavailable factory/helper and keep the existing create API for normal detection. |
| `src/util/diagnostics/gpu_timestamp_pool.cpp` | Modify | Implement the unavailable factory and make the hardware-unavailable branch delegate to the same path. |
| `src/render/chain/chain_runtime.cpp` | Modify | Resolve the policy override in `sync_gpu_timestamp_pool()`, choose normal create vs forced unavailable, and preserve existing runtime event emission. |
| `tests/render/test_gpu_timestamp_pool.cpp` | Modify | Replace the environment-gated unavailable assertion with deterministic construction of the unavailable pool while retaining real-device coverage for the available path. |
| `tests/render/test_runtime_diagnostics.cpp` | Modify | Add deterministic Tier 1 unavailable-event coverage via the policy override and keep the current available GPU-duration/debug-label runtime evidence. |

## Interfaces / Contracts

```cpp
namespace goggles::diagnostics {

enum class GpuTimestampAvailabilityMode : uint8_t {
    auto_detect,
    force_unavailable,
};

struct DiagnosticPolicy {
    PolicyMode mode = PolicyMode::compatibility;
    CaptureMode capture_mode = CaptureMode::standard;
    ActivationTier tier = ActivationTier::tier0;
    uint32_t capture_frame_limit = 1;
    uint64_t retention_bytes = 256ULL * 1024 * 1024;
    bool promote_fallback_to_error = false;
    bool reflection_loss_is_fatal = false;
    GpuTimestampAvailabilityMode gpu_timestamp_availability =
        GpuTimestampAvailabilityMode::auto_detect;
};

class GpuTimestampPool {
public:
    [[nodiscard]] static auto create(vk::Device device, vk::PhysicalDevice physical_device,
                                     uint32_t max_passes, uint32_t frames_in_flight)
        -> Result<std::unique_ptr<GpuTimestampPool>>;

    [[nodiscard]] static auto create_unavailable() -> std::unique_ptr<GpuTimestampPool>;
};

} // namespace goggles::diagnostics
```

`ChainRuntime::sync_gpu_timestamp_pool()` resolves the pool as follows:
- `auto_detect`: call existing `GpuTimestampPool::create(...)`
- `force_unavailable`: bypass physical-device property detection and install `create_unavailable()`

No other runtime code needs a new interface. `ChainExecutor::timestamps_active()` continues to rely on `gpu_timestamp_pool->is_available()`, so forced-unavailable sessions automatically follow the existing no-op behavior.

## Testing Strategy

| Layer | What to Test | Approach |
|-------|-------------|----------|
| Unit | Explicit unavailable pool semantics | In `tests/render/test_gpu_timestamp_pool.cpp`, construct `create_unavailable()` and assert `is_available() == false`, reset/write methods do not fail, and `read_results()` is empty without any hardware skip. |
| Unit | Normal available timestamp behavior | Keep the current real-device `create()` coverage for available query-pool allocation, timestamp writes, and sample ordering; still `SKIP()` only when no timestamp-capable GPU is present. |
| Integration | Tier 1 unavailable diagnostic event path | In `tests/render/test_runtime_diagnostics.cpp`, create a Tier 1 session with `gpu_timestamp_availability = force_unavailable`, register `TestHarnessSink`, record a frame, and assert the runtime info event is emitted while frame recording still succeeds. |
| Integration | Tier 1 available GPU durations and debug labels | Preserve the current runtime tests that use real hardware for `gpu_duration_us` evidence and debug-label capture/disabled-dispatch behavior. |
| Regression | Production default remains unchanged | Ensure existing callers that do not set the override still auto-detect timestamps from `vk::PhysicalDevice` and continue passing the current profiling tests. |

## Migration / Rollout

No migration required. The new policy field defaults to `auto_detect`, so existing production callers and API bridges retain current behavior unless a test explicitly opts into `force_unavailable`.

## Open Questions

- [ ] None blocking. The remaining follow-up is task refresh so implementation work can be split around the policy enum, unavailable factory, and deterministic test updates.
