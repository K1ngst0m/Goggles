#include <cstdio>

namespace goggles::app {

class LoggingProbe {
public:
    void report_startup() { std::printf("starting profile\n"); }
};

} // namespace goggles::app
