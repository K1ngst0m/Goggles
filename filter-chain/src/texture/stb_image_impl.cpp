// Dedicated translation unit for the stb_image implementation.
// Isolated here so the heavy third-party code is not subject to
// project clang-tidy analysis (avoids false-positive clang-analyzer
// diagnostics reported inside the header).

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
