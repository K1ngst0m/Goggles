#include "runtime_capture.hpp"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct Arguments {
    std::filesystem::path preset_path;
    std::string preset_name;
    std::filesystem::path output_dir;
    std::vector<uint32_t> frame_indices;
    std::vector<uint32_t> pass_ordinals;
};

auto split_csv(std::string_view csv_values) -> std::vector<std::string> {
    std::vector<std::string> parts;
    std::string current;
    std::stringstream stream{std::string(csv_values)};
    while (std::getline(stream, current, ',')) {
        if (!current.empty()) {
            parts.push_back(current);
        }
    }
    return parts;
}

auto parse_indices(std::string_view csv_values, const char* value_label)
    -> std::optional<std::vector<uint32_t>> {
    std::vector<uint32_t> indices;
    for (const auto& part : split_csv(csv_values)) {
        try {
            const auto parsed = static_cast<uint32_t>(std::stoul(part));
            indices.push_back(parsed);
        } catch (const std::exception&) {
            std::cerr << "Invalid " << value_label << " value: " << part << '\n';
            return std::nullopt;
        }
    }
    return indices;
}

void print_usage() {
    std::cerr << "Usage: goggles_visual_capture"
              << " --preset-path <path> --preset-name <name> --output-dir <dir>"
              << " --frames <csv> [--passes <csv>]\n";
}

auto parse_args(int argc, char** argv) -> std::optional<Arguments> {
    Arguments args;

    for (int index = 1; index < argc; ++index) {
        const std::string_view option(argv[index]);
        if (option == "--preset-path" && index + 1 < argc) {
            args.preset_path = argv[++index];
        } else if (option == "--preset-name" && index + 1 < argc) {
            args.preset_name = argv[++index];
        } else if (option == "--output-dir" && index + 1 < argc) {
            args.output_dir = argv[++index];
        } else if (option == "--frames" && index + 1 < argc) {
            const auto parsed = parse_indices(argv[++index], "frame");
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            args.frame_indices = *parsed;
        } else if (option == "--passes" && index + 1 < argc) {
            const auto parsed = parse_indices(argv[++index], "pass");
            if (!parsed.has_value()) {
                return std::nullopt;
            }
            args.pass_ordinals = *parsed;
        } else {
            std::cerr << "Unknown or incomplete option: " << option << '\n';
            return std::nullopt;
        }
    }

    if (args.preset_path.empty() || args.preset_name.empty() || args.output_dir.empty() ||
        args.frame_indices.empty()) {
        print_usage();
        return std::nullopt;
    }

    std::sort(args.frame_indices.begin(), args.frame_indices.end());
    args.frame_indices.erase(std::unique(args.frame_indices.begin(), args.frame_indices.end()),
                             args.frame_indices.end());
    std::sort(args.pass_ordinals.begin(), args.pass_ordinals.end());
    args.pass_ordinals.erase(std::unique(args.pass_ordinals.begin(), args.pass_ordinals.end()),
                             args.pass_ordinals.end());
    return args;
}

auto copy_capture(const std::filesystem::path& source, const std::filesystem::path& destination)
    -> bool {
    std::error_code ec;
    std::filesystem::copy_file(source, destination,
                               std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        std::cerr << "Failed to copy " << source << " to " << destination << ": " << ec.message()
                  << '\n';
        return false;
    }
    return true;
}

} // namespace

auto main(int argc, char** argv) -> int {
    const auto parsed = parse_args(argc, argv);
    if (!parsed.has_value()) {
        return 2;
    }

    std::error_code ec;
    std::filesystem::create_directories(parsed->output_dir, ec);
    if (ec) {
        std::cerr << "Failed to create output directory " << parsed->output_dir << ": "
                  << ec.message() << '\n';
        return 1;
    }

    goggles::test::RuntimeCapturePlan plan{};
    plan.preset_path = parsed->preset_path;
    plan.preset_name = parsed->preset_name;
    plan.frame_indices = parsed->frame_indices;
    plan.intermediate_pass_ordinals = parsed->pass_ordinals;

    auto capture = goggles::test::capture_runtime_outputs(plan);
    if (!capture) {
        std::cerr << capture.error().message << '\n';
        return 1;
    }

    for (const auto frame_index : parsed->frame_indices) {
        const auto final_iter = capture->final_frames.find(frame_index);
        if (final_iter == capture->final_frames.end()) {
            std::cerr << "Missing captured final frame for index " << frame_index << '\n';
            return 1;
        }

        const auto final_destination = parsed->output_dir / (parsed->preset_name + "_frame" +
                                                             std::to_string(frame_index) + ".png");
        if (!copy_capture(final_iter->second, final_destination)) {
            return 1;
        }
    }

    for (const auto frame_index : parsed->frame_indices) {
        for (const auto pass_ordinal : parsed->pass_ordinals) {
            const auto key = goggles::test::pass_frame_key(pass_ordinal, frame_index);
            const auto pass_iter = capture->intermediate_frames.find(key);
            if (pass_iter == capture->intermediate_frames.end()) {
                std::cerr << "Missing captured pass output for pass " << pass_ordinal << " frame "
                          << frame_index << '\n';
                return 1;
            }

            const auto frame_destination =
                parsed->output_dir / (parsed->preset_name + "_pass" + std::to_string(pass_ordinal) +
                                      "_frame" + std::to_string(frame_index) + ".png");
            if (!copy_capture(pass_iter->second, frame_destination)) {
                return 1;
            }

            if (frame_index == 0U) {
                const auto canonical_destination =
                    parsed->output_dir /
                    (parsed->preset_name + "_pass" + std::to_string(pass_ordinal) + ".png");
                if (!copy_capture(pass_iter->second, canonical_destination)) {
                    return 1;
                }
            }
        }
    }

    return 0;
}
