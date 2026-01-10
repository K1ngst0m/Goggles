## 1. Protocol Extension

- [ ] 1.1 Extend `CaptureControl` struct with `resolution_request` flag and `requested_width/height` fields
- [ ] 1.2 Maintain struct size (use reserved fields) for backward compatibility

## 2. Viewer Side (CaptureReceiver)

- [ ] 2.1 Add `request_resolution(uint32_t width, uint32_t height)` method
- [ ] 2.2 Send `CaptureControl` message with resolution request

## 3. Layer Side (vk_layer)

- [ ] 3.1 Parse resolution request fields in `IpcSocket::poll_control()`
- [ ] 3.2 Add `set_resolution()` method to `WsiVirtualSurface`
- [ ] 3.3 Update virtual surface capabilities to reflect new resolution
- [ ] 3.4 Trigger swapchain out-of-date to force application swapchain recreation

## 4. Application Integration

- [ ] 4.1 Call `request_resolution()` on window resize
- [ ] 4.2 Optional: Add aspect ratio lock mode
