# layer-shell-overlay Specification

## Purpose
This spec defines normative requirements for layer shell overlay behavior in Goggles.

## Requirements

### Requirement: Layer Shell Global

The compositor server SHALL create and advertise a `wlr_layer_shell_v1` global (version 4) on its
Wayland display during startup.

The compositor server SHALL:
- Call `wlr_layer_shell_v1_create(display, 4)` in `setup_layer_shell()`
- Return a `Result<void>` error if creation fails, propagated via `GOGGLES_TRY` in `start()`
- Register a `new_surface` signal listener to receive incoming layer surface connections
- Detach the `new_surface` listener and null the pointer during `stop()`

#### Scenario: Layer shell global created on startup
- **WHEN** `CompositorServer::start()` succeeds
- **THEN** a `zwlr_layer_shell_v1` global is available on the Wayland display
- **AND** Wayland clients can bind to it

#### Scenario: Layer shell creation failure propagates
- **WHEN** `wlr_layer_shell_v1_create()` returns null
- **THEN** `start()` returns an error
- **AND** the compositor does not reach the running state

---

### Requirement: Layer Surface Lifecycle Tracking

The compositor server SHALL track each incoming `wlr_layer_surface_v1` with a `LayerSurfaceHooks`
struct allocated on the heap, following the existing `XdgToplevelHooks` pattern.

`LayerSurfaceHooks` SHALL contain:
- `impl`: pointer back to `Impl`
- `layer_surface`: the `wlr_layer_surface_v1*`
- `surface`: the associated `wlr_surface*`
- `id`: unique surface identifier assigned on creation
- `layer`: the `zwlr_layer_shell_v1_layer` enum value from `pending.layer` at creation time
- `configured`: bool, set to true after the first `wlr_layer_surface_v1_configure()` call
- `mapped`: bool, toggled by map/unmap signal handlers
- Listeners: `surface_commit`, `surface_map`, `surface_unmap`, `surface_destroy`, `layer_destroy`,
  `new_popup`

The `layer_hooks` vector SHALL be guarded by `hooks_mutex` (same mutex as `xdg_hooks`).

#### Scenario: New layer surface registered
- **WHEN** a Wayland client creates a layer surface
- **THEN** a `LayerSurfaceHooks` is allocated and pushed to `layer_hooks`
- **AND** all six signal listeners are registered

#### Scenario: Layer surface destroyed
- **WHEN** the `layer_destroy` signal fires
- **THEN** all listeners on the hooks struct are detached
- **AND** the hooks struct is removed from `layer_hooks` and deleted

---

### Requirement: Layer Surface Initial Configuration

The compositor server SHALL send a configure event to each layer surface on its first commit,
using the headless output dimensions and the surface's requested anchor, size, and margin state.

The compositor server SHALL:
- Check `layer_surface->initial_commit` in the `surface_commit` handler
- Compute `width` and `height` using the anchor/margin/size rules (fully-anchored → output size;
  partially-anchored → output dimension on anchored axis, desired size on free axis)
- Call `wlr_layer_surface_v1_configure(layer_surface, width, height)` exactly once
- Set `hooks->configured = true` after the call
- Skip the configure call on subsequent commits

#### Scenario: Fully-anchored layer surface configured at output size
- **GIVEN** a layer surface with all four anchor edges set
- **WHEN** the surface commits for the first time (`initial_commit == true`)
- **THEN** `wlr_layer_surface_v1_configure()` is called with the headless output width and height

#### Scenario: Partially-anchored layer surface configured with requested size
- **GIVEN** a layer surface with only top+left+right anchors and `desired_height = 40`
- **WHEN** the surface commits for the first time
- **THEN** `wlr_layer_surface_v1_configure()` is called with output width and height 40

#### Scenario: Subsequent commits do not re-configure
- **GIVEN** a layer surface that has already been configured
- **WHEN** it commits again
- **THEN** `wlr_layer_surface_v1_configure()` is NOT called again

