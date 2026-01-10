# Tasks

## Implementation

1. [ ] Add VK_KHR_present_wait capability detection and tracking in the render backend
2. [ ] Update swapchain present mode selection to prefer FIFO + present wait, fallback to MAILBOX
3. [ ] Implement present-wait pacing using `render.target_fps` (0 = uncapped)
4. [ ] Add CLI option to override target FPS and pass into config
5. [ ] Allow `target_fps = 0` in config parsing and document meaning

## Validation

6. [ ] `pixi run build -p debug`
7. [ ] `pixi run test -p test`
