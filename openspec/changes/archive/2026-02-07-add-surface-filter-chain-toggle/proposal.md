# Change: Add per-surface filter chain toggle in Surface List

## Why
Some surfaces (overlays, launchers, popups) need a quick way to bypass shader processing while keeping other surfaces fully processed. A per-surface toggle lets users disable the filter chain on a specific surface while a global Window Management toggle provides a fast, session-wide override.

## What Changes
- Add a checkbox per surface entry in `Application -> Window Management -> Surface List` to enable or bypass the filter chain for that surface, with an icon label and tooltip.
- Add a global checkbox in `Application -> Window Management` to enable or bypass the filter chain for all surfaces during the session.
- The Effect Stage "Enable Shader" toggle continues to gate the effect stage only; Window Management toggles control the filter chain routing together.
- When the filter chain is bypassed (global or per-surface), the surface is resized using a real maximize-style resize event so the client re-renders at the new size, matching common compositor behavior (no stretch-blit).
- Toggle applies to the entire xdg_toplevel and all popups/subsurfaces belonging to it.
- New surfaces default to filter chain enabled for Vulkan capture path surfaces, and disabled for non-Vulkan capture path surfaces; state is cleared when a surface is removed.

## Impact
- Affected specs: app-window, render-pipeline
- Affected code: `src/ui/imgui_layer.hpp`, `src/ui/imgui_layer.cpp`, `src/app/application.cpp`, `src/render/chain/*`, `src/render/backend/*`, `src/input/*`
