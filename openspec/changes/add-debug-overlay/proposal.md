# Add Debug Overlay

## Summary

Add a simple debug overlay to ImGuiLayer displaying FPS, frame time, and a frame time history graph for both render and source (capture) frames.

## Problem

No runtime visibility into performance metrics. Users cannot see FPS or frame timing information while using the application.

## Solution

Add a compact debug overlay window using ImGui:
- Render FPS and frame time
- Source (capture) FPS and frame time
- Frame time history graph (PlotLines)

## Implementation

1. Add frame time tracking to ImGuiLayer (ring buffer for render and source)
2. Add `notify_source_frame()` method called when new capture frame arrives
3. Add `draw_debug_overlay()` method
4. Wire through UiController and Application

## Non-goals

- GPU timing (requires Vulkan timestamp queries)
- Memory usage tracking