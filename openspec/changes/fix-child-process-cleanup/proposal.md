# Change: Fix child process cleanup on parent crash

## Why

When Goggles crashes (e.g., due to Vulkan errors or device mismatch in multi-GPU systems), the spawned child process continues running headlessly. Users are unaware the child is still running, and subsequent launches may fail due to resource conflicts (e.g., capture socket already bound).

## What Changes

- Use `fork()` + `execvp()` instead of `posix_spawnp()` to spawn child processes
- Set `prctl(PR_SET_PDEATHSIG, SIGTERM)` in child process before exec, ensuring automatic termination when parent dies
- Add safeguard against PID 1 reparenting race condition

## Impact

- Affected specs: `app-window`
- Affected code: `src/app/main.cpp` (spawn_target_app function)