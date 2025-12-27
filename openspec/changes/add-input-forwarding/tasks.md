# Tasks: Add Input Forwarding

## Overview

Ordered implementation tasks for the input forwarding feature. Each task is independently verifiable and delivers incremental progress toward the final capability.

---

## Phase 1: Foundation (IPC Protocol & Module Setup)

### Task 1: Extend IPC protocol with input_display_ready message
**File**: `src/capture/capture_protocol.hpp`

- Add `input_display_ready = 4` to `CaptureMessageType` enum
- Define `CaptureInputDisplayReady` struct with `int32_t display_number` field
- Add `static_assert` for struct size (20 bytes)

**Validation**: Compiles without errors, struct size assertion passes

---

### Task 2: Create input module directory structure
**Files**: `src/input/`, CMakeLists.txt

- Create `src/input/` directory
- Create `src/input/CMakeLists.txt` with library target `goggles_input`
- Find required packages: wlroots-0.19, wayland-server, xkbcommon, X11, Xtst
- Add include directories and link libraries
- Add `goggles_input` to `src/CMakeLists.txt`

**Validation**: CMake configure succeeds with all packages found

---

## Phase 2: XWayland Server Implementation

### Task 3: Implement XWaylandServer class skeleton
**Files**: `src/input/xwayland_server.hpp`, `src/input/xwayland_server.cpp`

- Define `XWaylandServer` class in `goggles::input` namespace
- Add member variables for wlroots objects (display, backend, compositor, seat, xwayland)
- Add `std::jthread m_compositor_thread`
- Implement constructor/destructor (initialization to nullptr)
- Add `start() -> Result<int>` and `stop()` method declarations

**Validation**: Compiles, links against wlroots

---

### Task 4: Implement headless compositor initialization
**File**: `src/input/xwayland_server.cpp`

- In `start()`, create `wl_display` with `wl_display_create()`
- Get event loop with `wl_display_get_event_loop()`
- Create headless backend with `wlr_headless_backend_create()`
- Create renderer and allocator with autocreate functions
- Create compositor with `wlr_compositor_create()`
- Create seat with `wlr_seat_create()` and set keyboard capability
- Add error handling for each step (return `Result<int>` error)

**Validation**: Instantiate `XWaylandServer`, call `start()`, verify no crashes, check return value

---

### Task 5: Implement automatic DISPLAY selection
**File**: `src/input/xwayland_server.cpp`

- In `start()`, loop from display_num = 1 to 9
- Try `wl_display_add_socket()` with `wayland-{N}` name
- On success, break loop and proceed to XWayland creation
- On failure, continue to next number
- Return error if all 9 attempts fail

**Validation**: Start compositor, verify socket created (check `/run/user/$UID/` for wayland-N)

---

### Task 6: Implement XWayland server startup
**File**: `src/input/xwayland_server.cpp`

- After socket creation, call `wlr_xwayland_create(display, compositor, false)`
- Store the display number for return value
- Call `wlr_backend_start()` to initialize backend
- Return `ok(display_num)` on success

**Validation**: Start compositor, run `ps aux | grep Xwayland`, verify Xwayland process on :N

---

### Task 7: Implement compositor event loop thread
**File**: `src/input/xwayland_server.cpp`

- In `start()`, after backend start, spawn thread: `m_compositor_thread = std::jthread([this] { wl_display_run(m_display); })`
- In `stop()`, call `wl_display_terminate(m_display)` to stop event loop
- Thread joins automatically via `std::jthread` destructor

**Validation**: Start compositor, verify thread spawned (check with `ps -T`), call stop(), verify thread joined

---

### Task 8: Implement resource cleanup in correct order
**File**: `src/input/xwayland_server.cpp`

- In `stop()`, destroy wlroots resources in reverse order:
  - `wlr_xwayland_destroy(m_xwayland)`
  - `wlr_seat_destroy(m_seat)`
  - `wlr_compositor_destroy(m_compositor)` (if applicable - may be automatic)
  - `wlr_allocator_destroy(m_allocator)`
  - `wlr_renderer_destroy(m_renderer)`
  - `wlr_backend_destroy(m_backend)`
  - `wl_display_destroy(m_display)`
- Set pointers to nullptr after destruction

**Validation**: Start/stop compositor multiple times in a loop (10 iterations), verify no crashes or memory leaks (valgrind)

