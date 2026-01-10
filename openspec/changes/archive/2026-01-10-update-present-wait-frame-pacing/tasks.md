# Tasks

## Implementation

1. [x] Add VK_KHR_present_wait capability detection and tracking in the render backend
2. [x] Update swapchain present mode selection to prefer FIFO + present wait, fallback to MAILBOX
3. [x] Implement present-wait pacing using `render.target_fps` (0 = uncapped)
4. [x] Add CLI option to override target FPS and pass into config
5. [x] Allow `target_fps = 0` in config parsing and document meaning

## Validation

6. [x] `pixi run build -p debug`
7. [x] `pixi run test -p test`
8. [x] `pixi run dev -p quality`
