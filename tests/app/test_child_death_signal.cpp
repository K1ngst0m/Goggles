// Integration test: validates child process receives SIGTERM when parent dies
// Uses prctl(PR_SET_PDEATHSIG) which is Linux-specific

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

constexpr int EXIT_TEST_PASS = 0;
constexpr int EXIT_TEST_FAIL = 1;
constexpr int CHILD_SIGNAL_RECEIVED = 42;

volatile sig_atomic_t g_signal_received = 0;

void signal_handler(int sig) {
    if (sig == SIGTERM) {
        g_signal_received = 1;
    }
}

// Child process that waits for SIGTERM from parent death
[[noreturn]] void run_grandchild(pid_t expected_parent) {
    signal(SIGTERM, signal_handler);
    prctl(PR_SET_PDEATHSIG, SIGTERM);

    if (getppid() != expected_parent) {
        _exit(EXIT_TEST_FAIL);
    }

    for (int i = 0; i < 100 && g_signal_received == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    _exit(g_signal_received != 0 ? CHILD_SIGNAL_RECEIVED : EXIT_TEST_FAIL);
}

// Intermediate process that spawns grandchild then exits
[[noreturn]] void run_child() {
    const pid_t my_pid = getpid();
    const pid_t grandchild = fork();

    if (grandchild < 0) {
        std::fprintf(stderr, "fork grandchild failed: %s\n", std::strerror(errno));
        _exit(EXIT_TEST_FAIL);
    }

    if (grandchild == 0) {
        run_grandchild(my_pid);
    }

    // Parent of grandchild: wait briefly then exit to trigger grandchild's death signal
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    _exit(EXIT_TEST_PASS);
}

} // namespace

int main() {
    std::printf("Testing PR_SET_PDEATHSIG behavior...\n");

    // Become a subreaper so orphaned grandchild is reparented to us
    if (prctl(PR_SET_CHILD_SUBREAPER, 1) != 0) {
        std::fprintf(stderr, "prctl(PR_SET_CHILD_SUBREAPER) failed: %s\n", std::strerror(errno));
        return EXIT_TEST_FAIL;
    }

    const pid_t child = fork();
    if (child < 0) {
        std::fprintf(stderr, "fork child failed: %s\n", std::strerror(errno));
        return EXIT_TEST_FAIL;
    }

    if (child == 0) {
        run_child();
    }

    // Wait for intermediate child
    int child_status = 0;
    waitpid(child, &child_status, 0);

    // Wait for grandchild (now orphaned, reparented to us or init)
    // We need to wait a bit for the grandchild to receive signal and exit
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Try to reap any grandchild
    int grandchild_status = 0;
    const pid_t reaped = waitpid(-1, &grandchild_status, WNOHANG);

    if (reaped > 0 && WIFEXITED(grandchild_status)) {
        const int exit_code = WEXITSTATUS(grandchild_status);
        if (exit_code == CHILD_SIGNAL_RECEIVED) {
            std::printf("PASS: Grandchild received SIGTERM on parent death\n");
            return EXIT_TEST_PASS;
        }
        std::fprintf(stderr, "FAIL: Grandchild exited with code %d\n", exit_code);
        return EXIT_TEST_FAIL;
    }

    std::fprintf(stderr, "FAIL: Could not reap grandchild (reaped=%d)\n", reaped);
    return EXIT_TEST_FAIL;
}