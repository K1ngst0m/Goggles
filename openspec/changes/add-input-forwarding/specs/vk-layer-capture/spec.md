# vk-layer-capture Specification (Delta)

## ADDED Requirements

### Requirement: Config Handshake Protocol

The layer SHALL perform a separate config handshake connection in its constructor to receive input forwarding configuration before the application initializes.

#### Scenario: Config connection in constructor
- **WHEN** the layer constructor runs with priority 101
- **THEN** the layer SHALL create a temporary socket connection to `\0goggles/vkcapture`
- **AND** send `CaptureConfigRequest` message
- **AND** wait up to 100ms for `CaptureInputDisplayReady` response
- **AND** close the config connection after receiving response or timeout
- **AND** the config connection SHALL be independent of the frame capture connection

#### Scenario: Config request message
- **WHEN** the config connection is established
- **THEN** the layer SHALL send `CaptureConfigRequest` with type = 6
- **AND** version = 1
- **AND** message size SHALL be 16 bytes

#### Scenario: Receive input DISPLAY response
- **WHEN** `CaptureInputDisplayReady` is received (type = 7)
- **THEN** extract `display_number` field
- **AND** call `setenv("DISPLAY", ":N", 1)` where N is the display number
- **AND** if timeout expires (100ms), SHALL NOT modify DISPLAY environment variable

#### Scenario: Constructor priority ordering
- **WHEN** the shared library is loaded into the target application
- **THEN** the layer constructor SHALL have `__attribute__((constructor(101)))`
- **AND** SHALL execute before default priority (100) constructors
- **AND** SHALL set DISPLAY before the application's global initializers run

#### Scenario: Frame capture connection independence
- **WHEN** `on_present` connects for frame capture
- **THEN** the frame capture connection SHALL be a NEW socket connection
- **AND** SHALL NOT reuse the config connection
- **AND** SHALL send `client_hello` message as before
- **AND** existing frame capture protocol SHALL remain unchanged

### Requirement: Protocol Message Types

The IPC protocol SHALL add two new message types for input forwarding configuration.

#### Scenario: Config request message type
- **GIVEN** `CaptureMessageType::config_request = 6`
- **THEN** the struct SHALL contain type field and version field
- **AND** SHALL be 16 bytes total (type:4 + version:4 + reserved:8)

#### Scenario: Input display ready message type
- **GIVEN** `CaptureMessageType::input_display_ready = 7`
- **THEN** the struct SHALL contain type field and display_number field
- **AND** SHALL be 16 bytes total (type:4 + display_number:4 + reserved:8)

#### Scenario: Message type numbering
- **GIVEN** existing message types (client_hello=1, texture_data=2, control=3)
- **THEN** new message types SHALL use values 6 and 7
- **AND** SHALL NOT conflict with existing or reserved protocol messages (e.g. values 4-5 may be used by other capture features)

### Requirement: CaptureReceiver Config Handling

The CaptureReceiver SHALL handle config requests separately from frame capture connections.

#### Scenario: Detect config request
- **WHEN** CaptureReceiver receives a new connection
- **THEN** read the message type
- **AND** if type == `config_request`, respond with `input_display_ready` and close connection
- **AND** if type == `client_hello`, proceed with normal frame capture flow

#### Scenario: Config response
- **WHEN** a config request is received
- **THEN** construct `CaptureInputDisplayReady` with current DISPLAY number from InputForwarder
- **AND** send response message
- **AND** close the client connection immediately after sending
- **AND** wait for next connection (frame capture)
