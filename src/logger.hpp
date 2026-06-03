#ifndef TOKORO_LOGGER_HPP
#define TOKORO_LOGGER_HPP

#include <string>
#include <string_view>

namespace tokoro {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& instance();

    // Delete copy and move
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    void set_level(LogLevel level);
    void set_level(std::string_view level_str);
    
    // Core logging methods
    void debug(std::string_view msg);
    void info(std::string_view msg);
    void warn(std::string_view msg);
    void error(std::string_view msg);

    // Access log format: ts method uri status bytes duration_ms client_ip ua request_id
    void access(std::string_view method,
                std::string_view uri,
                int status,
                size_t bytes,
                size_t duration_ms,
                std::string_view client_ip,
                std::string_view ua,
                std::string_view request_id);

private:
    Logger();
    ~Logger();

    LogLevel current_level_;
};

} // namespace tokoro

#endif // TOKORO_LOGGER_HPP
