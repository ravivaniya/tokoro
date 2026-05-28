#include "server.hpp"
#include <iostream>
#include <vector>
#include <sys/socket.h>
#include <stdexcept>

namespace tokoro {

Server::Server(uint16_t port) 
    : port_(port), 
      thread_pool_(std::make_unique<ThreadPool>(std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4)) {
    if (!server_socket_.is_valid()) {
        throw std::runtime_error("Failed to create server socket.");
    }

    if (!server_socket_.set_reuse_address(true)) {
        throw std::runtime_error("Failed to set SO_REUSEADDR.");
    }

    if (!server_socket_.bind(port_)) {
        throw std::runtime_error("Failed to bind server socket to port " + std::to_string(port_));
    }

    if (!server_socket_.listen()) {
        throw std::runtime_error("Failed to listen on server socket.");
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
        
        auto shared_client = std::make_shared<Socket>(std::move(*client_opt));
        thread_pool_->enqueue([this, shared_client]() {
            this->handle_client(shared_client);
        });
    }
}

void Server::handle_client(std::shared_ptr<Socket> client_socket) {
    std::vector<char> buffer(4096);

    while (true) {
        ssize_t bytes_received = ::recv(client_socket->get(), buffer.data(), buffer.size(), 0);

        if (bytes_received < 0) {
            std::cerr << "Error receiving data.\n";
            break;
        } else if (bytes_received == 0) {
            std::cout << "Client disconnected.\n";
            break;
        }

        std::cout << "Received " << bytes_received << " bytes.\n";
        
        // Echo back to the client
        ssize_t bytes_sent = ::send(client_socket->get(), buffer.data(), static_cast<size_t>(bytes_received), 0);
        if (bytes_sent < 0) {
            std::cerr << "Error sending data.\n";
            break;
        }
    }
}

} // namespace tokoro
