## 1. CLI and Mode Semantics

- [ ] 1.1 Add `--detach` flag to launch workflow
- [ ] 1.2 Add `--app-width` and `--app-height` options (integers)
- [ ] 1.3 Enforce option dependencies:
  - [ ] default mode forces WSI proxy for launched app
  - [ ] input forwarding required in default mode
  - [ ] detach mode rejects default-only flags (`--app-width/--app-height`)
- [ ] 1.4 Ensure `--config` (and default `config/goggles.toml`) remains supported

## 2. Launch Orchestration

- [ ] 2.1 In default mode, start viewer with input forwarding enabled
- [ ] 2.2 In default mode, launch target app with env:
  - `GOGGLES_CAPTURE=1`
  - `GOGGLES_WSI_PROXY=1`
  - `GOGGLES_WIDTH`/`GOGGLES_HEIGHT` when `--app-width/--app-height` provided
  - `DISPLAY` and `WAYLAND_DISPLAY` from the input-forwarding compositor
- [ ] 2.3 Define and document process lifetime rules (who exits when the app exits)

## 3. UX and Documentation

- [ ] 3.1 Update launch documentation in `docs/` to use the new simplified workflow
- [ ] 3.2 Update `pixi run start --help` guidance (if `scripts/task/start.sh` is updated)

## 4. Verification

- [ ] 4.1 Manual test: default mode launches `vkcube` with input forwarding + WSI proxy
- [ ] 4.2 Manual test: detach mode starts viewer and rejects default-only flags
- [ ] 4.3 Confirm config loading still works (`--config` and default path)

## 5. OpenSpec Hygiene

- [ ] 5.1 Pass `openspec validate update-launch-modes-default-detach --strict`

