// Integration test: validates goggles-reaper kills children when parent dies

#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {

constexpr int EXIT_TEST_PASS = 0;
constexpr int EXIT_TEST_FAIL = 1;

auto get_reaper_path() -> std::string {
    std::array<char, 4096> exe_path{};
    const ssize_t len = readlink("/proc/self/exe", exe_path.data(), exe_path.size() - 1);
    if (len <= 0) {
        return "goggles-reaper";
    }
    exe_path[static_cast<size_t>(len)] = '\0';

    std::string path(exe_path.data());
    // Test binary is in build/debug/tests/, reaper is in build/debug/bin/
    const auto pos = path.rfind('/');
    if (pos != std::string::npos) {
        path = path.substr(0, pos);
        const auto pos2 = path.rfind('/');
        if (pos2 != std::string::npos) {
            path = path.substr(0, pos2);
        }
    }
    return path + "/bin/goggles-reaper";
}

} // namespace

int main() {
    std::printf("Testing goggles-reaper behavior...\n");

    const std::string reaper_path = get_reaper_path();

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
        std::array<char*, 4> argv = {const_cast<char*>(reaper_path.c_str()),
                                     const_cast<char*>("sleep"), const_cast<char*>("60"), nullptr};

        pid_t reaper_pid = -1;
        const int rc =
            posix_spawn(&reaper_pid, reaper_path.c_str(), nullptr, nullptr, argv.data(), environ);
        if (rc != 0) {
            std::fprintf(stderr, "posix_spawn failed: %s\n", std::strerror(rc));
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
