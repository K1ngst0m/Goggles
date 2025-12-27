# vk-layer-capture Specification (Delta)

## MODIFIED Requirements

### Requirement: Unix Socket IPC

The layer SHALL communicate with the Goggles application via Unix domain socket to transfer DMA-BUF file descriptors **and input display configuration**.

#### Scenario: Early Connection Check
- **WHEN** `on_present` is called
- **THEN** the layer SHALL check the socket connection status first
- **AND** if not connected and connection attempt fails, return immediately
- **AND** skip export image initialization and frame capture

#### Scenario: Receive input DISPLAY number (NEW)
- **WHEN** the layer constructor runs with priority 101
- **THEN** the layer SHALL attempt to receive a `CaptureInputDisplayReady` message
- **AND** SHALL wait up to 100ms for the message
- **AND** if received, extract the DISPLAY number from the message
- **AND** call `setenv("DISPLAY", ":N", 1)` where N is the received display number
- **AND** if timeout expires, SHALL NOT modify DISPLAY environment variable

#### Scenario: Constructor priority ordering (NEW)
- **WHEN** the shared library is loaded into the target application
- **THEN** the layer constructor SHALL have `__attribute__((constructor(101)))`
- **AND** SHALL execute before default priority (100) constructors
- **AND** SHALL set DISPLAY before the application's global initializers run

## ADDED Requirements

### Requirement: Input Display Handshake Message

The IPC protocol SHALL support a new message type for communicating the nested XWayland DISPLAY number.

#### Scenario: Message structure
- **GIVEN** a new message type `input_display_ready`
- **THEN** the message SHALL have type `CaptureMessageType::input_display_ready` (value 4)
- **AND** SHALL contain `int32_t display_number` field
- **AND** SHALL be 20 bytes total (4 + 4 + 12 padding)

#### Scenario: Message send from Goggles
- **WHEN** `InputForwarder::init()` successfully starts XWayland on :N
- **THEN** Goggles SHALL construct `CaptureInputDisplayReady` with display_number = N
- **AND** send the message over the Unix socket before any captured apps start

#### Scenario: Message receive in layer
- **WHEN** the layer constructor calls `receive_with_timeout(100ms)`
- **THEN** if a message is received, check if type == `input_display_ready`
- **AND** if so, read `display_number` and set DISPLAY environment variable
- **AND** if timeout or wrong message type, proceed with default behavior (no DISPLAY modification)

#### Scenario: Protocol version compatibility
- **WHEN** an older layer version (without input support) connects
- **THEN** the layer SHALL ignore unknown message types (forward compatibility)
- **AND** Goggles SHALL NOT require acknowledgment of `input_display_ready`

#### Scenario: Message ordering
- **GIVEN** the IPC protocol message sequence
- **THEN** `input_display_ready` SHALL be sent before `client_hello` is expected
- **AND** the layer SHALL receive `input_display_ready` before connecting to the socket for frame capture
- **AND** the layer SHALL send `client_hello` after setting DISPLAY (if received)
