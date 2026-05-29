#include <iostream>
#include <stdexcept>
#include <csignal>
#include <atomic>
#include "server.hpp"
#include "config.hpp"

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        g_running = false;
    }
}

int main(int argc, char** argv) {
    try {
        auto config_opt = tokoro::Config::parse(argc, argv);
        if (!config_opt) {
            return 0;
        }

        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        std::cout << "Starting tokoro HTTP server...\n";
        tokoro::Server server(*config_opt);
        server.run(g_running);
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
