## 1. goggles-reaper Implementation

- [x] 1.1 Create `src/app/reaper_main.cpp`
- [x] 1.2 Set `PR_SET_CHILD_SUBREAPER` to adopt orphaned descendants
- [x] 1.3 Set `PR_SET_PDEATHSIG(SIGTERM)` to detect parent death
- [x] 1.4 Set up signal handlers for SIGTERM/SIGINT/SIGHUP to trigger child cleanup
- [x] 1.5 Spawn target app via `fork()` + `execvp()`
- [x] 1.6 Implement `kill_process_tree()` - recursively kill children via `/proc` scanning
- [x] 1.7 On signal: kill all children, wait for them, then exit
- [x] 1.8 Update `src/app/CMakeLists.txt` to build `goggles-reaper`

## 2. main.cpp Changes

- [x] 2.1 Use `posix_spawn()` to exec `goggles-reaper` (safe: immediate exec)
- [x] 2.2 Pass env vars and target command as arguments to reaper
- [x] 2.3 Remove direct `PR_SET_PDEATHSIG` usage from main.cpp

## 3. Testing

- [x] 3.1 Update `tests/app/test_child_death_signal.cpp` for reaper architecture
- [x] 3.2 Manual test: kill goggles, verify target app terminates
- [x] 3.3 Manual test: target app exits, verify goggles exits cleanly