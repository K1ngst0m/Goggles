## 1. Implementation

- [x] 1.1 Replace `posix_spawnp()` with `fork()` + `execvp()` in `spawn_target_app()`
- [x] 1.2 Add `#include <sys/prctl.h>` header
- [x] 1.3 In child process (after fork), call `prctl(PR_SET_PDEATHSIG, SIGKILL)` before exec
- [x] 1.4 Add check for parent death race condition (verify parent PID hasn't changed to 1)

## 2. Automated Testing

- [x] 2.1 Create `tests/app/test_child_death_signal.cpp` with process death signal test
- [x] 2.2 Add test: child receives SIGKILL when parent exits

## 3. Manual Testing

- [ ] 3.1 Manual test: `goggles -- vkcube`, kill goggles with SIGKILL, verify vkcube terminates
- [ ] 3.2 Manual test: Normal exit, verify child process terminates gracefully