---

## Phase 3: Input Forwarder Public API

### Task 9: Implement InputForwarder class with PIMPL
**Files**: `src/input/input_forwarder.hpp`, `src/input/input_forwarder.cpp`

- Define `InputForwarder` class in `goggles::input` namespace
- Forward-declare `struct Impl`
- Add `std::unique_ptr<Impl> m_impl` member
- Implement constructor (allocate Impl), destructor (automatic cleanup)
- Add method declarations: `init() -> Result<void>`, `forward_key(const SDL_KeyboardEvent&) -> Result<void>`, `display_number() const -> int`

**Validation**: Compiles, InputForwarder can be instantiated and destroyed

---

### Task 10: Implement InputForwarder::Impl structure
**File**: `src/input/input_forwarder.cpp`

- Define `struct InputForwarder::Impl` with:
  - `XWaylandServer server`
  - `Display* x11_display = nullptr`
  - Destructor: call `XCloseDisplay()` if not nullptr

**Validation**: Compiles, Impl destructor cleans up X11 connection

---

### Task 11: Implement InputForwarder::init()
**File**: `src/input/input_forwarder.cpp`

- Call `m_impl->server.start()`, store result as `display_num`
- Call `XOpenDisplay(fmt::format(":{}", display_num).c_str())`
- If XOpenDisplay fails, call `m_impl->server.stop()` and return error
- Store x11_display in `m_impl`
- Send `CaptureInputDisplayReady` message via existing socket (implement helper)
- Return `ok()` on success

**Validation**: Call init(), verify X11 connection established (XOpenDisplay returns non-NULL)

---

### Task 12: Implement keycode translation map
**File**: `src/input/input_forwarder.cpp`

- Create static function `sdl_to_linux_keycode(SDL_Scancode) -> uint32_t`
- Map common keys: W, A, S, D, Escape, Enter, Space, arrow keys, numbers, letters
- Return 0 for unmapped scancodes
- Create helper `linux_to_x11_keycode(uint32_t) -> uint32_t` (return linux_code + 8)

**Validation**: Unit test for translation (SDL_SCANCODE_W → 17 → 25)

---

### Task 13: Implement InputForwarder::forward_key()
**File**: `src/input/input_forwarder.cpp`

- Early return if `m_impl->x11_display` is nullptr
- Translate scancode: `SDL → Linux → X11`
- If keycode is 0 (unmapped), return `ok()` (no-op)
- Determine press state from `event.down` (True for press, False for release)
- Call `XTestFakeKeyEvent(m_impl->x11_display, x11_keycode, is_press, CurrentTime)`
- Call `XFlush(m_impl->x11_display)` to send immediately
- Return `ok()`

**Validation**: Manual test - press key, check XFlush is called (use strace or logging)

---

## Phase 4: Layer Integration

### Task 14: Add receive_with_timeout helper to layer
**File**: `src/capture/vk_layer/ipc_socket.cpp`

- Implement `receive_message_with_timeout(int timeout_ms) -> std::optional<CaptureMessageType>`
- Use `poll()` or `select()` with timeout on socket fd
- Read message header, return type if successful
- Return `std::nullopt` on timeout or error

**Validation**: Unit test with mock socket (send message, verify receive; wait for timeout, verify nullopt)

---

### Task 15: Modify layer constructor to receive DISPLAY
**File**: `src/capture/vk_layer/vk_capture.cpp`

- Add `__attribute__((constructor(101)))` to `layer_early_init()` function
- Call `receive_message_with_timeout(100)`
- If message type is `input_display_ready`, read `display_number`
- Format string `:N` and call `setenv("DISPLAY", display_str, 1)`
- If timeout or wrong type, do nothing (keep default DISPLAY)

**Validation**: Start Goggles, start test app with layer, verify DISPLAY env var is :N in app process

---

### Task 16: Implement send_display_to_layer in InputForwarder
**File**: `src/input/input_forwarder.cpp`

- In `init()`, after X11 connection established:
  - Connect to existing capture socket `\0goggles/vkcapture`
  - Construct `CaptureInputDisplayReady` message
  - Send message via socket
  - Close socket (layer will reconnect for frame capture)
- Add error handling if socket send fails

**Validation**: Start Goggles, check layer receives DISPLAY (add debug logging in layer)

---

