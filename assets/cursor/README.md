# Breeze Light Cursor Theme

This directory contains a subset of the Breeze Light cursor theme, built at 64px for use in wlroots-based compositors.

## Source

- **Upstream**: [KDE Breeze](https://invent.kde.org/plasma/breeze)
- **Version**: 6.5.5
- **Theme**: Breeze Light
- **Source Package**: kde-plasma/breeze

## Authors & Credits

- **Cursor Designer**: Ken Vermette <vermette@gmail.com>
- **Visual Designer**: Andrew Lake <jamboarder@gmail.com>
- **Project**: KDE Visual Design Group (VDG)

## License

This cursor theme is licensed under the **GNU General Public License v2.0** (GPL-2).

See the [LICENSE](LICENSE) file for the full license text.

## Build Information

- **Size**: 64px (single size build)
- **Built from**: SVG sources at `/usr/share/icons/Breeze_Light/cursors_scalable/`
- **Total cursors**: 33 cursor files + symlinks
- **Animated cursors**: `wait` and `progress` (23 frames each)

## Included Cursors

Essential cursor set for compositor use:

- `default`, `arrow`, `left_ptr` - Standard pointer
- `text`, `xterm`, `ibeam` - Text selection
- `pointer`, `hand2`, `pointing_hand` - Clickable elements
- `grab`, `openhand` - Grabbable objects
- `grabbing`, `closedhand` - Active drag
- `move`, `fleur`, `size_all` - Move/pan
- `wait`, `watch` - Busy (animated)
- `progress`, `left_ptr_watch`, `half-busy` - Background activity (animated)
- `crosshair`, `tcross`, `cross` - Precision selection
- `not-allowed`, `forbidden`, `no-drop`, `dnd-no-drop` - Invalid action
- Resize cursors: `n-resize`, `s-resize`, `e-resize`, `w-resize`, `ne-resize`, `nw-resize`, `se-resize`, `sw-resize`, `ns-resize`, `ew-resize`, `nwse-resize`, `nesw-resize`
- `all-scroll` - Omni-directional scroll
- `context-menu` - Context menu
- `copy` - Copy operation
- `alias` - Link/alias creation
- `cell` - Cell selection
- `col-resize`, `row-resize` - Table resizing
- `vertical-text` - Vertical text
- `zoom-in`, `zoom-out` - Zoom operations
- `dnd-move` - Drag and drop move

## Usage in wlroots Compositor

To use these cursors in your wlroots-based compositor, point the Xcursor search
path at the parent `assets` directory and load the `cursor` theme:

```c
// Example using wlroots xcursor loader
setenv("XCURSOR_PATH", "/path/to/assets", 1);
setenv("XCURSOR_SIZE", "64", 1);
```

Then request the `cursor` theme (folder name) when loading.
