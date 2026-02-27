# headless-mode Specification

## Purpose
TBD - created by archiving change add-headless-mode. Update Purpose after archive.
## Requirements
### Requirement: Headless CLI Flags
The application SHALL accept `--headless`, `--frames <N>`, and `--output <path>` as CLI flags. When `--headless` is present, `--frames` and `--output` MUST both be provided; missing either SHALL produce a descriptive error and exit with a non-zero code.

#### Scenario: All three flags provided
- **WHEN** the application is run with `--headless --frames 10 --output /tmp/frame.png -- ./app`
- **THEN** `CliOptions.headless` SHALL be `true`, `frames` SHALL be `10`, and `output_path` SHALL be `/tmp/frame.png`

#### Scenario: Missing --output with --headless
- **WHEN** the application is run with `--headless --frames 10 -- ./app` without `--output`
- **THEN** the application SHALL print a descriptive error message
- **AND** SHALL exit with a non-zero code

#### Scenario: Missing --frames with --headless
- **WHEN** the application is run with `--headless --output /tmp/frame.png -- ./app` without `--frames`
- **THEN** the application SHALL print a descriptive error message
- **AND** SHALL exit with a non-zero code

### Requirement: Headless Initialization Skips SDL and ImGui
When `--headless` is set, the application SHALL NOT initialize SDL, create a window, or initialize the ImGui layer. The `CompositorServer` SHALL be initialized and operational.

#### Scenario: No SDL window in headless mode
- **GIVEN** the application is launched with `--headless`
- **WHEN** initialization completes
- **THEN** no SDL window SHALL exist
- **AND** no ImGui context SHALL be created
- **AND** `CompositorServer` SHALL report a valid Wayland display name

#### Scenario: Child app receives Wayland display
- **GIVEN** the application is launched with `--headless -- ./test_app`
- **WHEN** the child process is spawned
- **THEN** `WAYLAND_DISPLAY` SHALL be set in the child's environment to the compositor's socket

### Requirement: Surfaceless VulkanBackend for Headless Mode
When initialized for headless mode, `VulkanBackend` SHALL NOT create a `vk::SurfaceKHR`, swapchain, or present-related semaphores. It SHALL allocate an offscreen `vk::Image` with format `eR8G8B8A8Unorm` and usage flags `eColorAttachment | eTransferSrc` as the sole render target.

#### Scenario: Headless factory creates no surface
- **GIVEN** `VulkanBackend::create_headless()` is called
- **WHEN** initialization completes
- **THEN** no `vk::SurfaceKHR` SHALL exist
- **AND** no swapchain SHALL exist
- **AND** the offscreen image SHALL be allocated with format `eR8G8B8A8Unorm`

#### Scenario: Physical device selection without present queue
- **GIVEN** headless mode is active
- **WHEN** a physical device is selected
- **THEN** the selection SHALL require DMA-BUF and external memory extensions
- **AND** SHALL NOT require surface present support

### Requirement: Headless Render Loop
In headless mode, the application SHALL run a loop that consumes compositor frames via `get_presented_frame()`, imports each DMA-BUF into `VulkanBackend`, records and submits render commands into the offscreen image, and waits on a fence before the next frame. The loop SHALL exit when the configured number of frames have been rendered.

#### Scenario: N frames rendered then exit
- **GIVEN** the application is launched with `--headless --frames 5 --output /tmp/out.png -- ./app`
- **WHEN** 5 compositor frames have been delivered and rendered
- **THEN** the application SHALL call `readback_to_png` and write the PNG
- **AND** SHALL exit with code 0

#### Scenario: No vkQueuePresentKHR called
- **GIVEN** headless mode is active
- **WHEN** a frame is rendered
- **THEN** `vkQueuePresentKHR` SHALL NOT be called
- **AND** the render fence SHALL be waited on synchronously before the next frame

### Requirement: PNG Readback and Export
After rendering the final frame, `VulkanBackend` SHALL read back the offscreen image to CPU memory using a staging buffer and write a PNG file to the configured output path using `stb_image_write_png`. The operation SHALL return `tl::expected<void, Error>`; write failure SHALL propagate as an error.

#### Scenario: Successful PNG write
- **GIVEN** headless rendering of N frames is complete
- **WHEN** `readback_to_png(output_path)` is called
- **THEN** a valid PNG file SHALL exist at `output_path`
- **AND** the image dimensions SHALL match the configured compositor output resolution

#### Scenario: PNG write failure propagated
- **GIVEN** `output_path` is in a non-writable directory
- **WHEN** `readback_to_png(output_path)` is called
- **THEN** the function SHALL return an `Error` describing the failure
- **AND** the application SHALL exit with a non-zero code

#### Scenario: Image layout transition before readback
- **WHEN** `readback_to_png` is called after rendering
- **THEN** the offscreen image SHALL be transitioned from `eColorAttachmentOptimal` to `eTransferSrcOptimal` before `vkCmdCopyImageToBuffer`
- **AND** the staging buffer memory SHALL be invalidated before CPU read if the memory type is not host-coherent

### Requirement: Signal-Based Shutdown in Headless Mode
In headless mode, the application SHALL handle `SIGTERM` and `SIGINT` via `signalfd`. On signal receipt the run loop SHALL exit cleanly, child processes SHALL be terminated with the same SIGTERM→SIGKILL escalation used in windowed mode, and all Vulkan resources SHALL be released before process exit.

#### Scenario: SIGTERM triggers clean shutdown
- **GIVEN** the application is running in headless mode
- **WHEN** `SIGTERM` is delivered to the goggles process
- **THEN** the run loop SHALL exit at the next tick
- **AND** the child process SHALL receive SIGTERM
- **AND** the application SHALL exit with a non-zero code indicating signal termination

#### Scenario: Child exit triggers headless shutdown
- **GIVEN** the application is running in headless mode with a child app
- **WHEN** the child process exits before N frames are rendered
- **THEN** the run loop SHALL exit
- **AND** the application SHALL exit with a non-zero code

