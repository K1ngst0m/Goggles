#pragma once

// =============================================================================
// Profiling Macros
// =============================================================================
/// @brief Tracy profiling macros.
///
/// When `TRACY_ENABLE` is not defined, all macros expand to no-ops (zero overhead).

// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#ifdef TRACY_ENABLE

#include <cstring>
#include <tracy/Tracy.hpp>

#define GOGGLES_PROFILE_FRAME(name) FrameMarkNamed(name)

#define GOGGLES_PROFILE_FUNCTION() ZoneScoped

#define GOGGLES_PROFILE_SCOPE(name) ZoneScopedN(name)

#define GOGGLES_PROFILE_TAG(text)                                                                  \
    do {                                                                                           \
        ZoneText(text, std::strlen(text));                                                         \
    } while (0)

#define GOGGLES_PROFILE_VALUE(name, value) TracyPlot(name, value)

#else // !TRACY_ENABLE

#define GOGGLES_PROFILE_FRAME(name) (void)0
#define GOGGLES_PROFILE_FUNCTION() (void)0
#define GOGGLES_PROFILE_SCOPE(name) (void)0
#define GOGGLES_PROFILE_TAG(text) (void)0
#define GOGGLES_PROFILE_VALUE(name, value) (void)0

#endif // TRACY_ENABLE

// NOLINTEND(cppcoreguidelines-macro-usage)
