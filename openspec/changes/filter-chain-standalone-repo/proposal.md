## Why

`goggles-filter-chain` currently builds as a boundary target inside this monorepo, but it relies on internal build wiring, shared utility ownership, and monorepo-centric release workflows. Extracting the filter runtime as an independently versioned GitHub repository enables shipping it with stable, published C API guarantees while decoupling its release lifecycle from the host application.

## Problem

- The filter runtime boundary (`chain + shader + texture`) is decoupled from app/UI/backend ownership, but packaging and release contracts are still monorepo-bound.
- Shared `src/util` dependencies are broad and not scoped as a reusable core dependency contract.
- ABI and release checks are tied to monorepo test/build orchestration rather than a standalone publication flow.

## Scope

- Define requirements and design to extract the filter runtime into a standalone repository.
- Keep C API behavior and compatibility stable while moving packaging/release ownership.
- Define and apply a `util-core` split strategy for dependencies needed by filter runtime modules.
- Define standalone build, test, and versioned artifact publication gates for milestone 1.

## What Changes

- Define standalone-repo requirements for `goggles-filter-chain` extraction and host consumption boundaries.
- Define C API compatibility and release obligations for standalone publishing.
- Define dependency-management requirements for `util-core` extraction and host/runtime dependency wiring.
- Define milestone-1 release profile (Linux x86_64, clang/gcc static/shared validation lanes) and approved publication channels.
- Define artifact integrity and publisher authenticity requirements for publication and host consumption.
- Define a phased migration and verification plan that preserves behavior and avoids host regressions.

## Capabilities

### New Capabilities

- None.

### Modified Capabilities

- `goggles-filter-chain`: extend standalone target requirements into standalone repository ownership, util-core dependency boundaries, and publishable release gates.
- `filter-chain-c-api`: extend ABI/API compatibility policy to standalone publication and artifact versioning workflow.
- `dependency-management`: define how `util-core` and standalone filter artifacts are consumed in Pixi/CMake workflows without violating existing reproducibility constraints.

## Non-goals

- Rewriting app/UI/compositor architecture outside required boundary updates.
- Changing the host runtime behavior contract for filter stage ordering and frame processing semantics.
- Introducing breaking C API changes in milestone 1.

## Risks

- Incorrect util-core scoping can create cyclic ownership or duplicate utility divergence.
- Incomplete ABI regression checks can silently break downstream consumers.
- Packaging/distribution changes can drift from Pixi/CMake policy if not normalized to preset-based workflows.
- Migration order mistakes can destabilize host backend integration during transition.

## Validation Plan

- Require standalone CI build and scoped test pass for the extracted runtime.
- Require C API compatibility checks against baseline v1 expectations.
- Require versioned release artifact generation and publication checks.
- Require host integration verification against the boundary-facing runtime contract.

## Policy-Sensitive Impacts

- **Error handling / logging**: Runtime extraction and util-core split MUST preserve `Result`-based fallible contracts and single-boundary logging behavior; no exception-based expected failure paths are introduced.
- **Threading model**: This change MUST preserve existing render/pipeline concurrency constraints (JobSystem-based work, no direct render-path thread ownership changes).
- **Vulkan/lifetime boundaries**: Boundary extraction MUST preserve existing C API and runtime ownership contracts; no raw-handle policy regressions are introduced for app-side code.
- **Build/test policy**: Host integration verification remains preset/Pixi-driven, and standalone publication adds CI gates without bypassing reproducibility requirements.

## Impact

- **Impacted modules/files**: `src/render/chain/`, `src/render/shader/`, `src/render/texture/`, `src/util/` (util-core subset), build surfaces in `src/render/CMakeLists.txt` and `pixi.toml`.
- **Impacted APIs**: `src/render/chain/api/c/goggles_filter_chain.h` compatibility and publication contract; C++ wrapper behavior where needed to preserve C API contract.
- **Impacted systems**: standalone packaging/release workflow and host dependency wiring.
- **Impacted OpenSpec specs**: `openspec/specs/goggles-filter-chain/spec.md`, `openspec/specs/filter-chain-c-api/spec.md`, `openspec/specs/dependency-management/spec.md`.
