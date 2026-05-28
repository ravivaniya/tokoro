#include <iostream>
#include <stdexcept>
#include "server.hpp"

int main() {
    std::cout << "Starting tokoro HTTP server...\n";

    uint16_t port = 8080;
    try {
        tokoro::Server server(port);
        server.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
