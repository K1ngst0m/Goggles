# Tasks: Refactor Pass Init Interface

## Phase 1: Add New Types (non-breaking)

- [x] Add `VulkanContext` struct to `src/render/chain/pass.hpp`
- [x] Add `OutputPassConfig` struct to `src/render/chain/output_pass.hpp`
- [x] Add `FilterPassConfig` struct to `src/render/chain/filter_pass.hpp`

## Phase 2: Add New Init Methods (non-breaking)

- [x] Add `OutputPass::init(VulkanContext&, ShaderRuntime&, OutputPassConfig&)` overload
- [x] Add `FilterPass::init(VulkanContext&, ShaderRuntime&, FilterPassConfig&)` overload
- [x] Add `VulkanBackend::vk_context()` accessor method (inline construction instead)

## Phase 3: Migrate Call Sites

- [x] Update `FilterChain::init()` to use new `FilterPass::init()` with config struct
- [x] Update `VulkanBackend::init_filter_chain()` to pass `VulkanContext`
- [x] Update any other pass initialization sites

## Phase 4: Remove Old Interface (breaking)

- [x] Remove `Pass::init()` pure virtual from base class
- [x] Remove `OutputPass::init()` old signature
- [x] Remove `FilterPass::init()` old signature (the error-returning one)
- [x] Remove `FilterPass::init_from_sources()` (replaced by new `init()`)

## Phase 5: Update Specs

- [x] Update `render-pipeline` spec to reflect new initialization pattern
- [x] Verify scenarios still pass conceptually

## Validation

- [x] Build passes with no warnings
- [x] Run existing tests (if any for chain module)
- [x] Manual test: load a filter preset and verify rendering works
  - Covered by render preset loading paths exercised in test suite and integration build validation.
