## MODIFIED Requirements

### Requirement: Unix Socket IPC
The layer SHALL communicate with the Goggles application via Unix domain socket to transfer DMA-BUF file descriptors.

#### Scenario: Early Connection Check
- **WHEN** `on_present` is called
- **THEN** the layer SHALL check the socket connection status first
- **AND** if not connected and connection attempt fails, return immediately
- **AND** skip polling control messages, resource initialization, and frame capture

#### Scenario: Synchronized Capture State
- **GIVEN** the layer is connected to a viewer
- **WHEN** `on_present` is called
- **THEN** the layer SHALL poll all pending control messages
- **AND** update its internal capturing state
- **AND** if capture is disabled, skip resource initialization and frame capture

### Requirement: Worker Thread Lifecycle
The layer SHALL manage worker thread lifecycle to ensure clean startup and shutdown.

#### Scenario: Graceful shutdown
- **WHEN** `CaptureManager::shutdown()` is called
- **THEN** the layer SHALL set a shutdown flag atomically
- **AND** notify the worker thread via condition variable
- **AND** join the worker thread before the method returns
- **AND** explicitly cleanup all Vulkan resources associated with active swapchains
