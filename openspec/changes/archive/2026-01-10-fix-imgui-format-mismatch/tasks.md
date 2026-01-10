# Tasks: Fix ImGui Pipeline Format Mismatch

## Implementation

- [x] 1. Add format rebuild APIs to VulkanBackend
  - `needs_format_rebuild()` - check if format change requires rebuild
  - `rebuild_for_format()` - recreate swapchain with new format
  - `wait_all_frames()` - wait for in-flight frames before rebuild

- [x] 2. Add `rebuild_for_format()` to ImGuiLayer and UiController
  - Shutdown and reinitialize ImGui backends with new format

- [x] 3. Implement deferred format rebuild in Application
  - Add `m_pending_format` member
  - Detect format mismatch, set flag, return early
  - Handle rebuild at next frame start
  - Unify resize and format rebuild in one block

## Verification

- [x] 4. Build and test
  - No validation errors on format change
  - ImGui renders correctly after format change
  - Input works after format change