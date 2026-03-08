namespace goggles::util {

class JobSystem {
public:
    void enqueue() const {}
};

} // namespace goggles::util

namespace goggles::render {

void schedule_upload(const goggles::util::JobSystem& job_system) {
    job_system.enqueue();
}

} // namespace goggles::render
