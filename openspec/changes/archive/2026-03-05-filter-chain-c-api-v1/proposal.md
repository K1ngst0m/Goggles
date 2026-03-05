## Why

Goggles needs a stable, embeddable filter-chain API that non-C++ hosts can consume without coupling to internal C++ types. Delivering a Vulkan-only v1 C ABI now enables engine and FFI integrations while preserving runtime behavior parity and minimizing migration risk.

## Problem

The current boundary is C++-only and does not define a public, versioned ABI contract for ownership, nullability, error semantics, or calling convention. This blocks straightforward static/shared linking by C, Rust, Python, and C# hosts and makes cross-version compatibility guarantees unclear.

## Scope

This change defines and ships a v1 public C API surface for filter chain runtime creation, preset loading, frame recording, stage policy, control snapshots/mutation, and runtime diagnostics. It includes a thin C shim over existing filter-chain internals and contract tests that lock lifecycle, validation, ownership, and ABI behavior required for v1.x stability.

## What Changes

- Add a single public header `include/goggles_filter_chain.h` with explicit export/calling-convention macros, opaque handles, fixed-width enum-like scalar typedefs, and `struct_size`-gated extensible structs.
- Add mandatory v1 exported symbols for version/capability queries, runtime lifecycle (`create/destroy`), preset APIs (`*_load`, `*_load_ex`), resize/record APIs, stage policy and prechain resolution APIs, control snapshot/list APIs, control mutation/reset APIs, and status/diagnostics APIs.
- Introduce first-class stage domain values and masks for `prechain`, `effect`, and `postchain` while preserving deterministic execution and list ordering semantics.
- Define strict contract behavior for nullability, out-parameter writes on failure, ownership/lifetime, non-finite control rejection, UTF-8 path validation, `frame_index` bounds, and status-first error handling.
- Keep host-managed async reload/synchronization model and keep record API limited to command recording (no implicit submit/present).
- Add conformance and integration tests that lock ABI-visible structure/layout assumptions and behavioral guarantees for v1.

## Capabilities

### New Capabilities
- `filter-chain-c-api`: Public Vulkan v1 C ABI for filter-chain runtime lifecycle, recording, controls, diagnostics, and compatibility guarantees.

### Modified Capabilities
- None.

## Non-goals

- Dynamic loader-table API in v1.
- Non-Vulkan backend support in v1.
- Exposing pass-graph/shader-runtime internals in public ABI.
- Hidden background threads, implicit async preset reload, or global mutable runtime singleton.
- Implicit command submission or presentation from record APIs.

## Risks

- ABI freeze risk if symbol names/types/calling convention are not locked correctly in v1.
- Integration regressions if C shim diverges from existing C++ behavior for lifecycle, stage ordering, clamping, or resize/record sequencing.
- Host misuse risk around command-buffer/layout preconditions, pointer lifetime, or serialization when contracts are under-specified.
- Cross-compiler/packaging drift risk for shared/static builds if export requirements and layout checks are incomplete.

## Validation Plan

- Add contract tests for lifecycle state matrix, invalid call states, and null-safe/idempotent destroy behavior.
- Add validation tests for null pointers, invalid enum/scalar values, zero/invalid extents, invalid stage masks, malformed UTF-8/length inputs, and non-finite control values.
- Add tests for out-parameter guarantees, including unchanged-on-failure behavior and forced-null exceptions for create/list APIs.
- Add tests for record contract (`frame_index < num_sync_indices`, invalid-argument no-command-recorded behavior, and layout pre/post expectations).
- Add ownership/lifetime tests for snapshot data and descriptor-string validity windows.
- Add ABI durability checks (`sizeof`/`offsetof` and typedef widths) and cross-compiler shared/static smoke coverage.

## Impact

- **Impacted modules/files**: `include/goggles_filter_chain.h`, new C shim implementation under render-chain boundary, backend call sites that bind to filter-chain API, packaging/export definitions, and filter-chain test suites (unit/integration/FFI-oriented contract tests).
- **Impacted OpenSpec specs**: new capability spec at `openspec/changes/filter-chain-c-api-v1/specs/filter-chain-c-api/spec.md`.
- **Policy-sensitive impacts**: status-code-first failures (no exceptions), explicit ownership/lifetime boundaries, no hidden render-path threading, explicit Vulkan API boundary semantics, and no silent failure/logging ambiguity.
