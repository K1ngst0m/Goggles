#pragma once

#include "util/error.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace goggles::util {

class BinaryWriter {
public:
    std::vector<char> buffer;

    void write(const void* data, size_t size) {
        const auto* bytes = static_cast<const char*>(data);
        buffer.insert(buffer.end(), bytes, bytes + size);
    }

    template <typename T>
    void write_pod(const T& val) {
        static_assert(std::is_standard_layout_v<T>, "Type must be standard layout");
        const auto* bytes = reinterpret_cast<const char*>(&val);
        buffer.insert(buffer.end(), bytes, bytes + sizeof(T));
    }

    auto write_str(const std::string& str) -> Result<void> {
        if (str.size() > std::numeric_limits<uint32_t>::max()) {
            return make_error<void>(ErrorCode::invalid_data, "String size exceeds uint32_t limit");
        }
        write_pod(static_cast<uint32_t>(str.size()));
        buffer.insert(buffer.end(), str.begin(), str.end());
        return {};
    }

    template <typename T, typename Func>
    auto write_vec(const std::vector<T>& vec, Func func) -> Result<void> {
        if (vec.size() > std::numeric_limits<uint32_t>::max()) {
            return make_error<void>(ErrorCode::invalid_data, "Vector size exceeds uint32_t limit");
        }
        write_pod(static_cast<uint32_t>(vec.size()));
        for (const auto& item : vec) {
            GOGGLES_TRY(func(*this, item));
        }
        return {};
    }
};

class BinaryReader {
public:
    const char* ptr;
    size_t remaining;

    BinaryReader(const char* data, size_t size) : ptr(data), remaining(size) {}

    bool read(void* dest, size_t size) {
        if (remaining < size) {
            return false;
        }
        std::copy(ptr, ptr + size, static_cast<char*>(dest));
        ptr += size;
        remaining -= size;
        return true;
    }

    template <typename T>
    bool read_pod(T& val) {
        static_assert(std::is_standard_layout_v<T>, "Type must be standard layout");
        if (remaining < sizeof(T)) {
            return false;
        }
        std::copy(ptr, ptr + sizeof(T), reinterpret_cast<char*>(&val));
        ptr += sizeof(T);
        remaining -= sizeof(T);
        return true;
    }

    bool read_str(std::string& str) {
        uint32_t len = 0;
        if (!read_pod(len)) {
            return false;
        }
        if (remaining < len) {
            return false;
        }
        str.assign(ptr, len);
        ptr += len;
        remaining -= len;
        return true;
    }

    template <typename T, typename Func>
    bool read_vec(std::vector<T>& vec, Func func) {
        uint32_t count = 0;
        if (!read_pod(count)) {
            return false;
        }
        vec.resize(count);
        for (auto& item : vec) {
            if (!func(*this, item)) {
                vec.clear();
                return false;
            }
        }
        return true;
    }
};

inline auto read_file_binary(const std::filesystem::path& path) -> Result<std::vector<char>> {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return make_error<std::vector<char>>(ErrorCode::file_not_found,
                                             "File not found: " + path.string());
    }

    const auto size = file.tellg();
    if (size <= 0) {
        return std::vector<char>{};
    }

    std::vector<char> buffer(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(buffer.data(), size)) {
        return make_error<std::vector<char>>(ErrorCode::file_read_failed, "Failed to read file");
    }

    return buffer;
}

} // namespace goggles::util