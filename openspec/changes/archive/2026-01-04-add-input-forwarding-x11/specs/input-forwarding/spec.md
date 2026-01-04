# input-forwarding Specification

## Purpose

Defines the input forwarding module that enables users to control captured Vulkan applications by pressing keys and using the mouse in the Goggles viewer window. The module creates a nested XWayland server and forwards input events via XTest injection.

## ADDED Requirements

### Requirement: XWayland Server Lifecycle

The input forwarding module SHALL create a headless Wayland compositor with nested XWayland server for captured applications to connect to.

#### Scenario: Automatic DISPLAY selection
- **WHEN** `InputForwarder::create()` is called
- **THEN** the system SHALL attempt to create Wayland sockets on wayland-1, wayland-2, etc. in sequence
- **AND** start XWayland on the first available DISPLAY (:1, :2, ...)
- **AND** expose the selected DISPLAY number via `InputForwarder::display_number()`

#### Scenario: XWayland startup
- **WHEN** a Wayland socket is successfully created on wayland-N
- **THEN** the system SHALL call `wlr_xwayland_create()` to spawn XWayland process
- **AND** XWayland SHALL listen on DISPLAY :N
- **AND** the Wayland event loop SHALL run in a dedicated `std::jthread`

#### Scenario: Compositor thread lifecycle
- **WHEN** `XWaylandServer::start()` succeeds
- **THEN** a compositor thread SHALL be spawned running `wl_display_run()`
- **AND** the thread SHALL automatically join in `XWaylandServer::~XWaylandServer()`
- **AND** the event loop SHALL terminate via `wl_display_terminate()` before thread join

#### Scenario: Resource cleanup order
- **WHEN** `XWaylandServer::stop()` or destructor is called
- **THEN** wlroots resources SHALL be destroyed in reverse creation order
- **AND** XWayland SHALL be destroyed before compositor
- **AND** compositor SHALL be destroyed before backend
- **AND** backend SHALL be destroyed before display

### Requirement: Keyboard Event Forwarding

The input forwarding module SHALL translate SDL keyboard events to X11 KeyPress/KeyRelease events and inject them into the nested XWayland via XTest.

#### Scenario: SDL to X11 keycode translation
- **WHEN** `InputForwarder::forward_key()` receives an SDL_KeyboardEvent
- **THEN** the SDL scancode SHALL be translated to Linux keycode (e.g. SDL_SCANCODE_W → KEY_W = 17)
- **AND** the Linux keycode SHALL be translated to X11 keycode (+8 offset, e.g. 17 → 25)
- **AND** unknown scancodes SHALL be skipped without error

#### Scenario: XTest key injection
- **WHEN** a valid X11 keycode is obtained
- **THEN** `XTestFakeKeyEvent(display, keycode, is_press, CurrentTime)` SHALL be called
- **AND** `XFlush(display)` SHALL be called to send the event immediately

#### Scenario: Press and release events
- **WHEN** `SDL_EVENT_KEY_DOWN` is received
- **THEN** `XTestFakeKeyEvent(..., True, ...)` SHALL be called (key press)
- **WHEN** `SDL_EVENT_KEY_UP` is received
- **THEN** `XTestFakeKeyEvent(..., False, ...)` SHALL be called (key release)

### Requirement: X11 Connection Management

The input forwarding module SHALL maintain an X11 client connection to the nested XWayland server.

#### Scenario: X11 connection establishment
- **WHEN** XWayland server has started on :N
- **THEN** `XOpenDisplay(":N")` SHALL be called to establish X11 client connection
- **AND** if connection fails, `InputForwarder::create()` SHALL return an error result
- **AND** XWaylandServer SHALL be stopped and cleaned up on connection failure

#### Scenario: X11 connection cleanup
- **WHEN** `InputForwarder::~InputForwarder()` is called
- **THEN** `XCloseDisplay()` SHALL be called if connection is open
- **AND** the connection SHALL be closed before XWaylandServer is stopped

### Requirement: Expose Selected DISPLAY Number

The input forwarding module SHALL expose the selected nested DISPLAY number so the target application can be launched inside the nested XWayland session.

#### Scenario: Query DISPLAY after init
- **WHEN** `InputForwarder::create()` succeeds
- **THEN** `InputForwarder::display_number()` SHALL return a positive integer N
- **AND** the application SHALL be able to report N to the user (e.g. via logs)

### Requirement: Mouse Event Forwarding (Basic)

The input forwarding module SHALL forward basic mouse input into the nested XWayland server via XTest.

#### Scenario: Mouse button injection
- **WHEN** `InputForwarder::forward_mouse_button()` receives an SDL_MouseButtonEvent
- **THEN** `XTestFakeButtonEvent(display, button, is_press, CurrentTime)` SHALL be called
- **AND** `XFlush(display)` SHALL be called to send the event immediately

#### Scenario: Mouse motion injection
- **WHEN** `InputForwarder::forward_mouse_motion()` receives an SDL_MouseMotionEvent
- **THEN** `XTestFakeMotionEvent(display, 0, x, y, CurrentTime)` SHALL be called using the SDL event coordinates
- **AND** `XFlush(display)` SHALL be called to send the event immediately

