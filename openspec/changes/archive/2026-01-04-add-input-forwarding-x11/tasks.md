## 1. Implementation
- [x] 1.1 Add `goggles_input` module (`src/input/`) with wlroots headless compositor + XWayland.
- [x] 1.2 Implement `InputForwarder` (PIMPL) using X11 XTest for key + mouse injection.
- [x] 1.3 Integrate input forwarding into `src/app/main.cpp` SDL event loop (keys/buttons/motion/wheel).
- [x] 1.4 Add `goggles_input_test` manual test app.
- [x] 1.5 Make input forwarding opt-in via config and CLI (default disabled).
- [x] 1.6 Update docs: target must be launched with `DISPLAY=:N` (nested XWayland); Wayland input forwarding not supported yet.
