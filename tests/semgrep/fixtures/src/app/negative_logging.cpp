#include <string>

namespace goggles::app {

class LoggingProbe {
public:
    void report_startup(const std::string& profile_name) {
        const std::string message = "starting profile: " + profile_name;
        (void)message;
    }
};

} // namespace goggles::app
