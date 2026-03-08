#include <iostream>
#include <string>

namespace goggles::app {

class LoggingProbe {
public:
    void report_startup(const std::string& profile_name) {
        std::cout << "starting profile: " << profile_name << "\n";
    }
};

} // namespace goggles::app
