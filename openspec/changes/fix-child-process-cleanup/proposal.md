# Change: Fix child process cleanup on parent crash

## Why

When Goggles crashes (e.g., due to Vulkan errors or device mismatch in multi-GPU systems), the spawned child process continues running headlessly. Users are unaware the child is still running, and subsequent launches may fail due to resource conflicts (e.g., capture socket already bound).

The previous approach using `PR_SET_PDEATHSIG` directly from a multi-threaded process is unreliable because the signal is tied to the **thread** that called `fork()`, not the entire process.

## What Changes

Introduce a `goggles-reaper` watchdog process:

```
Goggles (main process, multi-threaded)
    ↓ fork() + exec("goggles-reaper") [SAFE: immediate exec]
goggles-reaper (watchdog, single-threaded)
    ↓ PR_SET_CHILD_SUBREAPER + PR_SET_PDEATHSIG
    ↓ fork() + exec(target_app) [SAFE: no threads]
Target Application
```

Benefits:
- Single-threaded reaper ensures `PR_SET_PDEATHSIG` works reliably
- `PR_SET_CHILD_SUBREAPER` catches orphaned grandchildren
- Process group kill ensures all descendants are terminated

## Impact

- Affected specs: `app-window`
- Affected code:
  - `src/app/main.cpp` - spawn reaper instead of target app directly
  - `src/app/reaper_main.cpp` - new watchdog process
  - `src/app/CMakeLists.txt` - build reaper executable