// goggles-reaper: single-threaded watchdog process
// Ensures target app and descendants are terminated when goggles dies

#include <array>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

volatile sig_atomic_t g_should_run = 1;

auto get_child_pids(pid_t parent_pid) -> std::vector<pid_t> {
    std::vector<pid_t> pids;
    DIR* proc_dir = opendir("/proc");
    if (proc_dir == nullptr) {
        return pids;
    }

    struct dirent* entry = nullptr;
    while ((entry = readdir(proc_dir)) != nullptr) {
        if (entry->d_type != DT_DIR || entry->d_name[0] < '0' || entry->d_name[0] > '9') {
            continue;
        }

        std::array<char, 64> path{};
        std::snprintf(path.data(), path.size(), "/proc/%s/stat", entry->d_name);

        FILE* stat_file = std::fopen(path.data(), "r");
        if (stat_file == nullptr) {
            continue;
        }

        std::array<char, 512> buf{};
        if (std::fgets(buf.data(), buf.size(), stat_file) == nullptr) {
            std::fclose(stat_file);
            continue;
        }
        std::fclose(stat_file);

        const char* comm_end = std::strrchr(buf.data(), ')');
        if (comm_end == nullptr || comm_end[1] == '\0') {
            continue;
        }

        char state = 0;
        pid_t ppid = -1;
        if (std::sscanf(comm_end + 1, " %c %d", &state, &ppid) != 2) {
            continue;
        }

        if (ppid == parent_pid) {
            pids.push_back(static_cast<pid_t>(std::atoi(entry->d_name)));
        }
    }
    closedir(proc_dir);
    return pids;
}

auto kill_process_tree(pid_t pid, int sig) -> void {
    const auto children = get_child_pids(pid);
    kill(pid, sig);
    for (const pid_t child : children) {
        kill_process_tree(child, sig);
    }
}

auto kill_all_children(int sig) -> void {
    const auto children = get_child_pids(getpid());
    for (const pid_t child : children) {
        kill_process_tree(child, sig);
    }
}

auto wait_all_children() -> void {
    while (waitpid(-1, nullptr, 0) > 0 || errno == EINTR) {
    }
}

auto signal_handler(int /*sig*/) -> void {
    g_should_run = 0;
}

auto setup_signal_handlers() -> void {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);
}

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 2) {
        std::fprintf(stderr, "goggles-reaper: missing command\n");
        return EXIT_FAILURE;
    }

    const pid_t parent_pid = getppid();

    // Become subreaper - orphaned descendants reparent to us
    prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);

    // Die when parent (goggles) dies
    prctl(PR_SET_PDEATHSIG, SIGTERM, 0, 0, 0);
    if (getppid() != parent_pid) {
        return EXIT_FAILURE;
    }

    setup_signal_handlers();

    // Fork and exec target app
    const pid_t child = fork();
    if (child < 0) {
        std::fprintf(stderr, "goggles-reaper: fork failed: %s\n", std::strerror(errno));
        return EXIT_FAILURE;
    }

    if (child == 0) {
        execvp(argv[1], &argv[1]);
        std::fprintf(stderr, "goggles-reaper: exec failed: %s\n", std::strerror(errno));
        _exit(EXIT_FAILURE);
    }

    // Wait for primary child or signal
    int status = 0;
    while (g_should_run != 0) {
        const pid_t pid = waitpid(child, &status, 0);
        if (pid == child) {
            break;
        }
        if (pid == -1 && errno != EINTR) {
            break;
        }
    }

    // Cleanup all remaining children
    g_should_run = 0;
    kill_all_children(SIGKILL);
    wait_all_children();

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return EXIT_FAILURE;
}
