# Proposal: Fix ImGui Pipeline Format Mismatch

## Summary
Fix Vulkan validation errors when swapchain format changes by deferring rebuild to next frame start.

## Problem
1. ImGui pipeline initialized before capture format is known
2. Format change mid-frame causes validation errors
3. Resize and format rebuild share similar swapchain recreation logic

## Solution: Deferred Format Rebuild

### Flow
```
Frame N:
  poll_frame()
  if (needs_format_rebuild) {
      m_pending_format = frame.format
      return  // skip this frame
  }
  render_frame()

Frame N+1:
  if (m_pending_format != 0 || m_window_resized) {
      wait_all_frames()
      if (m_pending_format) {
          rebuild_for_format()
          ui_controller->rebuild_for_format()
      } else {
          handle_resize()
      }
  }
  poll_frame()
  render_frame()
```

### Key Points
- Format detection sets flag, returns early (no inline rebuild)
- Rebuild happens at frame start, before any rendering
- Resize and format rebuild unified in one block
- Format rebuild also clears resize flag (both recreate swapchain)

## API

### Application (private)
```cpp
uint32_t m_pending_format = 0;  // 0 = no pending rebuild
```

### VulkanBackend
```cpp
bool needs_format_rebuild(vk::Format source_format) const;
Result<void> rebuild_for_format(vk::Format source_format);
void wait_all_frames();
```

### UiController
```cpp
void rebuild_for_format(vk::Format swapchain_format);
```
