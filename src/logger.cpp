#include "logger.hpp"
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace tokoro {

namespace {
    std::mutex log_mutex;

    std::string get_timestamp() {
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        gmtime_r(&t, &tm_buf);
        
        std::ostringstream oss;
        oss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%S")
            << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
        return oss.str();
    }
}

Logger& Logger::instance() {
    static Logger inst;
    return inst;
}

Logger::Logger() : current_level_(LogLevel::INFO) {}

Logger::~Logger() {}

void Logger::set_level(LogLevel level) {
    current_level_ = level;
}

void Logger::set_level(std::string_view level_str) {
    if (level_str == "debug" || level_str == "DEBUG") set_level(LogLevel::DEBUG);
    else if (level_str == "info" || level_str == "INFO") set_level(LogLevel::INFO);
    else if (level_str == "warn" || level_str == "WARN") set_level(LogLevel::WARN);
    else if (level_str == "error" || level_str == "ERROR") set_level(LogLevel::ERROR);
}

void Logger::debug(std::string_view msg) {
    if (current_level_ > LogLevel::DEBUG) return;
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << get_timestamp() << " [DEBUG] " << msg << "\n";
}

void Logger::info(std::string_view msg) {
    if (current_level_ > LogLevel::INFO) return;
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << get_timestamp() << " [INFO] " << msg << "\n";
}

void Logger::warn(std::string_view msg) {
    if (current_level_ > LogLevel::WARN) return;
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cerr << get_timestamp() << " [WARN] " << msg << "\n";
}

void Logger::error(std::string_view msg) {
    if (current_level_ > LogLevel::ERROR) return;
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cerr << get_timestamp() << " [ERROR] " << msg << "\n";
}

void Logger::access(std::string_view method,
                    std::string_view uri,
                    int status,
                    size_t bytes,
                    size_t duration_ms,
                    std::string_view client_ip,
                    std::string_view ua,
                    std::string_view request_id) {
    if (current_level_ > LogLevel::INFO) return;
    
    std::string ip = client_ip.empty() ? "-" : std::string(client_ip);
    std::string user_agent = ua.empty() ? "-" : std::string(ua);
    std::string rid = request_id.empty() ? "-" : std::string(request_id);

    std::ostringstream oss;
    oss << get_timestamp() << " [ACCESS] " 
        << method << " " 
        << uri << " " 
        << status << " " 
        << bytes << " " 
        << duration_ms << "ms " 
        << ip << " " 
        << "\"" << user_agent << "\" " 
        << rid;
        
    std::lock_guard<std::mutex> lock(log_mutex);
    std::cout << oss.str() << "\n";
}

} // namespace tokoro
