#pragma once

#include <cstdint>
#include <filesystem>
#include <goggles/error.hpp>
#include <string>
#include <vector>

namespace goggles::test {

struct Image {
    std::vector<std::uint8_t> data;
    int width = 0;
    int height = 0;
    int channels = 0;
};

struct Rect {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

struct CompareResult {
    bool passed = false;
    double max_channel_diff = 0.0;
    double mean_diff = 0.0;
    double structural_similarity = 1.0;
    std::uint32_t failing_pixels = 0;
    double failing_percentage = 0.0;
    std::string error_message;
};

[[nodiscard]] auto load_png(const std::filesystem::path& path) -> goggles::Result<Image>;

[[nodiscard]] auto compare_images(const Image& actual, const Image& reference, double tolerance,
                                  const std::filesystem::path& diff_out = {},
                                  bool compute_ssim = false) -> CompareResult;

[[nodiscard]] auto compare_images(const Image& actual, const Image& reference, double tolerance,
                                  const Rect& roi, const std::filesystem::path& diff_out = {},
                                  bool compute_ssim = false) -> CompareResult;

[[nodiscard]] auto generate_diff_heatmap(const Image& actual, const Image& reference,
                                         const std::filesystem::path& output) -> Result<void>;

} // namespace goggles::test
