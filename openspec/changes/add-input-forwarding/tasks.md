## 1. Implementation
- [x] 1.1 Extend capture protocol with `config_request` (6) and `input_display_ready` (7).
- [x] 1.2 Add `goggles_input` module (`src/input/`) with wlroots headless compositor + XWayland.
- [x] 1.3 Implement `InputForwarder` (PIMPL) using X11 XTest for key + mouse injection.
- [x] 1.4 Add layer early init handshake to request DISPLAY and `setenv("DISPLAY", ":N", 1)` before app main.
- [x] 1.5 Integrate input forwarding into `src/app/main.cpp` SDL event loop (keys/buttons/motion/wheel).
- [x] 1.6 Add `goggles_input_test` manual test app.

## 2. Follow-Ups
- [ ] 2.1 Validate XWayland display number selection (ensure `display_number()` matches XWayland’s actual `:N`).
- [ ] 2.2 Add mouse coordinate mapping (viewer → captured app surface) and pointer constraints as needed.
- [ ] 2.3 Harden config handshake behavior (invalid/negative display, partial reads, startup ordering).
- [ ] 2.4 Consider a build/runtime feature flag to make wlroots/X11 deps optional when input forwarding is not needed.

## 3. Validation
- [ ] 3.1 Run `openspec validate add-input-forwarding --strict`.
- [ ] 3.2 Manual test: run `goggles_input_test` with the layer and confirm key/mouse events are received.
