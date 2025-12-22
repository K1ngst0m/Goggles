## 1. Implementation
- [x] 1.1 Add `CLI11` dependency via `CPM.cmake`
- [x] 1.2 Implement `goggles::app::parse_cli` in `cli.hpp`
- [x] 1.3 Encapsulate `CLI::ParseError` to ensure no exceptions propagate to core logic
- [x] 1.4 Integrate `CliResult` (nonstd::expected) into `main.cpp`
- [x] 1.5 Verify help and version flags exit with code 0
- [x] 1.6 Verify invalid arguments trigger `EXIT_FAILURE`
- [x] 1.7 Clean up all narration comments per Policy C.7
- [x] 1.8 Define project version in `CMakeLists.txt` and generate `version.hpp` via `configure_file`