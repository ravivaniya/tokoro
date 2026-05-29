#include "config.hpp"
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace tokoro {

namespace fs = std::filesystem;

std::optional<Config> Config::parse(int argc, char** argv) {
    Config cfg;
    std::string docroot_str = "./www";

    std::vector<std::string_view> args(argv + 1, argv + argc);
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--help" || args[i] == "-h") {
            std::cout << "Usage: tokoro [options]\n"
                      << "Options:\n"
                      << "  --port <port>                 Port to listen on (default: 8080)\n"
                      << "  --bind <address>              Address to bind to (default: 0.0.0.0)\n"
                      << "  --docroot <path>              Document root directory (default: ./www)\n"
                      << "  --workers <count>             Number of worker threads (default: hardware concurrency)\n"
                      << "  --max-body <bytes>            Maximum request body size (default: 10485760)\n"
                      << "  --header-timeout <ms>         Timeout for reading headers (default: 5000)\n"
                      << "  --body-timeout <ms>           Timeout for reading body (default: 30000)\n"
                      << "  --keepalive-timeout <ms>      Keep-alive idle timeout (default: 60000)\n"
                      << "  --max-connections <count>     Maximum concurrent connections (default: 10000)\n"
                      << "  --log-level <level>           Logging level (default: info)\n"
                      << "  --log-file <path>             Log file path (default: empty for stdout)\n"
                      << "  --version                     Print version and exit\n"
                      << "  --help, -h                    Print this help and exit\n";
            return std::nullopt;
        } else if (args[i] == "--version") {
            std::cout << "tokoro/1.0\n";
            return std::nullopt;
        } else if (args[i] == "--port" && i + 1 < args.size()) {
            cfg.port = std::stoi(std::string(args[++i]));
        } else if (args[i] == "--bind" && i + 1 < args.size()) {
            cfg.bind_address = args[++i];
        } else if (args[i] == "--docroot" && i + 1 < args.size()) {
            docroot_str = args[++i];
        } else if (args[i] == "--workers" && i + 1 < args.size()) {
            cfg.workers = std::stoull(std::string(args[++i]));
        } else if (args[i] == "--max-body" && i + 1 < args.size()) {
            cfg.max_body_bytes = std::stoull(std::string(args[++i]));
        } else if (args[i] == "--header-timeout" && i + 1 < args.size()) {
            cfg.header_timeout_ms = std::stoull(std::string(args[++i]));
        } else if (args[i] == "--body-timeout" && i + 1 < args.size()) {
            cfg.body_timeout_ms = std::stoull(std::string(args[++i]));
        } else if (args[i] == "--keepalive-timeout" && i + 1 < args.size()) {
            cfg.keepalive_timeout_ms = std::stoull(std::string(args[++i]));
        } else if (args[i] == "--max-connections" && i + 1 < args.size()) {
            cfg.max_connections = std::stoull(std::string(args[++i]));
        } else if (args[i] == "--log-level" && i + 1 < args.size()) {
            cfg.log_level = args[++i];
        } else if (args[i] == "--log-file" && i + 1 < args.size()) {
            cfg.log_file = args[++i];
        } else {
            throw std::invalid_argument("Unknown or incomplete argument: " + std::string(args[i]));
        }
    }

    // Resolve docroot relative to CWD at startup
    cfg.docroot = fs::weakly_canonical(fs::absolute(docroot_str));
    if (!fs::exists(cfg.docroot)) {
        throw std::invalid_argument("Document root does not exist: " + cfg.docroot.string());
    }
    if (!fs::is_directory(cfg.docroot)) {
        throw std::invalid_argument("Document root is not a directory: " + cfg.docroot.string());
    }

    return cfg;
}

} // namespace tokoro
