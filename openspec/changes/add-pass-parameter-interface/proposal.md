# add-pass-parameter-interface

## Summary

Extend the `Pass` base class with a uniform interface for exposing tunable shader parameters. This enables internal passes (DownsamplePass, future post-chain passes) to expose runtime-adjustable uniforms through the same mechanism used by FilterPass.

## Problem

Currently, shader parameters are only accessible through FilterPass, which handles RetroArch preset parameters. Internal passes like DownsamplePass have no mechanism to expose tunable uniforms (e.g., filter type selection, blur radius) to the UI layer.

The existing `ShaderParameter` type in `retroarch_preprocessor.hpp` already defines the parameter metadata structure. This proposal reuses that type to provide a consistent interface across all pass types.

## Solution

Add two virtual methods to the `Pass` base class:
- `get_shader_parameters()` - returns parameter metadata for UI rendering
- `set_shader_parameter(name, value)` - updates a parameter value

Default implementations return empty/no-op, allowing passes to opt-in.

## Scope

- **In scope:** Pass interface extension, UI helper for parameter rendering
- **Out of scope:** Actual parameters for DownsamplePass (no shader uniforms exist yet)

## Key Design Decisions

1. **Reuse ShaderParameter** - no new types; matches RetroArch semantics
2. **Default empty implementation** - passes opt-in to parameters
3. **Float-based values** - enums use 0/1/2 internally with UI labels
4. **Separate from pipeline config** - resolution stays as explicit typed API

## Dependencies

- None (builds on existing infrastructure)

## Risks

- **Low:** Minimal interface change with backward-compatible default implementations
