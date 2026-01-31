## ADDED Requirements
### Requirement: Popup Surface Support

The system SHALL support Wayland `xdg_popup` surfaces associated with a mapped `xdg_toplevel`.

The compositor server SHALL:
- Listen for `new_popup` signals from `xdg_shell`
- Track popup surfaces separately from toplevel surfaces with parent linkage and stacking order
- Send initial configure for popups and respect ack_configure before focus changes
- Render popups above their parent surface in the presented frame
- Route pointer and keyboard events to the topmost mapped popup while a popup grab is active

#### Scenario: Wayland menu popup renders
- **GIVEN** a Wayland toplevel is mapped
- **WHEN** the client creates and maps an `xdg_popup` (menu/dropdown)
- **THEN** the popup is configured and rendered above the parent surface
- **AND** input is delivered to the popup until it is dismissed

#### Scenario: Popup dismissed with parent
- **GIVEN** a parent toplevel with a mapped popup
- **WHEN** the parent surface is destroyed
- **THEN** all associated popups are removed from tracking

### Requirement: XWayland Override-Redirect Popups

The system SHALL present XWayland override-redirect surfaces (menus/tooltips) as popups.

The compositor server SHALL:
- Track override-redirect XWayland surfaces as transient popups
- Accept map requests for override-redirect surfaces
- Render override-redirect surfaces above the focused XWayland surface
- Route pointer and keyboard events to the topmost mapped override-redirect surface while visible

#### Scenario: X11 menu popup renders
- **GIVEN** an XWayland surface is focused
- **WHEN** the app creates an override-redirect menu window
- **THEN** the menu is mapped and rendered above the parent surface
- **AND** pointer clicks are delivered to the menu window
