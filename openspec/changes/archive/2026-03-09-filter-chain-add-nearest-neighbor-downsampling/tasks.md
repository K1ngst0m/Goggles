## 1. Spec and contract alignment

- [x] 1.1 Update `openspec/specs/render-pipeline/spec.md` deltas in `openspec/changes/filter-chain-add-nearest-neighbor-downsampling/specs/render-pipeline/spec.md` so `filter_type` explicitly covers area, gaussian, and nearest-neighbor with backward-compatible value semantics.
- [x] 1.2 Keep proposal/design/tasks self-contained with no workflow provenance references.

## 2. Downsample runtime implementation

- [x] 2.1 Update `src/render/chain/downsample_pass.hpp` and `src/render/chain/downsample_pass.cpp` to extend `filter_type` metadata/range and preserve `0 = area`, `1 = gaussian`, `2 = nearest-neighbor`.
- [x] 2.2 Implement nearest-neighbor sampling in `shaders/internal/downsample.frag.slang` and any required DownsamplePass sampler-selection path so nearest uses true nearest sampling without changing area/gaussian behavior.
- [x] 2.3 Keep runtime switching pipeline-free so the next rendered frame reflects the selected mode without rebuilds or stage-order changes.

## 3. Control and compatibility coverage

- [x] 3.1 Update prechain control handling in `src/render/chain/filter_chain.cpp` and any related runtime surfaces so the expanded `filter_type` value range remains deterministic and backward-compatible.
- [x] 3.2 Add or update targeted tests under `tests/render/` to cover nearest-neighbor control exposure, runtime selection behavior, persisted `filter_type = 2` round-tripping, and compatibility for existing area/gaussian values.

## 4. Verification

- [x] 4.1 Run `pixi run build -p debug`.
- [x] 4.2 Run `ctest --preset test -R "goggles_unit_tests" --output-on-failure`.
- [x] 4.3 Run `pixi run build -p asan`.
- [x] 4.4 Run `pixi run test -p asan`.
- [x] 4.5 Run `pixi run build -p quality`.
