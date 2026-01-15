#pragma once

#include <unistd.h>
#include <utility>

namespace goggles::util {

/// @brief RAII wrapper for an owned POSIX file descriptor.
class UniqueFd {
public:
    UniqueFd() = default;
    explicit UniqueFd(int fd) : m_fd(fd) {}

    ~UniqueFd() {
        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }

    UniqueFd(const UniqueFd&) = delete;
    UniqueFd& operator=(const UniqueFd&) = delete;

    UniqueFd(UniqueFd&& other) noexcept : m_fd(std::exchange(other.m_fd, -1)) {}

    UniqueFd& operator=(UniqueFd&& other) noexcept {
        if (this != &other) {
            if (m_fd >= 0) {
                ::close(m_fd);
            }
            m_fd = std::exchange(other.m_fd, -1);
        }
        return *this;
    }

    /// @brief Duplicates `fd` and returns an owning wrapper.
    /// @param fd Descriptor to duplicate.
    /// @return Empty wrapper if `fd < 0` or `dup()` fails.
    [[nodiscard]] static UniqueFd dup_from(int fd) {
        if (fd < 0) {
            return UniqueFd{};
        }
        return UniqueFd{::dup(fd)};
    }

    /// @brief Duplicates this descriptor.
    /// @return Empty wrapper if invalid or `dup()` fails.
    [[nodiscard]] UniqueFd dup() const {
        if (m_fd < 0) {
            return UniqueFd{};
        }
        return UniqueFd{::dup(m_fd)};
    }

    /// @brief Returns the raw descriptor, or `-1` if invalid.
    [[nodiscard]] int get() const { return m_fd; }
    /// @brief Releases ownership and returns the raw descriptor.
    int release() { return std::exchange(m_fd, -1); }
    /// @brief Returns true if the descriptor is valid.
    [[nodiscard]] bool valid() const { return m_fd >= 0; }
    explicit operator bool() const { return valid(); }

private:
    int m_fd = -1;
};

} // namespace goggles::util
