# Add Debug Overlay

## Summary

Add a simple debug overlay to ImGuiLayer displaying FPS, frame time, and a frame time history graph.

## Problem

No runtime visibility into performance metrics. Users cannot see FPS or frame timing information while using the application.

## Solution

Add a compact debug overlay window using ImGui:
- FPS counter
- Frame time (ms)
- Frame time history graph (PlotLines)

## Implementation

1. Add frame time tracking to ImGuiLayer (ring buffer for history)
2. Add `draw_debug_overlay()` method
3. Toggle with existing visibility or separate key

## Non-goals

- GPU timing (requires Vulkan timestamp queries)
- Capture latency metrics
- Memory usage tracking