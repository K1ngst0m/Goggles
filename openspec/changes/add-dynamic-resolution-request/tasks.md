## 1. Protocol Extension

- [x] 1.1 Extend `CaptureControl` struct with `resolution_request` flag and `requested_width/height` fields
- [x] 1.2 Maintain struct size (use reserved fields) for backward compatibility

## 2. Viewer Side (CaptureReceiver)

- [x] 2.1 Add `request_resolution(uint32_t width, uint32_t height)` method
- [x] 2.2 Send `CaptureControl` message with resolution request

## 3. Layer Side (vk_layer)

- [x] 3.1 Parse resolution request fields in `IpcSocket::poll_control()`
- [x] 3.2 Add `set_resolution()` method to `WsiVirtualSurface`
- [x] 3.3 Update virtual surface capabilities to reflect new resolution
- [x] 3.4 Trigger swapchain out-of-date to force application swapchain recreation

## 4. Application Integration

- [x] 4.1 Call `request_resolution()` on window resize
- [ ] 4.2 Optional: Add aspect ratio lock mode
