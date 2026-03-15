#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace {

auto read_text_file(const std::filesystem::path& path) -> std::optional<std::string> {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

} // namespace

TEST_CASE("Windowed shutdown stops ticking after quit events", "[app][shutdown_contract]") {
    const auto main_path =
        std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
        "src/app/main.cpp";
    auto main_text = read_text_file(main_path);
    REQUIRE(main_text.has_value());

    const auto run_windowed_pos = main_text->find("static auto run_windowed_mode(");
    const auto process_event_pos = main_text->find("app.process_event();", run_windowed_pos);
    const auto running_guard_pos = main_text->find("if (!app.is_running()) {", process_event_pos);
    const auto break_pos = main_text->find("break;", running_guard_pos);
    const auto tick_frame_pos = main_text->find("app.tick_frame();", process_event_pos);

    REQUIRE(run_windowed_pos != std::string::npos);
    REQUIRE(process_event_pos != std::string::npos);
    REQUIRE(running_guard_pos != std::string::npos);
    REQUIRE(break_pos != std::string::npos);
    REQUIRE(tick_frame_pos != std::string::npos);

    REQUIRE(process_event_pos < running_guard_pos);
    REQUIRE(running_guard_pos < break_pos);
    REQUIRE(break_pos < tick_frame_pos);
}