---

### Requirement: Layer Surface Render Integration

The compositor server SHALL render mapped layer surfaces in the `render_surface_to_frame()` render
pass, ordered by protocol layer before and after the primary capture surface tree.

Render order within the pass SHALL be:
1. `ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND`
2. `ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM`
3. Primary capture surface tree (existing)
4. XWayland override-redirect popups (existing)
5. `ZWLR_LAYER_SHELL_V1_LAYER_TOP`
6. `ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY`
7. Software cursor (existing)

`render_layer_surfaces(pass, layer)` SHALL:
- Iterate `layer_hooks` under `hooks_mutex`
- Skip hooks where `mapped == false` or `layer_surface == nullptr`
- Use `wlr_layer_surface_v1_for_each_surface()` with `render_surface_iterator` to render the
  surface tree
- Compute position from `current.anchor` and `current.margin` relative to the headless output

#### Scenario: Overlay layer surface renders above game
- **GIVEN** a game surface is the primary capture target
- **AND** a layer surface with layer `overlay` is mapped
- **WHEN** `render_surface_to_frame()` runs
- **THEN** the layer surface is rendered after the game surface and before the cursor

#### Scenario: Background layer surface renders below game
- **GIVEN** a layer surface with layer `background` is mapped
- **WHEN** `render_surface_to_frame()` runs
- **THEN** the background layer surface is rendered before the game surface

#### Scenario: Unmapped layer surface not rendered
- **GIVEN** a layer surface exists but `mapped == false`
- **WHEN** `render_surface_to_frame()` runs
- **THEN** the layer surface is not included in the render pass

---

### Requirement: Layer Surface Popup Children

The compositor server SHALL handle `xdg_popup` surfaces spawned by a mapped layer surface by
routing them through the existing `handle_new_xdg_popup()` infrastructure.

The compositor server SHALL:
- Connect `layer_surface->events.new_popup` in `handle_new_layer_surface()`
- Forward the `wlr_xdg_popup*` to `handle_new_xdg_popup()` in the signal handler

#### Scenario: Layer surface spawns a popup
- **GIVEN** a mapped layer surface (e.g., a Steam overlay panel)
- **WHEN** the client creates an `xdg_popup` child surface
- **THEN** the popup is tracked via `XdgPopupHooks`
- **AND** the popup is rendered via the existing popup render path

---

### Requirement: Layer Surface Keyboard Interactivity

The compositor server SHALL forward keyboard focus to a mapped layer surface that requests
`exclusive` keyboard interactivity, and restore focus to the primary capture surface on unmap or
destroy.

The compositor server SHALL:
- In the `surface_map` handler: if `layer_surface->current.keyboard_interactive ==
  ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE`, call
  `wlr_seat_keyboard_enter(seat, surface, ...)` for the layer surface
- In the `surface_unmap` and `layer_destroy` handlers: restore keyboard focus to
  `focused_surface` (the primary capture target) if it is non-null, via `wlr_seat_keyboard_enter()`
- NOT change `focused_surface` or `focused_xsurface` in any layer surface handler

#### Scenario: Exclusive layer surface takes keyboard focus
- **GIVEN** a game surface has keyboard focus
- **WHEN** a layer surface with `exclusive` keyboard interactivity maps
- **THEN** the seat's keyboard focus moves to the layer surface
- **AND** `focused_surface` is unchanged

#### Scenario: Exclusive layer surface unmaps, focus restored
- **GIVEN** a layer surface with `exclusive` interactivity has keyboard focus
- **WHEN** the layer surface unmaps
- **THEN** keyboard focus is restored to `focused_surface`
- **AND** `focused_surface` is unchanged

#### Scenario: None-interactivity layer surface does not take keyboard focus
- **GIVEN** a game surface has keyboard focus
- **WHEN** a layer surface with `none` keyboard interactivity maps
- **THEN** keyboard focus remains with the game surface
