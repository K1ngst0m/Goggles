# Change: Remove Bundled GPL Cursor Theme Assets

## Problem

Goggles currently depends on bundled cursor theme files under `assets/cursor` for compositor software cursor rendering, and those assets are shipped in AppImage payloads. This keeps GPL-licensed cursor-theme content in the distributed artifact and creates avoidable license friction for project-level licensing decisions.

## Why

We need cursor rendering that is license-clean, behavior-stable, and independent of repository-shipped theme packs. Eliminating bundled GPL cursor assets reduces compliance risk while preserving current input-forwarding and software-cursor UX.

## Scope

- Replace hard dependency on bundled `assets/cursor` with a runtime cursor source chain.
- Preserve current compositor cursor behavior (visibility, hotspot alignment, pointer-lock hiding).
- Remove cursor-theme packaging from shipped artifacts.
- Update OpenSpec contracts for input-forwarding and packaging behavior.

## Non-goals

- Reworking pointer forwarding semantics, lock/confine behavior, or overlay visibility rules.
- Introducing new cursor customization UI.
- Broad compositor architecture refactors outside cursor-image sourcing and packaging paths.
- Changing shader/resource packaging beyond cursor-theme removal.

## What Changes

### Runtime cursor source strategy

Implement a deterministic cursor source order for compositor software cursor imagery:

1. Runtime-provided cursor image + hotspot from active session cursor state (when available).
2. System cursor lookup by standard cursor name (no bundled theme path required).
3. Built-in generated fallback cursor image + hotspot to guarantee startup behavior.

### Packaging and resource path updates

- Stop requiring `assets/cursor` in runtime resource discovery.
- Stop shipping `assets/cursor` in AppImage staging.
- Keep other packaged assets behavior unchanged.

### Contract updates

- Modify `input-forwarding` spec to require runtime/system/fallback cursor sourcing instead of bundled theme assets.
- Add packaging requirement that distributed artifacts do not include bundled cursor-theme assets.

## Impact

- **Affected specs:** `input-forwarding`, `packaging`
- **Affected code (expected):**
  - `src/compositor/compositor_server.cpp`
  - `src/compositor/compositor_server.hpp`
  - `src/app/application.cpp`
  - `scripts/task/appimage_stage.sh`
  - optional: `assets/cursor/*` removal and related docs updates

## Policy-sensitive impacts

- **Error handling:** Cursor source selection and fallbacks MUST return/propagate structured failures via `Result<T>` and avoid silent fallback failures.
- **Logging:** Cursor-source transitions and hard failures SHOULD be logged once with actionable context (source selected, fallback reason).
- **Threading:** No new direct `std::thread`/`std::jthread` usage in render/pipeline paths.
- **Ownership/lifetime:** Cursor pixel buffers/textures MUST retain explicit ownership and cleanup parity with current compositor texture lifecycle.

## Risks

| Risk | Severity | Likelihood | Mitigation |
|------|----------|------------|------------|
| Runtime cursor source unavailable in some environments | HIGH | MEDIUM | Enforce built-in generated fallback and startup-path tests |
| Hotspot mismatch causes perceived cursor offset | MEDIUM | MEDIUM | Add hotspot alignment checks in targeted cursor rendering tests |
| Packaging regression removes required non-cursor assets | MEDIUM | LOW | Add AppImage staging assertions scoped to cursor paths only |
| Behavior drift during pointer lock or overlay visibility | MEDIUM | LOW | Re-run existing input-forwarding and software-cursor regression checks |

## Validation Plan

1. `pixi run build -p test` succeeds.
2. `pixi run test -p test` passes relevant input-forwarding/compositor tests.
3. AppImage staging output does not contain `usr/share/goggles/assets/cursor`.
4. Manual runtime checks confirm:
   - software cursor renders with correct hotspot when unlocked,
   - software cursor hides during pointer lock,
   - software cursor hides when overlay is visible,
   - fallback cursor appears when system cursor lookup is unavailable.
