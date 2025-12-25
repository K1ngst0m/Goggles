#include "util/serializer.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>

using namespace goggles::util;

TEST_CASE("Binary Serialization", "[util][serializer]") {
    SECTION("Basic POD types") {
        BinaryWriter w;
        uint32_t u32 = 0x12345678;
        int32_t i32 = -12345678;
        float f32 = 3.14159f;

        w.write_pod(u32);
        w.write_pod(i32);
        w.write_pod(f32);

        BinaryReader r(w.buffer.data(), w.buffer.size());
        uint32_t ru32;
        int32_t ri32;
        float rf32;

        REQUIRE(r.read_pod(ru32));
        REQUIRE(r.read_pod(ri32));
        REQUIRE(r.read_pod(rf32));

        REQUIRE(ru32 == u32);
        REQUIRE(ri32 == i32);
        REQUIRE(rf32 == f32);
    }

    SECTION("Strings") {
        BinaryWriter w;
        std::string s1 = "Hello World";
        std::string s2 = "Goggles Shader Cache";
        std::string s3 = "";

        REQUIRE(w.write_str(s1));
        REQUIRE(w.write_str(s2));
        REQUIRE(w.write_str(s3));

        BinaryReader r(w.buffer.data(), w.buffer.size());
        std::string rs1, rs2, rs3;

        REQUIRE(r.read_str(rs1));
        REQUIRE(r.read_str(rs2));
        REQUIRE(r.read_str(rs3));

        REQUIRE(rs1 == s1);
        REQUIRE(rs2 == s2);
        REQUIRE(rs3 == s3);
    }

    SECTION("Vectors") {
        BinaryWriter w;
        std::vector<uint32_t> vec = {1, 2, 3, 4, 5};

        auto serialize_item = [](BinaryWriter& writer,
                                 const uint32_t& item) -> goggles::Result<void> {
            writer.write_pod(item);
            return {};
        };

        REQUIRE(w.write_vec(vec, serialize_item));

        BinaryReader r(w.buffer.data(), w.buffer.size());
        std::vector<uint32_t> rvec;

        auto deserialize_item = [](BinaryReader& reader, uint32_t& item) {
            return reader.read_pod(item);
        };

        REQUIRE(r.read_vec(rvec, deserialize_item));
        REQUIRE(rvec == vec);
    }

    SECTION("Complex Nested Data") {
        struct Member {
            uint32_t id;
            std::string name;
            bool operator==(const Member&) const = default;
        };

        std::vector<Member> members = {{1, "Alice"}, {2, "Bob"}, {3, "Charlie"}};

        BinaryWriter w;
        REQUIRE(w.write_vec(members,
                            [](BinaryWriter& writer, const Member& m) -> goggles::Result<void> {
                                writer.write_pod(m.id);
                                return writer.write_str(m.name);
                            }));

        BinaryReader r(w.buffer.data(), w.buffer.size());
        std::vector<Member> rmembers;
        REQUIRE(r.read_vec(rmembers, [](BinaryReader& reader, Member& m) {
            return reader.read_pod(m.id) && reader.read_str(m.name);
        }));

        REQUIRE(rmembers == members);
    }

    SECTION("Reader Bounds Checking") {
        BinaryWriter w;
        w.write_pod(42u);

        BinaryReader r(w.buffer.data(), w.buffer.size());
        uint32_t val;
        REQUIRE(r.read_pod(val));
        REQUIRE(!r.read_pod(val)); // EOF
        REQUIRE(r.remaining == 0);
    }

    SECTION("File I/O") {
        std::string content = "This is a test binary file.";
        auto path = std::filesystem::temp_directory_path() / "goggles_test.bin";

        struct Cleanup {
            std::filesystem::path p;
            ~Cleanup() { std::filesystem::remove(p); }
        } cleanup{path};

        {
            std::ofstream ofs(path, std::ios::binary);
            ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
        }

        auto result = read_file_binary(path);
        REQUIRE(result.has_value());
        REQUIRE(result->size() == content.size());
        REQUIRE(std::string(result->data(), result->size()) == content);
    }

    SECTION("File I/O Error Paths") {
        auto path = std::filesystem::temp_directory_path() / "goggles_non_existent.bin";
        std::filesystem::remove(path);

        auto result = read_file_binary(path);
        REQUIRE(!result.has_value());
        REQUIRE(result.error().code == goggles::ErrorCode::file_not_found);
    }
}
