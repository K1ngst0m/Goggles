// Integration test: validates goggles-reaper kills children when parent dies

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

constexpr int EXIT_TEST_PASS = 0;
constexpr int EXIT_TEST_FAIL = 1;

} // namespace

int main() {
    std::printf("Testing goggles-reaper behavior...\n");

    // Spawn: goggles-reaper sleep 60
    // Then kill the reaper's parent (this test process spawns an intermediate)
    // and verify the sleep process is also killed.

    // We need to fork a child that spawns the reaper, then we kill that child
    const pid_t child = fork();
    if (child < 0) {
        std::fprintf(stderr, "fork failed: %s\n", std::strerror(errno));
        return EXIT_TEST_FAIL;
    }

    if (child == 0) {
        // Child: spawn goggles-reaper with sleep 60
        std::array<char*, 4> argv = {const_cast<char*>("goggles-reaper"),
                                     const_cast<char*>("sleep"), const_cast<char*>("60"), nullptr};

        pid_t reaper_pid = -1;
        const int rc =
            posix_spawnp(&reaper_pid, "goggles-reaper", nullptr, nullptr, argv.data(), environ);
        if (rc != 0) {
            std::fprintf(stderr, "posix_spawnp failed: %s\n", std::strerror(rc));
            _exit(EXIT_TEST_FAIL);
        }

        // Wait a bit for reaper to start
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Now exit - reaper should receive SIGTERM and kill its children
        _exit(EXIT_TEST_PASS);
    }

    // Parent: wait for child to exit
    int status = 0;
    waitpid(child, &status, 0);

    // Give reaper time to clean up
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Check if any sleep process is still running (there shouldn't be)
    FILE* pgrep = popen("pgrep -x sleep 2>/dev/null | wc -l", "r");
    if (pgrep == nullptr) {
        std::fprintf(stderr, "popen failed\n");
        return EXIT_TEST_FAIL;
    }

    int sleep_count = 0;
    std::fscanf(pgrep, "%d", &sleep_count);
    pclose(pgrep);

    if (sleep_count == 0) {
        std::printf("PASS: No orphaned sleep processes found\n");
        return EXIT_TEST_PASS;
    }

    std::fprintf(stderr, "FAIL: Found %d sleep processes still running\n", sleep_count);
    return EXIT_TEST_FAIL;
}
