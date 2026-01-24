# add-pass-parameter-interface

## Summary

Extend the `Pass` base class with a uniform interface for exposing tunable shader parameters. This enables internal passes (DownsamplePass, future post-chain passes) to expose runtime-adjustable uniforms through the same mechanism used by FilterPass.

To validate the interface, DownsamplePass gains a runtime-selectable filter type parameter.

## Problem

Currently, shader parameters are only accessible through FilterPass, which handles RetroArch preset parameters. Internal passes like DownsamplePass have no mechanism to expose tunable uniforms (e.g., filter type selection) to the UI layer.

The existing `ShaderParameter` type in `retroarch_preprocessor.hpp` already defines the parameter metadata structure. This proposal reuses that type to provide a consistent interface across all pass types.

## Solution

1. Add two virtual methods to the `Pass` base class:
   - `get_shader_parameters()` - returns parameter metadata for UI rendering
   - `set_shader_parameter(name, value)` - updates a parameter value

2. Implement `filter_type` parameter in DownsamplePass with two options:
   - **0 = Area** (current behavior) - weighted average of covered source pixels
   - **1 = Gaussian** - Gaussian-weighted bilinear sampling (4 bilinear taps = 16 texels)

Default implementations return empty/no-op, allowing passes to opt-in.

## Scope

- **In scope:** Pass interface extension, UI helper, DownsamplePass filter_type parameter
- **Out of scope:** Additional filter types beyond area and gaussian

## Key Design Decisions

1. **Reuse ShaderParameter** - no new types; matches RetroArch semantics
2. **Default empty implementation** - passes opt-in to parameters
3. **Float-based values** - filter type uses 0.0/1.0 internally, UI shows labels
4. **Separate from pipeline config** - resolution stays as explicit typed API
5. **Gaussian filter name** - clean user-facing name; bilinear optimization is impl detail

## Filter Type Details

| Value | Name | Description |
|-------|------|-------------|
| 0 | Area | Box filter with coverage weighting (current) |
| 1 | Gaussian | 4 bilinear taps approximating Gaussian kernel (16 texels effective) |

The Gaussian filter uses strategically placed bilinear samples to approximate a Gaussian kernel. Each bilinear tap averages 4 texels via hardware filtering, so 4 taps effectively sample 16 texels with minimal ALU cost.

## Dependencies

- None (builds on existing infrastructure)

## Risks

- **Low:** Minimal interface change with backward-compatible default implementations
- **Low:** Shader change is additive (branch on filter_type uniform)
