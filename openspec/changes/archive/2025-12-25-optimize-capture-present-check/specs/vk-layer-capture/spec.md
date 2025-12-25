# vk-layer-capture Specification

## MODIFIED Requirements
### Requirement: Unix Socket IPC
The layer SHALL communicate with the Goggles application via Unix domain socket to transfer DMA-BUF file descriptors.

#### Scenario: Early Connection Check
- **WHEN** `on_present` is called
- **THEN** the layer SHALL check the socket connection status first
- **AND** if not connected and connection attempt fails, return immediately
- **AND** skip export image initialization and frame capture