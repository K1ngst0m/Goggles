#include "image_compare.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stb_image.h>
#include <stb_image_write.h>
#include <string>
#include <vector>

namespace goggles::test {

namespace {

struct RoiBounds {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
};

auto build_size_mismatch_message(const Image& actual, const Image& reference) -> std::string {
    return "Size mismatch: " + std::to_string(actual.width) + "x" + std::to_string(actual.height) +
           " vs " + std::to_string(reference.width) + "x" + std::to_string(reference.height);
}

auto build_write_failure_message(const std::filesystem::path& path) -> std::string {
    return "Failed to write diff PNG: " + path.string();
}

auto build_invalid_roi_message(const Rect& roi) -> std::string {
    return "Invalid ROI: x=" + std::to_string(roi.x) + ", y=" + std::to_string(roi.y) +
           ", width=" + std::to_string(roi.width) + ", height=" + std::to_string(roi.height);
}

auto compute_roi_bounds(const Image& image, const Rect* roi) -> std::optional<RoiBounds> {
    if (roi == nullptr) {
        return RoiBounds{.x0 = 0, .y0 = 0, .x1 = image.width, .y1 = image.height};
    }

    const int x0 = std::clamp(roi->x, 0, image.width);
    const int y0 = std::clamp(roi->y, 0, image.height);
    const int x1 = std::clamp(roi->x + roi->width, 0, image.width);
    const int y1 = std::clamp(roi->y + roi->height, 0, image.height);
    if (roi->width <= 0 || roi->height <= 0 || x1 <= x0 || y1 <= y0) {
        return std::nullopt;
    }

    return RoiBounds{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1};
}

auto pixel_offset(const Image& image, const int x, const int y) -> std::size_t {
    return (static_cast<std::size_t>(y) * static_cast<std::size_t>(image.width) +
            static_cast<std::size_t>(x)) *
           4U;
}

auto luminance_at(const Image& image, const int x, const int y) -> double {
    const auto offset = pixel_offset(image, x, y);
    const double red = static_cast<double>(image.data[offset + 0U]) / 255.0;
    const double green = static_cast<double>(image.data[offset + 1U]) / 255.0;
    const double blue = static_cast<double>(image.data[offset + 2U]) / 255.0;
    return (0.2126 * red) + (0.7152 * green) + (0.0722 * blue);
}

auto compute_ssim_value(const Image& actual, const Image& reference, const RoiBounds& roi)
    -> double {
    constexpr double C1 = 0.01 * 0.01;
    constexpr double C2 = 0.03 * 0.03;

    const auto width = static_cast<std::size_t>(roi.x1 - roi.x0);
    const auto height = static_cast<std::size_t>(roi.y1 - roi.y0);
    const auto sample_count = width * height;
    if (sample_count == 0U) {
        return 1.0;
    }

    double mean_actual = 0.0;
    double mean_reference = 0.0;
    for (int y = roi.y0; y < roi.y1; ++y) {
        for (int x = roi.x0; x < roi.x1; ++x) {
            mean_actual += luminance_at(actual, x, y);
            mean_reference += luminance_at(reference, x, y);
        }
    }
    mean_actual /= static_cast<double>(sample_count);
    mean_reference /= static_cast<double>(sample_count);

    double variance_actual = 0.0;
    double variance_reference = 0.0;
    double covariance = 0.0;
    for (int y = roi.y0; y < roi.y1; ++y) {
        for (int x = roi.x0; x < roi.x1; ++x) {
            const double actual_luma = luminance_at(actual, x, y) - mean_actual;
            const double reference_luma = luminance_at(reference, x, y) - mean_reference;
            variance_actual += actual_luma * actual_luma;
            variance_reference += reference_luma * reference_luma;
            covariance += actual_luma * reference_luma;
        }
    }

    if (sample_count > 1U) {
        const auto divisor = static_cast<double>(sample_count - 1U);
        variance_actual /= divisor;
        variance_reference /= divisor;
        covariance /= divisor;
    }

    const double numerator = (2.0 * mean_actual * mean_reference + C1) * (2.0 * covariance + C2);
    const double denominator =
        ((mean_actual * mean_actual) + (mean_reference * mean_reference) + C1) *
        (variance_actual + variance_reference + C2);
    if (denominator <= 0.0) {
        return 1.0;
    }

    return std::clamp(numerator / denominator, 0.0, 1.0);
}

void paint_heatmap_pixel(std::vector<std::uint8_t>& heatmap, std::size_t offset, double magnitude) {
    const double clamped = std::clamp(magnitude, 0.0, 1.0);
    const double red = std::clamp((clamped - 0.5) * 2.0, 0.0, 1.0);
    const double blue = std::clamp((0.5 - clamped) * 2.0, 0.0, 1.0);
    const double green = 1.0 - std::abs((clamped * 2.0) - 1.0);
    heatmap[offset + 0U] = static_cast<std::uint8_t>(red * 255.0);
    heatmap[offset + 1U] = static_cast<std::uint8_t>(green * 255.0);
    heatmap[offset + 2U] = static_cast<std::uint8_t>(blue * 255.0);
    heatmap[offset + 3U] = 255U;
}

auto compare_images_impl(const Image& actual, const Image& reference, const double tolerance,
                         const Rect* roi, const std::filesystem::path& diff_out,
                         const bool compute_ssim) -> CompareResult {
    if (actual.width != reference.width || actual.height != reference.height) {
        CompareResult result;
        result.structural_similarity = 0.0;
        result.error_message = build_size_mismatch_message(actual, reference);
        return result;
    }

    const auto roi_bounds = compute_roi_bounds(actual, roi);
    if (!roi_bounds.has_value()) {
        CompareResult result;
        result.structural_similarity = 0.0;
        result.error_message = build_invalid_roi_message(*roi);
        return result;
    }

    const auto bounds = *roi_bounds;
    const std::size_t pixel_count = static_cast<std::size_t>(bounds.x1 - bounds.x0) *
                                    static_cast<std::size_t>(bounds.y1 - bounds.y0);

    CompareResult result;
    double total_diff_sum = 0.0;
    std::vector<std::uint8_t> diff_data;
    if (!diff_out.empty()) {
        diff_data.resize(static_cast<std::size_t>(actual.width) *
                             static_cast<std::size_t>(actual.height) * 4U,
                         0);
        for (int y = 0; y < actual.height; ++y) {
            for (int x = 0; x < actual.width; ++x) {
                const auto offset = pixel_offset(actual, x, y);
                for (std::size_t channel = 0; channel < 3U; ++channel) {
                    diff_data[offset + channel] = static_cast<std::uint8_t>(
                        static_cast<double>(actual.data[offset + channel]) * 0.25);
                }
                diff_data[offset + 3U] = 255U;
            }
        }
    }

    for (int y = bounds.y0; y < bounds.y1; ++y) {
        for (int x = bounds.x0; x < bounds.x1; ++x) {
            const auto offset = pixel_offset(actual, x, y);
            double pixel_max_diff = 0.0;
            double pixel_channel_sum = 0.0;

            for (std::size_t channel = 0; channel < 4U; ++channel) {
                const int delta = std::abs(static_cast<int>(actual.data[offset + channel]) -
                                           static_cast<int>(reference.data[offset + channel]));
                const double channel_diff = static_cast<double>(delta) / 255.0;
                pixel_max_diff = std::max(pixel_max_diff, channel_diff);
                pixel_channel_sum += channel_diff;
            }

            result.max_channel_diff = std::max(result.max_channel_diff, pixel_max_diff);
            total_diff_sum += pixel_channel_sum / 4.0;

            const bool pixel_failed = pixel_max_diff > tolerance;
            if (pixel_failed) {
                ++result.failing_pixels;
            }

            if (!diff_data.empty() && pixel_failed) {
                diff_data[offset + 0U] = 255U;
                diff_data[offset + 1U] = 0U;
                diff_data[offset + 2U] = 0U;
                diff_data[offset + 3U] = 255U;
            }
        }
    }

    if (pixel_count > 0U) {
        result.mean_diff = total_diff_sum / static_cast<double>(pixel_count);
        result.failing_percentage =
            (static_cast<double>(result.failing_pixels) * 100.0) / static_cast<double>(pixel_count);
    }

    if (compute_ssim) {
        result.structural_similarity = compute_ssim_value(actual, reference, bounds);
    }

    if (!diff_out.empty() && result.failing_pixels > 0U) {
        const int stride = actual.width * 4;
        const int write_status = stbi_write_png(diff_out.string().c_str(), actual.width,
                                                actual.height, 4, diff_data.data(), stride);
        if (write_status == 0) {
            result.error_message = build_write_failure_message(diff_out);
        }
    }

    result.passed = result.failing_pixels == 0U && result.error_message.empty();
    return result;
}

} // namespace

auto load_png(const std::filesystem::path& path) -> goggles::Result<Image> {
    stbi_set_unpremultiply_on_load(0);

    int width = 0;
    int height = 0;
    int channels = 0;
    constexpr int DESIRED_CHANNELS = 4;

    auto* raw_pixels =
        stbi_load(path.string().c_str(), &width, &height, &channels, DESIRED_CHANNELS);
    if (raw_pixels == nullptr) {
        const char* reason = stbi_failure_reason();
        const std::string failure_reason = reason == nullptr ? "unknown failure" : reason;
        return make_error<Image>(ErrorCode::file_read_failed,
                                 "Failed to load PNG: " + path.string() + ": " + failure_reason);
    }

    const auto deleter = [](stbi_uc* pixels) { stbi_image_free(pixels); };
    std::unique_ptr<stbi_uc, decltype(deleter)> pixels(raw_pixels, deleter);

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    const std::size_t byte_count = pixel_count * static_cast<std::size_t>(DESIRED_CHANNELS);

    Image image;
    image.width = width;
    image.height = height;
    image.channels = DESIRED_CHANNELS;
    image.data.assign(pixels.get(), pixels.get() + byte_count);
    return image;
}

auto compare_images(const Image& actual, const Image& reference, const double tolerance,
                    const std::filesystem::path& diff_out, const bool compute_ssim)
    -> CompareResult {
    return compare_images_impl(actual, reference, tolerance, nullptr, diff_out, compute_ssim);
}

auto compare_images(const Image& actual, const Image& reference, const double tolerance,
                    const Rect& roi, const std::filesystem::path& diff_out, const bool compute_ssim)
    -> CompareResult {
    return compare_images_impl(actual, reference, tolerance, &roi, diff_out, compute_ssim);
}

auto generate_diff_heatmap(const Image& actual, const Image& reference,
                           const std::filesystem::path& output) -> Result<void> {
    if (actual.width != reference.width || actual.height != reference.height) {
        return make_error<void>(ErrorCode::invalid_data,
                                build_size_mismatch_message(actual, reference));
    }

    std::vector<std::uint8_t> heatmap(
        static_cast<std::size_t>(actual.width) * static_cast<std::size_t>(actual.height) * 4U, 0U);
    for (int y = 0; y < actual.height; ++y) {
        for (int x = 0; x < actual.width; ++x) {
            const auto offset = pixel_offset(actual, x, y);
            double magnitude = 0.0;
            for (std::size_t channel = 0; channel < 4U; ++channel) {
                const int delta = std::abs(static_cast<int>(actual.data[offset + channel]) -
                                           static_cast<int>(reference.data[offset + channel]));
                magnitude += static_cast<double>(delta) / 255.0;
            }
            paint_heatmap_pixel(heatmap, offset, magnitude / 4.0);
        }
    }

    const int stride = actual.width * 4;
    if (stbi_write_png(output.string().c_str(), actual.width, actual.height, 4, heatmap.data(),
                       stride) == 0) {
        return make_error<void>(ErrorCode::file_write_failed, build_write_failure_message(output));
    }

    return {};
}

} // namespace goggles::test
