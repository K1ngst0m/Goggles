// Integration test: validates waitid(WNOWAIT) child-exit detection used by
// the headless frame loop.  The mechanism must peek at the child's exit status
// without consuming the zombie so that terminate_child() in main.cpp can still
// reap it safely.

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

constexpr int EXIT_TEST_PASS = 0;
constexpr int EXIT_TEST_FAIL = 1;

// Mirrors the detection logic in Application::run_headless.
auto child_has_exited(pid_t pid) -> bool {
    siginfo_t info{};
    int ret = waitid(P_PID, static_cast<id_t>(pid), &info, WEXITED | WNOHANG | WNOWAIT);
    return ret == 0 && info.si_pid != 0;
}

} // namespace

int main() {
    std::printf("Test 1: waitid(WNOWAIT) detects exited child...\n");
    {
        const pid_t child = fork();
        if (child < 0) {
            std::fprintf(stderr, "  fork failed: %s\n", std::strerror(errno));
            return EXIT_TEST_FAIL;
        }
        if (child == 0) {
            _exit(42);
        }

        // Give child time to exit and become a zombie.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!child_has_exited(child)) {
            std::fprintf(stderr, "  FAIL: waitid(WNOWAIT) did not detect exited child\n");
            waitpid(child, nullptr, 0);
            return EXIT_TEST_FAIL;
        }
        std::printf("  PASS: exited child detected\n");

        // Verify the zombie was NOT consumed — waitpid must still succeed.
        int status = 0;
        pid_t reaped = waitpid(child, &status, WNOHANG);
        if (reaped != child) {
            std::fprintf(stderr, "  FAIL: zombie was consumed by waitid (reaped=%d, errno=%s)\n",
                         reaped, std::strerror(errno));
            return EXIT_TEST_FAIL;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 42) {
            std::fprintf(stderr, "  FAIL: unexpected exit status %d\n", status);
            return EXIT_TEST_FAIL;
        }
        std::printf("  PASS: zombie preserved for waitpid (exit code %d)\n", WEXITSTATUS(status));
    }

    std::printf("Test 2: waitid(WNOWAIT) reports nothing for running child...\n");
    {
        const pid_t child = fork();
        if (child < 0) {
            std::fprintf(stderr, "  fork failed: %s\n", std::strerror(errno));
            return EXIT_TEST_FAIL;
        }
        if (child == 0) {
            // Sleep long enough for the parent to check.
            std::this_thread::sleep_for(std::chrono::seconds(5));
            _exit(0);
        }

        // Child is still running.
        if (child_has_exited(child)) {
            std::fprintf(stderr,
                         "  FAIL: waitid(WNOWAIT) falsely reported running child as exited\n");
            kill(child, SIGKILL);
            waitpid(child, nullptr, 0);
            return EXIT_TEST_FAIL;
        }
        std::printf("  PASS: running child not reported as exited\n");

        // Clean up.
        kill(child, SIGKILL);
        waitpid(child, nullptr, 0);
    }

    std::printf("Test 3: after reap, waitid(WNOWAIT) does not detect stale PID...\n");
    {
        const pid_t child = fork();
        if (child < 0) {
            std::fprintf(stderr, "  fork failed: %s\n", std::strerror(errno));
            return EXIT_TEST_FAIL;
        }
        if (child == 0) {
            _exit(0);
        }

        waitpid(child, nullptr, 0);

        // After reaping, waitid should return -1/ECHILD or 0 with si_pid==0.
        if (child_has_exited(child)) {
            std::fprintf(stderr, "  FAIL: waitid(WNOWAIT) detected already-reaped child\n");
            return EXIT_TEST_FAIL;
        }
        std::printf("  PASS: reaped child not falsely detected\n");
    }

    std::printf("All headless child-exit detection tests passed.\n");
    return EXIT_TEST_PASS;
}
