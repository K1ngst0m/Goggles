## ADDED Requirements

### Requirement: Cursor Theme Asset Exclusion

The packaging workflow SHALL NOT ship bundled cursor-theme assets as part of Goggles distribution
artifacts.

The packaging workflow SHALL:
- Exclude `assets/cursor` from AppImage payload content.
- Keep existing required packaged assets (configuration templates and shader assets) intact.
- Preserve software cursor runtime behavior through runtime/system/fallback cursor sourcing.

#### Scenario: AppImage payload excludes bundled cursor theme assets
- **GIVEN** AppImage staging is executed for a release build
- **WHEN** the staged payload tree is inspected
- **THEN** `usr/share/goggles/assets/cursor` is absent
- **AND** other expected asset roots remain present

#### Scenario: Viewer still starts with software cursor after packaging change
- **GIVEN** a packaged Goggles runtime without bundled cursor theme assets
- **WHEN** the viewer starts and presents a forwarded surface
- **THEN** software cursor rendering remains available through runtime/system/fallback cursor sourcing
