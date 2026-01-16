# Design: Vulkan implicit-layer logging

## Goals
- Default-off logging for an implicit Vulkan layer (no host app noise unless enabled).
- Minimal overhead when disabled: a single cached boolean check at call sites.
- Low overhead when enabled: format once, write once (`write(2)`).
- No dynamic allocation and no `stdio` locks.
- Anti-spam primitives to prevent log storms.
- No interface churn: keep `LAYER_DEBUG/WARN/ERROR/CRITICAL` call sites unchanged.

## Alternatives Considered
1) Keep `fprintf`/`stderr` macros: simplest but adds `stdio` lock contention and offers no runtime
   gating/level control.
2) Integrate `spdlog` in the layer: aligns with app logging but complicates standalone/layer-only
   builds and risks allocations and heavier initialization in host processes.
3) `write(2)` backend + env gating + anti-spam (chosen): keeps overhead low and behavior explicit,
   avoids `stdio`, and is safer for an implicit layer.

## Runtime Controls
- `GOGGLES_DEBUG_LOG`
  - Unset/`0`: logging disabled (default)
  - Non-`0`: logging enabled
- `GOGGLES_DEBUG_LOG_LEVEL`
  - Accepted: `trace|debug|info|warn|error|critical|off`
  - When logging is enabled and this variable is unset/invalid: default `info`

Both values are read once and cached (thread-safe, static initialization).

## Launcher / CLI Forwarding
Goggles’ default mode launcher SHOULD provide CLI flags that set these environment variables for
the spawned target application, similar to existing `--dump-*` options.

## Backend
- Use `::write(STDERR_FILENO, buf, len)` for emission.
- Use a fixed-size stack buffer (or `thread_local` buffer) and `vsnprintf`.
- Prefix format: `[goggles_vklayer] <level>: <message>\n`
- Truncation policy: ensure trailing `\n` is present even when truncated.

## Anti-spam
Provide macros that build on the same backend and do not allocate:
- `LAYER_*_ONCE(...)` (static atomic flag)
- `LAYER_*_EVERY_N(n, ...)` (static atomic counter)

Prefer using anti-spam only for paths that can repeat rapidly.

## Testing Strategy
Unit tests run in-process with Catch2:
- Redirect `stderr` to a pipe (`pipe`, `dup2`), invoke `LAYER_*`, restore `stderr`.
- Assert:
  - Disabled-by-default emits no bytes
  - Enabled emits prefix/level/message
  - Level filtering drops lower-severity messages
  - Anti-spam “once” emits exactly once
  - Truncation keeps prefix and newline
