## 1. CMake Configuration
- [x] 1.1 Add `add_compile_definitions(GOGGLES_VERSION_MAJOR=${PROJECT_VERSION_MAJOR})` after existing `GOGGLES_VERSION` definition
- [x] 1.2 Add `add_compile_definitions(GOGGLES_VERSION_MINOR=${PROJECT_VERSION_MINOR})`
- [x] 1.3 Add `add_compile_definitions(GOGGLES_VERSION_PATCH=${PROJECT_VERSION_PATCH})`
- [x] 1.4 Add comment noting `project(VERSION ...)` as single source of truth

## 2. Code Migration
- [x] 2.1 Update `src/render/backend/vulkan_backend.cpp` line 160: replace `VK_MAKE_VERSION(0, 1, 0)` with `VK_MAKE_VERSION(GOGGLES_VERSION_MAJOR, GOGGLES_VERSION_MINOR, GOGGLES_VERSION_PATCH)` for `applicationVersion`
- [x] 2.2 Update `src/render/backend/vulkan_backend.cpp` line 162: replace `VK_MAKE_VERSION(0, 1, 0)` with `VK_MAKE_VERSION(GOGGLES_VERSION_MAJOR, GOGGLES_VERSION_MINOR, GOGGLES_VERSION_PATCH)` for `engineVersion`
- [x] 2.3 Search for any other hardcoded version strings with `rg "0\.1\.0" src/`

## 3. Testing & Validation
- [x] 3.1 Clean build: `make clean && make dev`
- [x] 3.2 Verify build succeeds
- [x] 3.3 Run `./build/debug/bin/goggles --version` and verify correct version output
- [x] 3.4 Test version change: modify `CMakeLists.txt` VERSION to `0.2.0`, rebuild, verify propagation
- [x] 3.5 Restore original version
