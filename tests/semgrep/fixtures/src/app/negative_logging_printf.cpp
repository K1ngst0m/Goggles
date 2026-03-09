namespace goggles::app {

class LoggingProbe {
public:
    void report_startup() {
        constexpr const char* message = "starting profile";
        (void)message;
    }
};

} // namespace goggles::app