#### Scenario: Mouse wheel injection
- **WHEN** `InputForwarder::forward_mouse_wheel()` receives an SDL_MouseWheelEvent
- **THEN** wheel input SHALL be translated into X11 button events (4/5 for vertical, 6/7 for horizontal)
- **AND** a press+release pair SHALL be sent via `XTestFakeButtonEvent`

### Requirement: Error Handling and Graceful Degradation

The input forwarding module SHALL handle initialization failures gracefully and allow the application to continue without input forwarding.

#### Scenario: XWayland start failure
- **WHEN** all DISPLAY numbers 1-9 are already bound
- **THEN** `XWaylandServer::start()` SHALL return `Result<int>` error with `ErrorCode::input_init_failed`
- **AND** `InputForwarder::create()` SHALL propagate the error to the caller
- **AND** the application SHALL log the error and continue without input forwarding

#### Scenario: X11 connection failure
- **WHEN** `XOpenDisplay(":N")` returns NULL
- **THEN** `InputForwarder::create()` SHALL stop the XWaylandServer
- **AND** return an error result with `ErrorCode::input_init_failed`
- **AND** clean up all allocated resources via RAII

#### Scenario: Forward key when not initialized
- **WHEN** `InputForwarder::forward_key()` is called but `InputForwarder::create()` failed or was not called
- **THEN** the function SHALL return immediately without error (no-op)
- **AND** no logging SHALL occur (avoid log spam)

### Requirement: PIMPL Pattern for Implementation Hiding

The input forwarding public API SHALL use the PIMPL idiom to hide wlroots and X11 implementation details from public headers.

#### Scenario: Forward declaration in public header
- **WHEN** `input_forwarder.hpp` is included by application code
- **THEN** the header SHALL NOT include wlroots or X11 headers directly
- **AND** SHALL forward-declare `struct InputForwarder::Impl`
- **AND** store `std::unique_ptr<Impl> m_impl` as the only data member

#### Scenario: Implementation details in .cpp
- **WHEN** `input_forwarder.cpp` is compiled
- **THEN** `struct InputForwarder::Impl` SHALL be defined with full wlroots and X11 types
- **AND** SHALL own `XWaylandServer` instance
- **AND** SHALL own `Display* x11_display` for XTest injection

### Requirement: Integration with Main Application

The input forwarding module SHALL integrate with the existing main application event loop and lifecycle.

#### Scenario: Initialization in main
- **WHEN** `run_app()` initializes subsystems
- **THEN** the application SHALL only initialize input forwarding when explicitly enabled by user configuration or CLI
- **AND** `InputForwarder::create()` SHALL be called after SDL initialization
- **AND** failures SHALL be logged and the application SHALL continue without input forwarding

#### Scenario: Event forwarding in main loop
- **WHEN** the main event loop receives `SDL_EVENT_KEY_DOWN` or `SDL_EVENT_KEY_UP`
- **THEN** `InputForwarder::forward_key(event.key)` SHALL be called
- **AND** the return value SHALL be checked (propagate errors to log)

#### Scenario: Shutdown order
- **WHEN** the application shuts down
- **THEN** `InputForwarder` SHALL be destroyed after capture receiver stops
- **AND** the X11 connection SHALL be closed before XWaylandServer stops
- **AND** the compositor thread SHALL join before main thread exits

### Requirement: Wayland Input Forwarding Not Supported (Temporary)

The input forwarding module SHALL NOT claim to support input injection into Wayland-native clients.

#### Scenario: Wayland-native target
- **GIVEN** a target application uses a Wayland backend
- **WHEN** the user attempts to use input forwarding
- **THEN** input forwarding is not supported for that target (X11-only)

### Requirement: Keycode Translation Map

The input forwarding module SHALL maintain a translation map from SDL scancodes to Linux keycodes.

#### Scenario: Common key mappings
- **GIVEN** SDL_SCANCODE_W (26)
- **THEN** SHALL translate to KEY_W (17)
- **GIVEN** SDL_SCANCODE_A (4)
- **THEN** SHALL translate to KEY_A (30)
- **GIVEN** SDL_SCANCODE_ESCAPE (41)
- **THEN** SHALL translate to KEY_ESC (1)

#### Scenario: Unmapped scancode
- **WHEN** an SDL scancode has no Linux keycode mapping
- **THEN** `forward_key()` SHALL return immediately (no-op)
- **AND** no error SHALL be logged

#### Scenario: X11 keycode offset
- **WHEN** translating Linux keycode to X11 keycode
- **THEN** 8 SHALL be added to the Linux keycode
- **AND** the resulting X11 keycode SHALL be passed to XTest

### Requirement: Namespace and Module Placement

The input forwarding module SHALL follow project conventions for namespace and directory structure.

#### Scenario: Namespace hierarchy
- **GIVEN** the input forwarding module
- **THEN** classes SHALL be in `goggles::input` namespace
- **AND** SHALL NOT use `using namespace` in headers

#### Scenario: File location
- **GIVEN** the input forwarding module
- **THEN** source files SHALL be placed in `src/input/` directory
- **AND** public headers SHALL use `snake_case.hpp` naming
- **AND** implementation files SHALL use `snake_case.cpp` naming

#### Scenario: Header includes
- **WHEN** including input forwarding headers
- **THEN** `#pragma once` SHALL be used (no include guards)
- **AND** include order SHALL follow project policy (self, C++ std, third-party, project)
