## Context

Goggles currently exposes filter-chain behavior through internal C++ boundaries in `src/render/chain/` and uses those boundaries directly from the viewer render path. The change introduces a stable public C ABI for Vulkan-based embedding and FFI while preserving runtime behavior, ordering, and ownership semantics already relied on by the renderer.

Key constraints:
- Public contract MUST be ABI-stable across v1.x with additive-only minor evolution.
- Runtime failures MUST remain explicit status returns; no exception-based public flow.
- C API MUST not expose internal pass graph/shader runtime types.
- Host integration MUST keep ownership of host Vulkan objects and synchronization lifecycle.

Stakeholders:
- Goggles renderer maintainers (parity and maintainability)
- embedding/engine integrators (stable lifecycle + record contract)
- FFI authors (string/length, scalar widths, handle ownership, deterministic failure behavior)

## Goals / Non-Goals

**Goals:**
- Define and ship a Vulkan-only v1 public C ABI in `include/goggles_filter_chain.h`.
- Preserve behavior parity with existing filter-chain lifecycle, controls, and record semantics.
- Lock contracts for nullability, ownership, out-parameter behavior, threading, and diagnostics.
- Provide FFI-friendly APIs (`*_ex` length-based paths) and deterministic ABI typing.
- Add conformance tests that freeze v1 contracts and detect regression.

**Non-Goals:**
- Dynamic loader-table API in v1.
- Non-Vulkan backend support in v1.
- Exposing pass-graph internals or shader runtime internals publicly.
- Introducing hidden background worker threads or implicit async preset reload.
- Performing submission/presentation from the record API.

## Decisions

1. **Single-header C ABI with mandatory symbol export**
   - Decision: Export all v1 declarations from `include/goggles_filter_chain.h` using `GOGGLES_CHAIN_API` and `GOGGLES_CHAIN_CALL`, with `goggles_chain_` prefix throughout.
   - Rationale: Reduces integration ambiguity and locks ABI/linkage expectations for static/shared users.
   - Alternatives considered:
     - Loader-table API in v1: rejected to avoid duplicated negotiation surface and avoid symbol-presence ambiguity.
     - Multi-header split by feature: rejected to reduce discoverability and stability.

2. **Status-first error model with optional structured diagnostics**
   - Decision: Fallible functions return `goggles_chain_status_t`; `goggles_chain_error_last_info_get(...)` provides optional numeric diagnostics (`status`, `vk_result`, `subsystem_code`).
   - Rationale: Keeps C ABI deterministic and FFI-friendly while preserving rich troubleshooting without runtime-owned dynamic strings.
   - Alternatives considered:
     - Exception/abort semantics: rejected due incompatible C/FFI behavior.
     - Runtime-owned mutable error strings: rejected due ownership and thread-safety complexity.

3. **Fixed-width scalar typedefs + explicit struct extensibility**
   - Decision: Enum-like API scalars use `uint32_t` typedefs with named constants; extensible structs use `struct_size` prefix contract; `goggles_chain_control_desc_t` layout remains fixed for v1.x.
   - Rationale: Provides deterministic ABI layout and forward-compatible struct growth.
   - Alternatives considered:
     - Native C enums as ABI type: rejected due compiler-dependent width.
     - Extensible control descriptor struct in v1.x: rejected to avoid breaking snapshot layout guarantees.

4. **First-class three-stage model in v1**
   - Decision: Stage domain includes `prechain`, `effect`, and `postchain` now, with order fixed as `prechain -> effect -> postchain`.
   - Rationale: Prevents ABI churn when postchain behavior evolves and preserves deterministic execution semantics.
   - Alternatives considered:
     - Keep postchain implicit/deferred: rejected due likely ABI break in later introduction.

5. **Snapshot-based controls API with explicit ownership**
   - Decision: Expose control lists as owned snapshots and borrowed descriptor pointers valid only for snapshot lifetime.
   - Rationale: Gives stable iteration semantics across FFI boundaries and clear release pairing.
   - Alternatives considered:
     - Iterator/callback listing: rejected due callback reentrancy, ABI callback shape, and language binding friction.

6. **Host-managed synchronization and synchronous preset mutation**
   - Decision: Runtime provides no internal synchronization for a runtime instance; host serializes per-instance calls; preset load remains synchronous and host-managed.
   - Rationale: Preserves current integration model and avoids hidden mutation threads or contention surprises.
   - Alternatives considered:
     - Internal worker threads for reload: rejected due state-race complexity and harder deterministic contracts.

7. **FFI-friendly string surface with `_ex` variants**
   - Decision: Keep C-string APIs and add length-based `_ex` variants for create-time and preset paths with UTF-8 + embedded-NUL validation.
   - Rationale: Supports languages without stable NUL-terminated ownership while retaining ergonomic C call sites.
   - Alternatives considered:
     - C-string-only API: rejected due FFI friction and unsafe buffer assumptions.

8. **Record API as command-record only with hot-path bounds**
   - Decision: `goggles_chain_record_vk(...)` records commands only, does not submit/present, validates frame index, and preserves hot-path constraints (no allocation/I/O/blocking work).
   - Rationale: Keeps host in control of queue/presentation lifecycle and protects frame-time predictability.
   - Alternatives considered:
     - Internal submit/present convenience: rejected due backend coupling and ownership confusion.

9. **Thin C shim over existing C++ internals**
   - Decision: Implement C entrypoints as an adapter layer over current filter-chain boundary behavior, preserving semantics while introducing API contract checks.
   - Rationale: Reduces migration risk and keeps renderer parity while enabling standalone library extraction.
   - Alternatives considered:
     - Rewrite runtime internals first: rejected due larger blast radius and longer parity validation cycle.

## Risks / Trade-offs

- [Risk] ABI drift from header/type/symbol mismatch across toolchains -> Mitigation: add ABI checks (`sizeof`, `offsetof`, typedef widths) plus cross-compiler smoke tests for static/shared linking.
- [Risk] Behavior mismatch between C shim and existing C++ call paths -> Mitigation: parity tests for lifecycle, stage ordering, control listing/clamping, and resize/record flows.
- [Risk] Host misuse of preconditions (layout/frame index/serialization/lifetime) -> Mitigation: strict input validation, explicit failure codes, and contract-focused docs/tests.
- [Risk] Optional diagnostics implemented inconsistently -> Mitigation: capability-gated contract tests for supported/unsupported paths and stable numeric fields.
- [Trade-off] Synchronous preset load favors determinism over background reload latency -> Mitigation: preserve host-managed async orchestration outside ABI and revisit in future additive release.

## Migration Plan

1. Add `include/goggles_filter_chain.h` with v1 ABI types/constants/macros and exported prototypes.
2. Implement C shim mapping from exported functions to existing filter-chain boundary behavior.
3. Keep backend behavior unchanged while switching internal integration call sites to the C shim.
4. Implement `_ex` create/load APIs, capability query, and last-error info query.
5. Add/expand tests for lifecycle matrix, input validation, out-parameter semantics, control contract, and stage ordering.
6. Add record-path contract tests for frame-index bounds and invalid-argument no-command behavior.
7. Add ABI conformance checks and packaging/export verification for shared/static distribution.
8. Freeze v1 ABI and publish integration guidance for hosts and binding authors.

Rollback strategy:
- If compatibility regressions are found before release freeze, disable new C ABI packaging/export and keep internal C++ boundary active while retaining test coverage for discovered gaps.

## Open Questions

- None for v1 scope. Future additive discussions (for later changes) include optional telemetry callbacks and additional `_ex` variants for new string-bearing APIs.
