# Change: Fix Slang unsized array declaration compatibility

## Why

Some RetroArch shaders use `int[] var = int[](...)` syntax for array declarations, which Slang's GLSL frontend rejects with error `expected an expression of type 'int[]', got 'int[N]'`. This prevents shaders like `vhs_font.slang` from compiling.

## What Changes

- Add `fix_unsized_array_decl()` to the RetroArch preprocessor
- Transform `type[] var = type[](` to `const type var[] = type[](`
- Apply fix in `fix_slang_compat()` alongside existing matrix/compound fixes

## Impact

- Affected specs: render-pipeline
- Affected code: `src/render/shader/retroarch_preprocessor.cpp`
- Breaking changes: none
