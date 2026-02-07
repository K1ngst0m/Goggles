## 1. Implementation
- [x] 1.1 Remove absolute pointer coordinates from compositor input events; keep relative-only motion.
- [x] 1.2 Load Xcursor assets from `assets/cursor` and build a reusable cursor texture + hotspot.
- [x] 1.3 Update input forwarding to drive the compositor cursor with raw relative deltas only.
- [x] 1.4 Honor pointer constraints: freeze cursor on lock, apply cursor hint, react to set_region,
      and confine via `wlr_region_confine` when active.
- [x] 1.5 Render the cursor texture in `render_surface_to_frame` using the compositor cursor
      position and hotspot.
- [x] 1.6 Update viewer cursor visibility/relative mode/grab based on UI visibility (no forwarding
      when UI is shown).
- [x] 1.7 Restructure cursor assets into a standard Xcursor theme layout.
