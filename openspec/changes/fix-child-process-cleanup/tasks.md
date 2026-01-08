## 1. Implementation

- [ ] 1.1 Replace `posix_spawnp()` with `fork()` + `execvp()` in `spawn_target_app()`
- [ ] 1.2 Add `#include <sys/prctl.h>` header
- [ ] 1.3 In child process (after fork), call `prctl(PR_SET_PDEATHSIG, SIGTERM)` before exec
- [ ] 1.4 Add check for parent death race condition (verify parent PID hasn't changed to 1)

## 2. Automated Testing

- [ ] 2.1 Create `tests/app/test_process_utils.cpp` with process utility tests
- [ ] 2.2 Add test: child receives SIGTERM when parent is killed
- [ ] 2.3 Add test: normal parent exit terminates child gracefully

## 3. Manual Testing

- [ ] 3.1 Manual test: `goggles -- vkcube`, kill goggles with SIGKILL, verify vkcube terminates
- [ ] 3.2 Manual test: Normal exit, verify child process terminates gracefully
- [ ] 3.3 Manual test: Child exits first, verify goggles exits cleanly