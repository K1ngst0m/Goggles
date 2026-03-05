## Why

Internal C++ runtime code currently consumes the filter-chain C ABI directly via `goggles_filter_chain.h`, which exposes C-style ownership, status/macro ceremony, and weak typing in C++ callsites. We need a C++20-native public interface for Goggles C++ code so runtime integration is idiomatic, safer, and easier to maintain without changing the C ABI boundary.

## Problem

- C++ runtime callsites rely on C ABI signatures and macros (`struct_size`, status codes, pointer-to-pointer ownership), reducing readability and type safety.
- The current internal API shape does not express C++ ownership and misuse-resistant patterns.
- A direct C-header dependency in internal C++ code makes future C++-first API evolution harder.

## Scope

Wave 1 scope is intentionally narrow and brownfield-safe:

- Add a C++20 wrapper header for the filter-chain API with strong typing and RAII-friendly ownership.
- Keep `goggles_filter_chain.h` as ABI boundary contract for C-facing implementation/tests.
- Migrate runtime internal C++ callsites (anchored at `src/render/backend/vulkan_backend.hpp`) away from direct C-header usage.
- Keep C API contract tests on the C header.

## Non-goals

- Replacing or removing the C ABI (`goggles_filter_chain.h`) in this change.
- Rewriting C API contract tests to use the C++ wrapper.
- Shipping full C API parity in wave 1 (advanced controls/snapshot APIs can be deferred).
- Introducing behavioral or rendering-path feature changes unrelated to API shape/migration.

## What Changes

- Add a C++20 header interface for filter-chain lifecycle/runtime operations (create, preset load, resize, record, stage policy) with strong C++ types and no C-style callsite ceremony.
- Define explicit ownership model (RAII handle) so C++ callsites do not use pointer-to-pointer lifecycle operations.
- Replace internal C++ runtime includes/usages of `goggles_filter_chain.h` within wave-1 scope.
- Preserve C ABI boundary implementation in `src/render/chain/filter_chain_c_api.cpp` and ABI contract tests.
- Define migration fallback policy: if blockers appear, use adapter/shim patterns without reintroducing direct runtime C-header usage.

## Capabilities

### New Capabilities
- `filter-chain-cpp-wrapper`: C++20-native wrapper contract for Goggles internal/public C++ usage of filter-chain runtime operations while preserving C ABI boundary.

### Modified Capabilities
- None.

## Risks

- Wrapper design could drift into a naming-only shim if strong-typing/ownership constraints are not enforced.
- Migration may uncover hidden callsites or coupling that pressure temporary regressions to direct C-header usage.
- C++ wrapper and C ABI behavior could diverge without parity validation.

## Validation Plan

- Verify include boundary directly:
  - `grep -R --line-number '#include "goggles_filter_chain.h"' src/render/backend`
  - `grep -R --line-number '#include <goggles_filter_chain.h>' src/render/backend`
  - Expected result after migration: no matches in runtime backend paths.
- Run policy-aligned verification commands:
  - `pixi run build -p asan`
  - `pixi run test -p asan`
  - `pixi run build -p quality`
- Verify C ABI contract coverage remains green:
  - `ctest --preset asan -R "goggles_unit_tests" --output-on-failure`
- Add/adjust C++-focused checks validating no C-style surface leakage in wrapper-facing callsites.

## Divergence Handling

- Default path for hidden callsite blockers is adapter/shim remediation that preserves C++ runtime boundaries.
- If behavioral/spec divergence is required, apply work MUST halt and proposal/design/spec artifacts MUST be reconciled before implementation continues.

## Impact

- **Code modules/files**: `src/render/chain/include/` (new C++ wrapper header), `src/render/backend/` runtime callsites, potential wrapper support code in `src/render/chain/`.
- **C ABI boundary**: `src/render/chain/filter_chain_c_api.cpp` remains authoritative bridge and must preserve behavior.
- **Tests**: `tests/render/test_filter_chain_c_api_contracts.cpp` stays C-header based; additional C++ wrapper tests may be added for migrated surface.
- **Build/install**: render include/install rules may need to export/install the new C++ header alongside existing C header.
- **OpenSpec artifacts impacted**:
  - Delta spec introduced by this change: `openspec/changes/cpp20-filter-chain-hpp-wrapper/specs/filter-chain-cpp-wrapper/spec.md`
  - Proposed living-spec sync target after archive/sync: `openspec/specs/filter-chain-cpp-wrapper/spec.md`
- **Policy-sensitive areas**:
  - Error handling MUST remain `Result`-style for expected failures (no exception-driven runtime failure model).
  - Ownership/lifetime semantics MUST be explicit and RAII-friendly.
  - Vulkan API split and existing threading policies MUST remain unchanged.
