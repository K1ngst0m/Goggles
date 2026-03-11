# Tasks: Profiling GPU Timestamp Unavailable Testability Seam

## Phase 1: Diagnostics Policy and Unavailable Pool Foundation

- [x] 1.1 Update `src/util/diagnostics/diagnostic_policy.hpp` to add the defaulted `GpuTimestampAvailabilityMode` enum and a `gpu_timestamp_availability` field on `DiagnosticPolicy`, preserving the current production default of `auto_detect`.
  - **Verify:** `pixi run build -p test`

- [x] 1.2 Update `src/util/diagnostics/gpu_timestamp_pool.hpp` to declare an explicit unavailable construction path for `GpuTimestampPool` and document it as the shared path for forced-unavailable tests and hardware-unavailable runtime fallback.
  - **Verify:** `pixi run build -p test`

- [x] 1.3 Update `src/util/diagnostics/gpu_timestamp_pool.cpp` so the existing `timestampPeriod <= 0` branch delegates to the explicit unavailable construction path while preserving current available-path allocation and no-op unavailable semantics.
  - **Verify:** `pixi run -- ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`

## Phase 2: Runtime Wiring and Deterministic Profiling Coverage

- [x] 2.1 Update `src/render/chain/chain_runtime.cpp` so `ChainRuntime::sync_gpu_timestamp_pool()` resolves `DiagnosticPolicy::gpu_timestamp_availability`, uses normal auto-detection by default, and reuses the existing unavailable-event path when `force_unavailable` is requested.
  - **Verify:** `pixi run -- ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`

- [x] 2.2 Update `tests/render/test_gpu_timestamp_pool.cpp` so unavailable-path coverage constructs the explicit unavailable pool directly and asserts deterministic no-op behavior (`is_available() == false`, safe reset/write calls, empty readback) without hardware-based `SKIP()` on the core unavailable assertion.
  - **Verify:** `pixi run -- ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`

- [x] 2.3 Update `tests/render/test_runtime_diagnostics.cpp` so Tier 1 runtime diagnostics create a session with `gpu_timestamp_availability = force_unavailable`, record a frame through the real runtime path, and assert the info-severity `"GPU timestamps are unavailable on this device"` event while frame recording still succeeds.
  - **Verify:** `pixi run -- ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`

- [x] 2.4 Keep the existing available-path profiling assertions in `tests/render/test_runtime_diagnostics.cpp` intact by proving the default `auto_detect` path still carries GPU-duration/debug-label evidence unchanged when timestamps are available.
  - **Verify:** `pixi run -- ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`

## Phase 3: Registration and Goggles Regression Gates

- [x] 3.1 Update `tests/CMakeLists.txt` only if required to keep `tests/render/test_gpu_timestamp_pool.cpp` and `tests/render/test_runtime_diagnostics.cpp` registered correctly after the diagnostics-policy seam changes.
  - **Verify:** `pixi run build -p test`

- [x] 3.2 Run targeted profiling coverage with `pixi run -- ctest --preset test -R "^goggles_unit_tests$" --output-on-failure` to confirm the deterministic unavailable-path evidence replaces the prior environment-limited skip and that the profiling test binary still passes on the active Vulkan setup.
  - **Verify:** `pixi run -- ctest --preset test -R "^goggles_unit_tests$" --output-on-failure`

- [x] 3.3 Run the Goggles CI-parity gate `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality` to prove the seam stays narrow, preserves normal runtime behavior, and remains policy-clean.
  - **Verify:** `pixi run build -p asan && pixi run test -p asan && pixi run build -p quality`

---

## Summary

| Phase | Tasks | Focus |
|-------|-------|-------|
| Phase 1 | 3 | Diagnostics policy override and explicit unavailable pool construction |
| Phase 2 | 4 | Runtime wiring plus deterministic unavailable-path profiling evidence |
| Phase 3 | 3 | Test registration and Goggles regression gates |
| **Total** | **10** | |

## Implementation Order

Start with the diagnostics-scoped seam in `src/util/diagnostics/diagnostic_policy.hpp` and `src/util/diagnostics/gpu_timestamp_pool.*` so the unavailable state becomes a first-class construct before touching runtime wiring. Then update `src/render/chain/chain_runtime.cpp` and the profiling tests to exercise the real unavailable-event path deterministically, and finish with registration/build verification plus the standard ASAN and quality gates.
