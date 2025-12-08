#pragma once

#include <unistd.h>
#include <utility>

namespace goggles::util {

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

    [[nodiscard]] static UniqueFd dup_from(int fd) {
        if (fd < 0) {
            return UniqueFd{};
        }
        return UniqueFd{::dup(fd)};
    }

    [[nodiscard]] UniqueFd dup() const {
        if (m_fd < 0) {
            return UniqueFd{};
        }
        return UniqueFd{::dup(m_fd)};
    }

    [[nodiscard]] int get() const { return m_fd; }
    int release() { return std::exchange(m_fd, -1); }
    [[nodiscard]] bool valid() const { return m_fd >= 0; }
    explicit operator bool() const { return valid(); }

private:
    int m_fd = -1;
};

} // namespace goggles::util
