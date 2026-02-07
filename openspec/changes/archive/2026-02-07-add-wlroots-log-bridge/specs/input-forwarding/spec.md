## ADDED Requirements
### Requirement: Wlroots Logging Bridge
The compositor server SHALL route wlroots log output through the project logging utilities and respect configured verbosity.

#### Scenario: Default log level filters wlroots debug noise
- **GIVEN** the application log level is info (default)
- **WHEN** the compositor initializes wlroots logging
- **THEN** wlroots debug logs are suppressed
- **AND** wlroots info/error logs are emitted through the project logger

#### Scenario: Debug level exposes wlroots diagnostics
- **GIVEN** the application log level is debug or trace
- **WHEN** wlroots emits a debug log message
- **THEN** the message is emitted via the project logger at debug level

### Requirement: Stderr Suppression Does Not Hide Wlroots Logs
The compositor server SHALL NOT suppress wlroots logs when stderr suppression is active for external helper noise.

#### Scenario: External stderr suppression enabled
- **GIVEN** stderr suppression is enabled to reduce external helper noise
- **WHEN** wlroots emits an error log
- **THEN** the error is still visible via the project logger
