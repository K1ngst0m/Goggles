#include "capture/capture_protocol.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using namespace goggles::capture;

TEST_CASE("CaptureMessageType enum values", "[capture][protocol]") {
    REQUIRE(static_cast<uint32_t>(CaptureMessageType::client_hello) == 1);
    REQUIRE(static_cast<uint32_t>(CaptureMessageType::texture_data) == 2);
    REQUIRE(static_cast<uint32_t>(CaptureMessageType::control) == 3);
    REQUIRE(static_cast<uint32_t>(CaptureMessageType::semaphore_init) == 4);
    REQUIRE(static_cast<uint32_t>(CaptureMessageType::frame_metadata) == 5);
}

TEST_CASE("Capture struct sizes match wire format", "[capture][protocol]") {
    REQUIRE(sizeof(CaptureClientHello) == 72);
    REQUIRE(sizeof(CaptureTextureData) == 32);
    REQUIRE(sizeof(CaptureControl) == 16);
    REQUIRE(sizeof(CaptureSemaphoreInit) == 16);
    REQUIRE(sizeof(CaptureFrameMetadata) == 40);
}

TEST_CASE("CaptureClientHello default values", "[capture][protocol]") {
    CaptureClientHello hello{};
    REQUIRE(hello.type == CaptureMessageType::client_hello);
    REQUIRE(hello.version == 1);
    REQUIRE(hello.exe_name[0] == '\0');
}

TEST_CASE("CaptureTextureData default values", "[capture][protocol]") {
    CaptureTextureData tex{};
    REQUIRE(tex.type == CaptureMessageType::texture_data);
    REQUIRE(tex.width == 0);
    REQUIRE(tex.height == 0);
    REQUIRE(tex.format == VK_FORMAT_UNDEFINED);
    REQUIRE(tex.stride == 0);
    REQUIRE(tex.offset == 0);
    REQUIRE(tex.modifier == 0);
}

TEST_CASE("CaptureControl default values", "[capture][protocol]") {
    CaptureControl ctrl{};
    REQUIRE(ctrl.type == CaptureMessageType::control);
    REQUIRE(ctrl.flags == 0);
    REQUIRE(ctrl.requested_width == 0);
    REQUIRE(ctrl.requested_height == 0);
}

TEST_CASE("CaptureControl flag constants", "[capture][protocol]") {
    REQUIRE(CAPTURE_CONTROL_CAPTURING == 1u);
    REQUIRE(CAPTURE_CONTROL_RESOLUTION_REQUEST == 2u);
}

TEST_CASE("CaptureSemaphoreInit default values", "[capture][protocol]") {
    CaptureSemaphoreInit sem{};
    REQUIRE(sem.type == CaptureMessageType::semaphore_init);
    REQUIRE(sem.version == 1);
    REQUIRE(sem.initial_value == 0);
}

TEST_CASE("CaptureFrameMetadata default values", "[capture][protocol]") {
    CaptureFrameMetadata meta{};
    REQUIRE(meta.type == CaptureMessageType::frame_metadata);
    REQUIRE(meta.width == 0);
    REQUIRE(meta.height == 0);
    REQUIRE(meta.format == VK_FORMAT_UNDEFINED);
    REQUIRE(meta.stride == 0);
    REQUIRE(meta.offset == 0);
    REQUIRE(meta.modifier == 0);
    REQUIRE(meta.frame_number == 0);
}

TEST_CASE("Socket path is abstract namespace", "[capture][protocol]") {
    REQUIRE(CAPTURE_SOCKET_PATH[0] == '\0');
    REQUIRE(CAPTURE_SOCKET_PATH_LEN == sizeof(CAPTURE_SOCKET_PATH) - 1);
    REQUIRE(std::strcmp(&CAPTURE_SOCKET_PATH[1], "goggles/vkcapture") == 0);
}