#include "filter_controls.hpp"

#include <algorithm>

namespace goggles::render {

namespace {

constexpr std::uint64_t FNV1A_OFFSET = 14695981039346656037ULL;
constexpr std::uint64_t FNV1A_PRIME = 1099511628211ULL;

auto hash_append(std::uint64_t hash, std::string_view value) -> std::uint64_t {
    for (char ch : value) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(ch));
        hash *= FNV1A_PRIME;
    }
    return hash;
}

} // namespace

auto make_filter_control_id(FilterControlStage stage, std::string_view name) -> FilterControlId {
    std::uint64_t hash = FNV1A_OFFSET;
    hash = hash_append(hash, std::string_view{to_string(stage)});
    hash = hash_append(hash, ":");
    hash = hash_append(hash, name);
    return hash;
}

auto clamp_filter_control_value(const FilterControlDescriptor& descriptor, float value) -> float {
    float min_value = descriptor.min_value;
    float max_value = descriptor.max_value;
    if (min_value > max_value) {
        std::swap(min_value, max_value);
    }
    return std::clamp(value, min_value, max_value);
}

} // namespace goggles::render
