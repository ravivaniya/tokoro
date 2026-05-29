#ifndef TOKORO_CONFIG_HPP
#define TOKORO_CONFIG_HPP

#include <string>
#include <cstdint>
#include <thread>
#include <filesystem>
#include <optional>

namespace tokoro {

struct Config {
    uint16_t port = 8080;
    std::string bind_address = "0.0.0.0";
    std::filesystem::path docroot;
    size_t workers = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4;
    size_t max_body_bytes = 10 * 1024 * 1024; // 10 MB default
    size_t header_timeout_ms = 5000;
    size_t body_timeout_ms = 30000;
    size_t keepalive_timeout_ms = 60000;
    size_t max_connections = 10000;
    std::string log_level = "info";
    std::string log_file = "";

    // Parse command line arguments. Returns nullopt if --help or --version is passed.
    // Throws std::invalid_argument if invalid args.
    static std::optional<Config> parse(int argc, char** argv);
};

} // namespace tokoro

#endif // TOKORO_CONFIG_HPP
