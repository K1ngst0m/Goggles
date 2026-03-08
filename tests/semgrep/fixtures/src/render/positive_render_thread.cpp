#include <thread>

namespace goggles::render {

void schedule_upload() {
    std::jthread worker([] {});
}

} // namespace goggles::render
