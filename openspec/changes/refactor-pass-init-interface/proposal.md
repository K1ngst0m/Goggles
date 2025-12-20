# Proposal: Refactor Pass Init Interface

## Summary
Refactor the `Pass` base class and derived classes (`OutputPass`, `FilterPass`) to use flat config structs and a shared `VulkanContext`, reducing parameter bloat and making the interface more maintainable.

## Problem
`FilterPass::init_from_sources()` has **10 parameters**, which:
- Is hard to read and maintain
- Makes call sites verbose and error-prone
- Mixes different concerns (Vulkan context, pass config, shader sources)

Additionally, the current base class `Pass::init()` is a poor abstraction:
- `FilterPass::init()` returns an error telling callers to use `init_from_sources()` instead
- The signature doesn't fit all pass types

## Solution
1. **Introduce `VulkanContext` struct** - Groups device handles shared across all passes
2. **Flat config structs per pass type** - `OutputPassConfig`, `FilterPassConfig` with no inheritance
3. **Remove `init()` from base `Pass` class** - Each pass defines its own typed `init()` method
4. **Keep common interface** - `shutdown()` and `record()` remain virtual in base

## Before/After

**Before (10 args):**
```cpp
pass->init_from_sources(
    m_device, m_physical_device, format, num_sync,
    *m_shader_runtime, vs, fs, name, filter_mode, params);
```

**After (3 args):**
```cpp
pass->init(m_vk_ctx, *m_shader_runtime, config);
```

## Scope
- `src/render/chain/pass.hpp` - Base class, add `VulkanContext`
- `src/render/chain/output_pass.hpp/cpp` - Add `OutputPassConfig`, update `init()`
- `src/render/chain/filter_pass.hpp/cpp` - Add `FilterPassConfig`, replace `init_from_sources()`
- `src/render/chain/filter_chain.cpp` - Update call sites
- `src/render/backend/vulkan_backend.hpp/cpp` - Store/pass `VulkanContext`

## Non-Goals
- No inheritance between config structs (per user request)
- Not changing `PassContext` (runtime context, already well-designed)
- Not changing `record()` or `shutdown()` signatures
