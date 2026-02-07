## ADDED Requirements

### Requirement: Cross-Process Semaphore Export

The layer SHALL create and export timeline semaphores for cross-process GPU synchronization with the Goggles application.

#### Scenario: Exportable semaphore creation

- **WHEN** capture is initialized for a swapchain
- **THEN** the layer SHALL create two timeline semaphores with `VK_SEMAPHORE_TYPE_TIMELINE`
- **AND** use `VkExportSemaphoreCreateInfo` with `VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT`
- **AND** name them `frame_ready` (Layer signals) and `frame_consumed` (Goggles signals)

#### Scenario: Semaphore FD export

- **WHEN** exportable semaphores are created
- **THEN** the layer SHALL export file descriptors via `vkGetSemaphoreFdKHR`
- **AND** store the FDs for transfer to Goggles

#### Scenario: Required extensions

- **WHEN** `vkCreateDevice` is hooked
- **THEN** the layer SHALL add `VK_KHR_external_semaphore` to enabled extensions
- **AND** add `VK_KHR_external_semaphore_fd` to enabled extensions

### Requirement: Semaphore IPC Transfer

The layer SHALL transfer semaphore file descriptors to the Goggles application via Unix socket.

#### Scenario: Semaphore init message

- **WHEN** the first frame is captured after connection
- **THEN** the layer SHALL send a `semaphore_init` message via IPC
- **AND** attach two FDs via `SCM_RIGHTS`: `[frame_ready_fd, frame_consumed_fd]`
- **AND** include the initial timeline value (0)

#### Scenario: One-time transfer

- **GIVEN** semaphore FDs have been sent for this connection
- **WHEN** subsequent frames are captured
- **THEN** the layer SHALL NOT resend semaphore FDs
- **AND** SHALL send only frame metadata with timeline values

### Requirement: Bidirectional GPU Synchronization

The layer SHALL use the exported semaphores for bidirectional synchronization with Goggles.

#### Scenario: Back-pressure wait

- **GIVEN** frame N is being captured
- **AND** N > 1
- **WHEN** `vkQueuePresentKHR` is called
- **THEN** the layer SHALL wait on `frame_consumed` semaphore for value N-1
- **AND** use a timeout of 100ms to detect disconnection
- **AND** skip the frame if timeout occurs

#### Scenario: Frame ready signal

- **WHEN** the copy command is submitted
- **THEN** the layer SHALL signal `frame_ready` semaphore with value N
- **AND** increment the frame counter

#### Scenario: Frame metadata transfer

- **WHEN** the copy command is submitted
- **THEN** the layer SHALL send `frame_metadata` message via IPC
- **AND** include width, height, format, stride, offset, modifier
- **AND** include the frame number (timeline value N)

### Requirement: Semaphore Reconnection Handling

The layer SHALL handle client reconnection by resetting semaphore state.

#### Scenario: Disconnect detection

- **WHEN** `vkWaitSemaphoresKHR` times out
- **OR** IPC send fails
- **THEN** the layer SHALL mark semaphores as not sent
- **AND** reset frame counter to 0

#### Scenario: Re-export on reconnection

- **WHEN** a new client connects after disconnect
- **THEN** the layer SHALL call `vkGetSemaphoreFdKHR` again for new FDs
- **AND** send `semaphore_init` message to the new client
- **AND** resume sync from frame 1

## MODIFIED Requirements

### Requirement: Instance and Device Hooking

The layer SHALL intercept `vkCreateDevice` to establish dispatch chains and add required extensions.

#### Scenario: Device creation with extensions

- **WHEN** the target application calls `vkCreateDevice`
- **THEN** the layer SHALL add required device extensions:
  - `VK_KHR_EXTERNAL_MEMORY_FD`
  - `VK_EXT_EXTERNAL_MEMORY_DMA_BUF`
  - `VK_KHR_external_semaphore`
  - `VK_KHR_external_semaphore_fd`
- **AND** enumerate and store all queues
- **AND** identify the graphics queue for capture operations
