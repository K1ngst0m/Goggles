# Threading Support Helper
# Provides function to enable threading dependencies when performance requires it
# See: docs/threading_architecture.md for design rationale and trigger conditions

# Enable threading dependencies for the Goggles project
# Call this function when profiling shows main thread CPU time > 8ms per frame
#
# Usage:
#   include(cmake/Threading.cmake)
#   enable_goggles_threading()
function(enable_goggles_threading)
    message(STATUS "Enabling Goggles threading support...")

    # Define preprocessor flag for conditional compilation
    add_compile_definitions(GOGGLES_THREADING_ENABLED)

    # BS::thread_pool - Simple job submission (Phase 1)
    CPMAddPackage(
        NAME BS_thread_pool
        GITHUB_REPOSITORY bshoshany/thread-pool
        VERSION 3.5.0
        OPTIONS
            "BUILD_TESTS OFF"
            "BUILD_EXAMPLES OFF"
    )

    # rigtorp::SPSCQueue - Wait-free queue for real-time IPC
    CPMAddPackage(
        NAME rigtorp_SPSCQueue
        GITHUB_REPOSITORY rigtorp/SPSCQueue
        VERSION 1.2.0
    )

    # Future: Taskflow for dependency-aware scheduling (Phase 2)
    # Uncomment when upgrading from BS::thread_pool
    # CPMAddPackage(
    #     NAME taskflow
    #     GITHUB_REPOSITORY taskflow/taskflow
    #     VERSION 3.10.0
    #     OPTIONS
    #         "TF_BUILD_EXAMPLES OFF"
    #         "TF_BUILD_TESTS OFF"
    # )

    message(STATUS "Threading libraries configured. Recompile to activate.")
endfunction()
