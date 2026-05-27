#include "server.hpp"
#include <iostream>
#include <vector>
#include <sys/socket.h>

namespace tokoro {

Server::Server(uint16_t port) : port_(port) {
    if (!server_socket_.is_valid()) {
        std::cerr << "Failed to create server socket.\n";
        return;
    }

    if (!server_socket_.set_reuse_address(true)) {
        std::cerr << "Failed to set SO_REUSEADDR.\n";
        return;
    }

    if (!server_socket_.bind(port_)) {
        std::cerr << "Failed to bind server socket to port " << port_ << ".\n";
        return;
    }

    if (!server_socket_.listen()) {
        std::cerr << "Failed to listen on server socket.\n";
        return;
    }

    std::cout << "Server initialized and listening on port " << port_ << "...\n";
}

void Server::run() {
    if (!server_socket_.is_valid()) {
        std::cerr << "Server socket not valid, cannot run.\n";
        return;
    }

    while (true) {
        auto client_opt = server_socket_.accept();
        if (!client_opt) {
            std::cerr << "Failed to accept client connection.\n";
            continue;
        }

        std::cout << "Accepted new connection.\n";
        handle_client(*client_opt);
    }
}

void Server::handle_client(Socket& client_socket) {
    std::vector<char> buffer(4096);

    while (true) {
        ssize_t bytes_received = ::recv(client_socket.get(), buffer.data(), buffer.size(), 0);

        if (bytes_received < 0) {
            std::cerr << "Error receiving data.\n";
            break;
        } else if (bytes_received == 0) {
            std::cout << "Client disconnected.\n";
            break;
        }

        std::cout << "Received " << bytes_received << " bytes.\n";
        
        // Echo back to the client
        ssize_t bytes_sent = ::send(client_socket.get(), buffer.data(), static_cast<size_t>(bytes_received), 0);
        if (bytes_sent < 0) {
            std::cerr << "Error sending data.\n";
            break;
        }
    }
}

} // namespace tokoro
