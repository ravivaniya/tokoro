#include <iostream>
#include "server.hpp"

int main() {
    std::cout << "Starting tokoro HTTP server...\n";

    uint16_t port = 8080;
    tokoro::Server server(port);
    server.run();

    return 0;
}
