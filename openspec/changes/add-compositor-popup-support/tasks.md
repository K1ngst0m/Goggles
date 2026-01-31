## 1. Implementation
- [x] 1.1 Add xdg_popup hooks and listeners (new_popup, map, commit, destroy, ack_configure).
- [x] 1.2 Track popup stacking per parent surface and maintain topmost ordering.
- [x] 1.3 Composite popup surfaces above the focused surface in the presented frame.
- [x] 1.4 Resolve pointer targets via hit-testing and popup-local coordinates.
- [x] 1.5 Extend cursor bounds to cover mapped popups (Wayland + XWayland).
- [x] 1.6 Track XWayland override-redirect windows as popup surfaces and present them.
- [x] 1.7 Add diagnostics/logging for popup lifecycle at debug level.

## 2. Verification
- [x] 2.1 Manual test: Wayland menu popup (native app) renders and receives input.
- [x] 2.2 Manual test: XWayland menu popup (X11 app) renders and receives input.
