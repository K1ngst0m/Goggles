#include "capture/capture_receiver.hpp"

#include <catch2/catch_test_macros.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

using goggles::CaptureReceiver;

TEST_CASE("CaptureReceiver factory creation", "[capture][factory]") {
    SECTION("Successful creation") {
        auto receiver = CaptureReceiver::create();
        REQUIRE(receiver.has_value());

        receiver.value()->shutdown();
    }

    SECTION("Multiple receivers fail (socket already in use)") {
        auto receiver1 = CaptureReceiver::create();
        REQUIRE(receiver1.has_value());

        auto receiver2 = CaptureReceiver::create();
        REQUIRE(!receiver2.has_value());
        REQUIRE(receiver2.error().code == goggles::ErrorCode::capture_init_failed);

        receiver1.value()->shutdown();

        auto receiver3 = CaptureReceiver::create();
        REQUIRE(receiver3.has_value());
    }
}

TEST_CASE("CaptureReceiver error messages", "[capture][error][messages]") {
    auto receiver1 = CaptureReceiver::create();
    REQUIRE(receiver1.has_value());

    SECTION("Address in use error is clear") {
        auto receiver2 = CaptureReceiver::create();
        REQUIRE(!receiver2.has_value());

        const auto& msg = receiver2.error().message;
        REQUIRE((msg.find("already in use") != std::string::npos ||
                 msg.find("another instance") != std::string::npos ||
                 msg.find("socket") != std::string::npos));
        REQUIRE(msg.length() > 15);
    }

    SECTION("Error code is appropriate") {
        auto receiver2 = CaptureReceiver::create();
        REQUIRE(!receiver2.has_value());
        REQUIRE(receiver2.error().code == goggles::ErrorCode::capture_init_failed);
    }
}

TEST_CASE("CaptureReceiver cleanup behavior", "[capture][cleanup]") {
    SECTION("Shutdown is safe") {
        auto receiver = CaptureReceiver::create();
        REQUIRE(receiver.has_value());

        receiver.value()->shutdown();
        receiver.value()->shutdown();
    }

    SECTION("Destructor releases socket") {
        {
            auto receiver = CaptureReceiver::create();
            REQUIRE(receiver.has_value());
        }

        auto receiver2 = CaptureReceiver::create();
        REQUIRE(receiver2.has_value());
    }

    SECTION("No operations after shutdown") {
        auto receiver = CaptureReceiver::create();
        REQUIRE(receiver.has_value());

        receiver.value()->shutdown();

        auto frame = receiver.value()->poll_frame();
        REQUIRE(!frame);
    }
}

TEST_CASE("CaptureReceiver factory failure cleanup", "[capture][factory][cleanup]") {
    SECTION("Failed creation doesn't leak resources") {
        auto receiver1 = CaptureReceiver::create();
        REQUIRE(receiver1.has_value());

        auto receiver2 = CaptureReceiver::create();
        REQUIRE(!receiver2.has_value());
    }
}
