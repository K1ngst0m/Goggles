## 1. Wrapper API Surface (Wave 1)

- [x] 1.1 Add C++20 wrapper header in `src/render/chain/include/` for lifecycle/runtime operations (create, preset load, resize, record, stage policy) with typed enums and RAII ownership.
- [x] 1.2 Implement wrapper support code in `src/render/chain/` that bridges to existing C ABI calls without changing ABI behavior.
- [x] 1.3 Ensure every fallible wrapper operation returns project `Result` (`tl::expected` alias), uses no expected-failure exceptions, and follows boundary logging rules (no duplicate cascading logs).
- [x] 1.4 Ensure wrapper-facing runtime signatures use `vk::` conventions and do not expose raw `Vk*` handles or C-header ceremony to C++ consumers.

## 2. Runtime Integration Migration

- [x] 2.1 Migrate wave-1 internal runtime callsites (starting from `src/render/backend/vulkan_backend.hpp` and related backend integration points) to include/use the new C++ wrapper.
- [x] 2.2 Keep `src/render/chain/filter_chain_c_api.cpp` as C ABI boundary on `goggles_filter_chain.h`.
- [x] 2.3 Keep `tests/render/test_filter_chain_c_api_contracts.cpp` on C header to preserve ABI contract validation.
- [x] 2.4 If hidden blocker callsites appear, apply adapter/shim strategy that preserves runtime C++ wrapper boundary (no direct runtime C-header regression) and record evidence fields: blocker path, shim/adaptor entrypoint, and follow-up removal task ID.
- [x] 2.5 Preserve stage policy behavior so wrapper migration does not alter invariant execution order (`pre-chain -> effect chain -> output pass`).

## 3. Boundary Enforcement and Verification

- [x] 3.1 Add/update include-boundary checks, with explicit command and expectation:
  - `grep -R --line-number '#include "goggles_filter_chain.h"' src/render/backend`
  - `grep -R --line-number '#include <goggles_filter_chain.h>' src/render/backend`
  - Expected result after migration: no matches in `src/render/backend`.
- [x] 3.2 Run policy-aligned build/test verification:
  - `pixi run build -p asan`
  - `pixi run test -p asan`
  - `pixi run build -p quality`
- [x] 3.3 Verify C ABI contract tests with explicit commands:
  - `ctest --preset asan -R "goggles_unit_tests" --output-on-failure`
- [x] 3.4 Add/update static policy checks with explicit commands:
  - `grep -R --line-number '\bthrow\b' src/render/backend src/render/chain/include src/render/chain`
  - `grep -R --line-number 'std::thread\|std::jthread' src/render/backend src/render/chain`
  - Expected result after migration: no new expected-failure exception paths and no render-path thread primitives in migrated scope.
- [x] 3.5 Add/update wrapper-focused tests (for example under `tests/render/`) validating RAII destruction behavior and no pointer-to-pointer lifecycle leakage, and run:
  - `ctest --preset asan -R "goggles_unit_tests" --output-on-failure`
- [x] 3.6 Add/update wrapper logging tests that validate exactly one boundary log event and zero duplicate lower-layer logs for representative wrapper failure paths.
  - `ctest --preset asan -R "goggles_unit_tests" --output-on-failure`
- [x] 3.7 Add/update static signature checks for wrapper-facing headers:
  - `grep -R --line-number 'Vk[A-Za-z0-9_]*' src/render/chain/include --include='*.hpp'`
  - Expected result after migration: wrapper-facing filter-chain API surface has no raw `Vk*` parameter/return signatures.
- [x] 3.8 Add/update stage-policy regression tests asserting invariant order (`pre-chain -> effect chain -> output pass`) under all-enabled and subset-enabled masks.
  - `ctest --preset asan -R "goggles_unit_tests" --output-on-failure`

## 4. Spec/Design Consistency and Handoff

- [x] 4.1 Confirm implementation matches `openspec/changes/cpp20-filter-chain-hpp-wrapper/specs/filter-chain-cpp-wrapper/spec.md` scenarios and `design.md` decisions.
- [x] 4.2 If implementation requires behavior divergence, `/goggles-apply` MUST stop and wait for proposal/spec/design reconciliation before any further implementation; automatic implementation-side spec rewrites are forbidden.
- [x] 4.3 Prepare apply handoff note with touched files, verification evidence, and deferred wave-2 scope.

## 5. Requirement Traceability

- [x] 5.1 Keep this mapping updated during apply so each requirement/scenario has at least one implementation task and one verification command.

| Requirement (spec.md) | Task IDs | Verification commands |
| --- | --- | --- |
| C++20 Wrapper Header for Filter Chain | 1.1, 2.1 | `pixi run build -p asan` |
| RAII Ownership for Chain Handle | 1.1, 1.2, 3.5 | `ctest --preset asan -R "goggles_unit_tests" --output-on-failure` |
| Strongly Typed C++ Runtime Surface | 1.1, 1.4, 2.1, 3.5 | `ctest --preset asan -R "goggles_unit_tests" --output-on-failure`; `pixi run build -p quality` |
| Result-Based Error Propagation | 1.3, 3.4 | `grep -R --line-number '\bthrow\b' src/render/backend src/render/chain/include src/render/chain` |
| Single-Boundary Error Logging | 1.3, 3.6, 4.1 | `ctest --preset asan -R "goggles_unit_tests" --output-on-failure` |
| App-Side Vulkan Type Conventions | 1.4, 3.7, 4.1 | `grep -R --line-number 'Vk[A-Za-z0-9_]*' src/render/chain/include --include='*.hpp'` |
| Boundary Isolation | 2.1, 2.2, 3.1 | `grep -R --line-number '#include "goggles_filter_chain.h"' src/render/backend`; `grep -R --line-number '#include <goggles_filter_chain.h>' src/render/backend` |
| Stage Policy Preserves Pipeline Order Invariants | 2.5, 3.8, 4.1 | `ctest --preset asan -R "goggles_unit_tests" --output-on-failure` |
| C ABI Continuity During Migration | 2.2, 2.3, 3.3 | `ctest --preset asan -R "goggles_unit_tests" --output-on-failure` |
| Non-Regressive Blocker Handling | 2.4, 4.2, 4.3 | `grep -R --line-number 'blocker path, shim/adaptor entrypoint, and follow-up removal task ID' openspec/changes/cpp20-filter-chain-hpp-wrapper/tasks.md` |
