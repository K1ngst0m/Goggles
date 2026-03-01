## MODIFIED Requirements

### Requirement: Compositor Software Cursor

The system SHALL render a software cursor inside compositor-presented frames for the focused
surface.

The compositor server SHALL:
- Track cursor position in surface-local coordinates.
- Render the cursor overlay into the compositor-presented frame buffer.
- Hide the software cursor when pointer lock is active or when input forwarding is suspended.
- Source cursor imagery via runtime cursor providers without requiring bundled cursor theme assets.
- Use a deterministic fallback chain: runtime cursor image when available, then system cursor lookup,
  then a built-in generated cursor image.
- Preserve hotspot-correct placement for all cursor sources.

#### Scenario: Cursor visible for compositor surface
- **GIVEN** the compositor is presenting a surface frame
- **AND** the focused surface does not hold a pointer lock
- **WHEN** pointer forwarding is active
- **THEN** a software cursor is rendered into the presented frame
- **AND** the cursor position matches the compositor cursor coordinates used for pointer events

#### Scenario: Cursor hidden during pointer lock
- **GIVEN** a focused surface activates `zwp_pointer_constraints_v1.lock_pointer`
- **WHEN** pointer lock is active
- **THEN** the software cursor is hidden
- **AND** only relative pointer events continue to be delivered

#### Scenario: Runtime cursor source unavailable
- **GIVEN** no runtime cursor image is available from the active session cursor source
- **WHEN** the compositor needs a cursor image for rendering
- **THEN** the compositor attempts system cursor lookup
- **AND** if system lookup is unavailable it renders the built-in generated fallback cursor

#### Scenario: Cursor hidden while UI overlay is visible
- **GIVEN** the viewer UI overlay is visible
- **WHEN** pointer events are suspended for forwarded clients
- **THEN** the compositor software cursor is hidden
