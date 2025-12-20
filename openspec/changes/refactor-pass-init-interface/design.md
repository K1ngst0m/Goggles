# Design: Pass Init Interface Refactor

## Current State

```
Pass (base)
├── init(device, format, num_sync, runtime, shader_dir) -> pure virtual
├── shutdown() -> pure virtual
└── record(cmd, ctx) -> pure virtual

OutputPass : Pass
└── init(...) -> loads shaders from shader_dir ✓

FilterPass : Pass
├── init(...) -> returns error "use init_from_sources()" ✗
└── init_from_sources(device, physical_device, format, num_sync,
                       runtime, vs, fs, name, filter_mode, params)
    └── 10 parameters!
```

**Problems:**
1. Base class contract violated by `FilterPass`
2. Parameter bloat makes maintenance difficult
3. Device handles repeated across every init call

## Proposed Structure

```
VulkanContext {device, physical_device}  <- shared, lives in VulkanBackend

Pass (base)
├── shutdown() -> pure virtual
└── record(cmd, ctx) -> pure virtual
    (no init - each pass has typed init)

OutputPassConfig {target_format, num_sync_indices, shader_dir}

OutputPass : Pass
└── init(VulkanContext&, ShaderRuntime&, OutputPassConfig&) -> Result<void>

FilterPassConfig {target_format, num_sync_indices, vertex_source,
                  fragment_source, shader_name, filter_mode, parameters}

FilterPass : Pass
└── init(VulkanContext&, ShaderRuntime&, FilterPassConfig&) -> Result<void>
```

## Key Decisions

### 1. No Config Inheritance
Flat structs are simpler. `OutputPassConfig` and `FilterPassConfig` have different fields - forcing them into a hierarchy adds complexity without benefit.

### 2. `VulkanContext` as Separate Struct
- Device handles are **immutable** after creation
- Same handles used by all passes
- Could live as member in `VulkanBackend`, passed by reference
- Alternative: store in `Pass` constructor - rejected because it couples lifetime

### 3. Remove `init()` from Base Class
- Current design already broken (FilterPass doesn't implement it properly)
- Passes have fundamentally different initialization needs
- Type-erased config (variant/any) loses compile-time safety
- Each pass knows its concrete type at construction site anyway

### 4. Keep `ShaderRuntime&` as Separate Parameter
- Not part of Vulkan context (higher-level abstraction)
- Already exists as `unique_ptr` in `VulkanBackend`
- Pass by reference, not ownership

## Data Flow

```
VulkanBackend
├── m_device, m_physical_device -> VulkanContext (struct, stack)
├── m_shader_runtime            -> passed by ref to init()
│
├── FilterChain
│   └── init(vk_ctx, runtime, preset_path)
│       └── for each pass in preset:
│           └── FilterPass::init(vk_ctx, runtime, FilterPassConfig{...})
│
└── OutputPass (future: could be in chain too)
    └── init(vk_ctx, runtime, OutputPassConfig{...})
```

## Migration Path

1. Add `VulkanContext` struct to `pass.hpp`
2. Add config structs to respective headers
3. Add new `init()` overloads alongside old methods
4. Update call sites to use new interface
5. Remove old `init_from_sources()` and base class `init()`
6. Update render-pipeline spec
