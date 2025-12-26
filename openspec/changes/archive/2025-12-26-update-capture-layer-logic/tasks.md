## 1. Implementation
- [x] 1.1 Loop `recv` in `LayerSocketClient::poll_control` to process all pending messages.
- [x] 1.2 Move connection check to the beginning of `CaptureManager::on_present`.
- [x] 1.3 Add `socket.is_capturing()` check in `CaptureManager::on_present`.
- [x] 1.4 Implement explicit resource cleanup in `CaptureManager::shutdown()`.

## 2. Validation
- [x] 2.1 Build layer and application.
- [x] 2.2 Verify tests pass.
- [x] 2.3 (Manual) Verify capture toggling works correctly without resource leaks.
