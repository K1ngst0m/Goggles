#pragma once

#include <cstdint>

namespace goggles::diagnostics {

enum class PolicyMode : uint8_t { compatibility, strict };

enum class CaptureMode : uint8_t { minimal, standard, investigate, forensic };

enum class ActivationTier : uint8_t { tier0, tier1, tier2 };

enum class GpuTimestampAvailabilityMode : uint8_t { auto_detect, force_unavailable };

struct DiagnosticPolicy {
    PolicyMode mode = PolicyMode::compatibility;
    CaptureMode capture_mode = CaptureMode::standard;
    ActivationTier tier = ActivationTier::tier0;
    uint32_t capture_frame_limit = 1;
    uint64_t retention_bytes = 256ULL * 1024 * 1024;
    bool promote_fallback_to_error = false;
    bool reflection_loss_is_fatal = false;
    GpuTimestampAvailabilityMode gpu_timestamp_availability =
        GpuTimestampAvailabilityMode::auto_detect;
};

/// @brief Creates a policy with strict-mode derived fields set.
inline auto make_strict_policy() -> DiagnosticPolicy {
    DiagnosticPolicy policy;
    policy.mode = PolicyMode::strict;
    policy.promote_fallback_to_error = true;
    policy.reflection_loss_is_fatal = true;
    return policy;
}

} // namespace goggles::diagnostics
