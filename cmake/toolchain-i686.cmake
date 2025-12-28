# CMake toolchain file for 32-bit (i686) cross-compilation
# Usage: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-i686.cmake ...

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

# Use gcc from the Pixi environment
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

# Target triple for 32-bit Linux
set(TRIPLE "i686-linux-gnu")

# Locate the managed sysroot in the Pixi environment
if(DEFINED ENV{CONDA_PREFIX})
    set(SYSROOT_PATH "$ENV{CONDA_PREFIX}/i686-sysroot")
else()
    message(FATAL_ERROR "CONDA_PREFIX not set. This toolchain must be run within the Pixi environment.")
endif()

if(NOT EXISTS "${SYSROOT_PATH}")
    message(FATAL_ERROR
        "Sysroot not found at ${SYSROOT_PATH}.\n"
        "Build the sysroot-i686 package first with: pixi run build-i686")
endif()

set(CMAKE_SYSROOT "${SYSROOT_PATH}")

# Compiler flags
# -m32: Force 32-bit
# --sysroot: Specifies the root directory for headers/libs
# -B: Directory to search for startup files and libraries
set(ARCH_FLAGS "-m32 --sysroot=${SYSROOT_PATH} -B${SYSROOT_PATH}/usr/lib")

# C++ include paths for cross-compilation with external sysroot
set(CXX_INCLUDE_FLAGS "-isystem ${SYSROOT_PATH}/usr/include/c++/14 -isystem ${SYSROOT_PATH}/usr/include/c++/14/i686-linux-gnu")

# Library search path for 32-bit libraries
set(LIB_FLAGS "-L${SYSROOT_PATH}/usr/lib -static-libstdc++ -static-libgcc")

set(CMAKE_C_FLAGS_INIT "${ARCH_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${ARCH_FLAGS} ${CXX_INCLUDE_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${ARCH_FLAGS}")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "${LIB_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${LIB_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${LIB_FLAGS}")

# Search paths
set(CMAKE_FIND_ROOT_PATH "${SYSROOT_PATH}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)

# Help find_library
set(CMAKE_LIBRARY_ARCHITECTURE "i386-linux-gnu")
