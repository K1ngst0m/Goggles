## ADDED Requirements

### Requirement: Present Wait Frame Pacing

The render backend SHALL use VK_KHR_present_wait when supported to pace presentation and avoid uncapped mailbox behavior on high-end GPUs.

#### Scenario: Present wait enabled
- **GIVEN** the physical device supports `VK_KHR_present_wait`
- **WHEN** the swapchain is created
- **THEN** the device SHALL enable the extension
- **AND** the present mode SHALL be `FIFO`
- **AND** the backend SHALL use present wait to pace to `render.target_fps`

#### Scenario: Uncapped target fps
- **GIVEN** `render.target_fps` is set to `0`
- **WHEN** present wait is available
- **THEN** the backend SHALL skip waiting for a target interval
- **AND** presentation SHALL proceed as fast as FIFO allows

#### Scenario: Present wait unsupported
- **GIVEN** `VK_KHR_present_wait` is not supported
- **WHEN** the swapchain is created
- **THEN** the backend SHALL prefer `MAILBOX` present mode
- **AND** it SHALL apply CPU-side frame capping when `render.target_fps` is non-zero
- **AND** it SHALL fall back to `FIFO` if `MAILBOX` is unavailable

#### Scenario: Target fps changes via config
- **GIVEN** a configuration file sets `render.target_fps` to a non-zero value
- **WHEN** the application starts
- **THEN** the backend SHALL pace presentation to that value

