## 1. Runtime Cursor Source Refactor

- [x] 1.1 Replace bundled-theme loading in `src/compositor/compositor_server.cpp` with runtime/system/fallback cursor source selection.
- [x] 1.2 Remove hard requirement for `XCURSOR_PATH=<resource>/assets` in `src/app/application.cpp` while preserving cursor size and visibility semantics.
- [x] 1.3 Add deterministic fallback cursor generation path (image + hotspot) so compositor startup does not depend on external theme files.
- [x] 1.4 Keep pointer-lock and overlay visibility behavior unchanged in compositor cursor rendering paths.

## 2. Packaging and Asset Surface Updates

- [x] 2.1 Update `scripts/task/appimage_stage.sh` so AppImage payload excludes bundled cursor theme assets.
- [x] 2.2 Remove or relocate `assets/cursor/*` from tracked runtime payload inputs according to final implementation plan.
- [x] 2.3 Verify packaged runtime still resolves required non-cursor assets (config/shaders) after cursor asset removal.

## 3. Spec and Documentation Updates

- [x] 3.1 Add `input-forwarding` spec delta to replace bundled cursor-theme requirement with runtime/system/fallback sourcing contract.
- [x] 3.2 Add `packaging` spec delta requiring cursor-theme assets to be excluded from distributed artifacts.
- [x] 3.3 Update user-facing docs/config comments that currently imply bundled cursor theme dependency.

## 4. Verification

- [x] 4.1 Build verification: `pixi run build -p test`.
- [x] 4.2 Test verification: `pixi run test -p test`.
- [x] 4.3 Packaging verification: stage AppImage and assert `usr/share/goggles/assets/cursor` is absent.
- [ ] 4.4 Runtime verification (manual): confirm software cursor render/hide behavior in unlocked, pointer-locked, and overlay-visible states.
- [ ] 4.5 Fallback verification (manual): simulate unavailable system cursor source and confirm generated fallback cursor renders with correct hotspot.