## Phase 5: Main Application Integration

### Task 17: Integrate InputForwarder in main.cpp
**Files**: `src/app/main.cpp`, `src/app/CMakeLists.txt`

- Add `#include "input/input_forwarder.hpp"`
- In `run_app()`, after SDL init, instantiate `goggles::input::InputForwarder input_forwarder`
- Call `GOGGLES_TRY(input_forwarder.init())`
- Pass `input_forwarder` to `run_main_loop()` by reference
- In event loop, on `SDL_EVENT_KEY_DOWN` or `SDL_EVENT_KEY_UP`, call `input_forwarder.forward_key(event.key)`
- Add `goggles_input` to CMakeLists link libraries

**Validation**: Build succeeds, app runs without crashes

---

### Task 18: Add logging for input events
**File**: `src/input/input_forwarder.cpp`

- In `init()`, log success: `GOGGLES_LOG_INFO("Input forwarding initialized on DISPLAY :{}", display_num)`
- In `forward_key()`, log at debug level: `GOGGLES_LOG_DEBUG("Forward key: SDL {} → X11 {}", scancode, x11_keycode)`
- On init failure, log error: `GOGGLES_LOG_ERROR("Input forwarding init failed: {}", error.message)`

**Validation**: Run with log level debug, verify logs appear

---

### Task 19: Handle init failure gracefully
**File**: `src/app/main.cpp`

- Change `GOGGLES_TRY(input_forwarder.init())` to:
  ```cpp
  auto init_result = input_forwarder.init();
  if (!init_result) {
      GOGGLES_LOG_WARN("Input forwarding disabled: {}", init_result.error().message);
  }
  ```
- Modify `forward_key()` to check if initialized (early return if not)
- Application continues without input forwarding

**Validation**: Kill all Xwayland processes, run app, verify it starts and logs warning

---

## Phase 6: Testing & Documentation

### Task 20: Create test application
**File**: `test_app.cpp`

- SDL3 + Vulkan app that prints received keyboard events
- Force SDL to use X11 driver (`SDL_VIDEODRIVER=x11`)
- Print scancode and key name on key down/up
- Compile in CMakeLists as test target

**Validation**: `GOGGLES_CAPTURE=1 ./test_app` prints key events when keys are pressed in Goggles

---

### Task 21: Manual integration test
**Steps**:
1. Start Goggles
2. Start test_app with `GOGGLES_CAPTURE=1 GOGGLES_WSI_PROXY=1`
3. Press W, A, S, D keys in Goggles window
4. Verify test_app terminal shows: `[Input] KEY DOWN: name='W'`, etc.
5. Test Wine app (e.g., RE4) if available

**Validation**: Input events received by captured app, no crashes

---

### Task 22: Update docs/input_forwarding.md
**File**: `docs/input_forwarding.md`

- Add section: "Testing" with manual test instructions
- Add section: "Troubleshooting" with common issues (DISPLAY conflict, XWayland not found)
- Add section: "Limitations" (keyboard only, limited keymap)

**Validation**: Documentation is clear and accurate

---

### Task 23: Validate OpenSpec compliance
**Command**: `openspec validate add-input-forwarding --strict`

- Fix any validation errors reported
- Ensure all scenarios reference valid requirements
- Check all file references are correct

**Validation**: Validation passes with zero errors

---

### Task 24: Update architecture.md with input module
**File**: `docs/architecture.md`

- Add `src/input/` to module overview table
- Add description: "Keyboard input forwarding via nested XWayland"
- Update system context diagram to show input flow

**Validation**: Documentation accurately reflects new architecture

---

## Dependencies Between Tasks

- **Tasks 1-2**: Can run in parallel (protocol definition + module setup)
- **Tasks 3-8**: Sequential (XWayland server implementation)
- **Tasks 9-13**: Sequential (InputForwarder implementation), depends on Task 8
- **Tasks 14-16**: Sequential (Layer integration), depends on Task 1
- **Task 17**: Depends on Task 13 (InputForwarder API complete)
- **Tasks 18-19**: Sequential refinements of Task 17
- **Tasks 20-24**: Can run in parallel after Task 19

## Parallelization Opportunities

- **Phase 2 and Phase 4** can proceed in parallel after Phase 1 completes
- **Tasks 20, 22, 24** (testing and docs) can be done concurrently during final phase
