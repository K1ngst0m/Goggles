#include "image_compare.hpp"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

auto print_usage(const char* program_name) -> void {
    std::cout << "Usage: " << program_name
              << " <actual> <reference> [--tolerance T] [--diff out.png]\n";
}

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc < 3) {
        print_usage(argv[0]);
        return 2;
    }

    std::filesystem::path actual_path = argv[1];
    std::filesystem::path reference_path = argv[2];
    double tolerance = 0.0;
    std::filesystem::path diff_out;

    int index = 3;
    while (index < argc) {
        const std::string argument = argv[index];
        if (argument == "--tolerance") {
            if (index + 1 >= argc) {
                std::cerr << "--tolerance requires a value\n";
                return 2;
            }
            char* end = nullptr;
            const double parsed = std::strtod(argv[index + 1], &end);
            if (end == argv[index + 1] || *end != '\0') {
                std::cerr << "--tolerance value is not a valid number: " << argv[index + 1] << "\n";
                return 2;
            }
            if (!std::isfinite(parsed) || parsed < 0.0 || parsed > 1.0) {
                std::cerr << "--tolerance must be a finite value in [0.0, 1.0]\n";
                return 2;
            }
            tolerance = parsed;
            index += 2;
            continue;
        }
        if (argument == "--diff") {
            if (index + 1 >= argc) {
                std::cerr << "--diff requires a path\n";
                return 2;
            }
            diff_out = argv[index + 1];
            index += 2;
            continue;
        }

        std::cerr << "Unknown argument: " << argument << "\n";
        print_usage(argv[0]);
        return 2;
    }

    auto actual_result = goggles::test::load_png(actual_path);
    if (!actual_result) {
        std::cerr << actual_result.error().message << "\n";
        return 2;
    }

    auto reference_result = goggles::test::load_png(reference_path);
    if (!reference_result) {
        std::cerr << reference_result.error().message << "\n";
        return 2;
    }

    const goggles::test::CompareResult result = goggles::test::compare_images(
        actual_result.value(), reference_result.value(), tolerance, diff_out);

    if (result.passed) {
        return 0;
    }

    if (!result.error_message.empty()) {
        std::cout << result.error_message << "\n";
    }
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "failing_pixels: " << result.failing_pixels << "\n";
    std::cout << "max_channel_diff: " << result.max_channel_diff << "\n";
    std::cout << "failing_percentage: " << result.failing_percentage << "\n";
    return 1;
}
