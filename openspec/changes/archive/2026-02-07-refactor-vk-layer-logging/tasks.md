## 1. Implementation
- [x] 1.1 Add a single vk-layer logging header providing `LAYER_*` macros (interface unchanged)
- [x] 1.2 Implement `write(2)` backend with fixed-size buffer and no allocations
- [x] 1.3 Implement runtime env gating:
  - [x] `GOGGLES_DEBUG_LOG` (default off)
  - [x] `GOGGLES_DEBUG_LOG_LEVEL` (default info when enabled)
- [x] 1.4 Add anti-spam macros (once / every-N) suitable for hot code paths
- [x] 1.5 Replace per-file `fprintf` macros in `src/capture/vk_layer/` with the shared header
- [x] 1.6 Ensure `vkQueuePresentKHR` contains no logging calls
- [x] 1.7 Add Goggles CLI flags to forward to launched app:
  - [x] `--layer-log` -> `GOGGLES_DEBUG_LOG=1`
  - [x] `--layer-log-level <level>` -> `GOGGLES_DEBUG_LOG_LEVEL=<level>` (implies `--layer-log`)
- [x] 1.8 Update capture layer logging policy docs

## 2. Tests
- [x] 2.1 Add Catch2 unit tests for vk-layer logging (including stderr capture via `dup2` + pipe)
- [x] 2.2 Cover truncation edge case (message exceeds buffer)
- [x] 2.3 Cover anti-spam behavior under repeated calls
- [x] 2.4 Add Catch2 unit tests for new CLI flags (parse + detach rejection)

## 3. Validation
- [x] 3.1 `pixi run format`
- [x] 3.2 `pixi run build -p test`
- [x] 3.3 `pixi run test -p test`
