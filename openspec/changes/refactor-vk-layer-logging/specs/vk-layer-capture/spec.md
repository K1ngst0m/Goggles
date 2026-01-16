## MODIFIED Requirements

### Requirement: Layer Logging Constraints
The layer SHALL follow project logging policies for capture layer code, with explicit opt-in runtime
logging controls suitable for an implicit Vulkan layer.

#### Scenario: Minimal hot-path logging
- **WHEN** `vkQueuePresentKHR` executes
- **THEN** no logging SHALL occur

#### Scenario: Default-off logging
- **GIVEN** `GOGGLES_DEBUG_LOG` is unset or `0`
- **WHEN** any `LAYER_*` logging macro is invoked
- **THEN** no output SHALL be emitted

#### Scenario: Enable logging with default level
- **GIVEN** `GOGGLES_DEBUG_LOG=1` is set
- **AND** `GOGGLES_DEBUG_LOG_LEVEL` is unset
- **WHEN** a log macro at `info` level or higher is invoked
- **THEN** the layer SHALL emit the log message
- **AND** prefix all logs with `[goggles_vklayer]`

#### Scenario: Level filtering
- **GIVEN** `GOGGLES_DEBUG_LOG=1` is set
- **AND** `GOGGLES_DEBUG_LOG_LEVEL=error` is set
- **WHEN** a `debug`, `info`, or `warn` log macro is invoked
- **THEN** no output SHALL be emitted
- **AND** `error` and `critical` logs MAY be emitted

#### Scenario: Launcher forwards options to target app
- **GIVEN** Goggles is launched in default mode with `--layer-log`
- **WHEN** the target app is spawned
- **THEN** Goggles SHALL set `GOGGLES_DEBUG_LOG=1` in the target app environment
- **AND** the capture layer MAY emit `info/warn/debug` logs according to
  `GOGGLES_DEBUG_LOG_LEVEL`

#### Scenario: Efficient backend
- **GIVEN** logging is enabled
- **WHEN** the layer emits a log message
- **THEN** the layer SHOULD use a `write(2)`-based backend
- **AND** SHOULD avoid `stdio` to reduce lock contention

#### Scenario: Anti-spam logging
- **GIVEN** logging is enabled
- **WHEN** an error condition occurs repeatedly in a loop
- **THEN** the layer SHOULD support anti-spam logging primitives (e.g., log once / every N)
