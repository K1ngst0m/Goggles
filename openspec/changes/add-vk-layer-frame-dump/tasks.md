## 1. Specification

- [x] 1.1 Add `GOGGLES_DUMP_DIR` + `GOGGLES_DUMP_FRAME_MODE` + `GOGGLES_DUMP_FRAME_RANGE` requirements to `vk-layer-capture`
- [x] 1.2 Specify `{processname}_{frameid}.ppm` + `{processname}_{frameid}.ppm.desc` output rules and metadata key/value format
- [x] 1.3 Add CLI requirements for default mode flags that map to `GOGGLES_DUMP_*` for the launched app

## 2. Implementation (vk_layer)

- [x] 2.0 Determine `processname` for filename prefix (sanitize for filesystem safety)
- [x] 2.1 Parse `GOGGLES_DUMP_FRAME_MODE` (default `ppm`)
- [x] 2.2 Parse `GOGGLES_DUMP_DIR` (default `/tmp/goggles_dump`)
- [x] 2.3 Parse `GOGGLES_DUMP_FRAME_RANGE` (single/list/range); treat empty as disabled
- [x] 2.4 In non-WSI-proxy path, dump the presented swapchain image when its frame number matches
- [x] 2.5 In WSI proxy path, dump the presented virtual swapchain image when its frame number matches
- [x] 2.6 For each dumped frame, write `{processname}_{frameid}.ppm` and `{processname}_{frameid}.ppm.desc`
- [x] 2.7 Ensure `vkQueuePresentKHR` performs no file I/O and no logging (off-thread write path)
- [x] 2.8 Ensure dumping failures are best-effort and never break present/capture behavior

## 3. Validation

- [x] 3.1 Run `openspec validate add-vk-layer-frame-dump --strict` (pending: `openspec` CLI not available in this environment)
- [x] 3.2 Run unit tests (`pixi run test -p test`) if impacted tests exist
