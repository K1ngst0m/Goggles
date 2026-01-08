## 1. CLI and Mode Semantics

- [x] 1.1 Add `--detach` flag to launch workflow
- [x] 1.2 Add `--app-width` and `--app-height` options (integers)
- [x] 1.3 Enforce option dependencies:
  - [x] default mode forces WSI proxy for launched app
  - [x] input forwarding required in default mode
  - [x] detach mode rejects default-only flags (`--app-width/--app-height`)
- [x] 1.4 Ensure `--config` (and default `config/goggles.toml`) remains supported

## 2. Launch Orchestration

- [x] 2.1 In default mode, start viewer with input forwarding enabled
- [x] 2.2 In default mode, launch target app with env:
  - `GOGGLES_CAPTURE=1`
  - `GOGGLES_WSI_PROXY=1`
  - `GOGGLES_WIDTH`/`GOGGLES_HEIGHT` when `--app-width/--app-height` provided
  - `DISPLAY` and `WAYLAND_DISPLAY` from the input-forwarding compositor
- [x] 2.3 Define and document process lifetime rules (who exits when the app exits)

## 3. UX and Documentation

- [x] 3.1 Update launch documentation in `docs/` to use the new simplified workflow
- [x] 3.2 Update `pixi run start --help` guidance (if `scripts/task/start.sh` is updated)

## 4. Verification

- [ ] 4.1 Manual test: default mode launches `vkcube` with input forwarding + WSI proxy
- [ ] 4.2 Manual test: detach mode starts viewer and rejects default-only flags
- [ ] 4.3 Confirm config loading still works (`--config` and default path)

## 5. OpenSpec Hygiene

- [x] 5.1 Pass `openspec validate update-launch-modes-default-detach --strict`
