# CMake toolchain file for 32-bit (i686) cross-compilation using Clang + Local Sysroot
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-i686.cmake ...

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Use Clang from the Pixi environment
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# Target triple for 32-bit Linux
set(TRIPLE "i686-linux-gnu")

# Locate the managed sysroot in the Pixi environment
if(DEFINED ENV{CONDA_PREFIX})
    set(SYSROOT_PATH "$ENV{CONDA_PREFIX}/i686-sysroot")
else()
    message(FATAL_ERROR "CONDA_PREFIX not set. This toolchain must be run within the Pixi environment.")
endif()

if(NOT EXISTS "${SYSROOT_PATH}")
    message(WARNING "Sysroot not found at ${SYSROOT_PATH}. Build the sysroot-i686 package first.")
endif()

set(CMAKE_SYSROOT "${SYSROOT_PATH}")

# Compiler flags
# --target: Specifies architecture
# --sysroot: Specifies the root directory for headers/libs
# -m32: Force 32-bit
set(ARCH_FLAGS "--target=${TRIPLE} -m32 --sysroot=${SYSROOT_PATH}")

set(CMAKE_C_FLAGS_INIT "${ARCH_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${ARCH_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${ARCH_FLAGS}")

# Linker flags
# Use lld if available (usually is with clang in conda-forge)
set(CMAKE_EXE_LINKER_FLAGS_INIT "-fuse-ld=lld ${ARCH_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-fuse-ld=lld ${ARCH_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-fuse-ld=lld ${ARCH_FLAGS}")

# Search paths
set(CMAKE_FIND_ROOT_PATH "${SYSROOT_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# Help find_library
set(CMAKE_LIBRARY_ARCHITECTURE "i386-linux-gnu")