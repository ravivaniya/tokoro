#include "socket.hpp"
#include <unistd.h>
#include <iostream>
#include <cstring>
#include <cerrno>
#include <sys/types.h>

namespace tokoro {

Socket::Socket() {
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ == -1) {
        std::cerr << "Failed to create socket: " << std::strerror(errno) << "\n";
    }
}

Socket::Socket(int fd) : fd_(fd) {}

Socket::~Socket() {
    if (is_valid()) {
        ::close(fd_);
        fd_ = -1;
    }
}

Socket::Socket(Socket&& other) noexcept : fd_(other.fd_) {
    other.fd_ = -1;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        if (is_valid()) {
            ::close(fd_);
        }
        fd_ = other.fd_;
        other.fd_ = -1;
    }
    return *this;
}

bool Socket::set_reuse_address(bool reuse) {
    if (!is_valid()) return false;
    int opt = reuse ? 1 : 0;
    if (::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to setsockopt SO_REUSEADDR: " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

bool Socket::bind(uint16_t port) {
    if (!is_valid()) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "Failed to bind to port " << port << ": " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

bool Socket::listen(int backlog) {
    if (!is_valid()) return false;

    if (::listen(fd_, backlog) < 0) {
        std::cerr << "Failed to listen: " << std::strerror(errno) << "\n";
        return false;
    }
    return true;
}

std::optional<Socket> Socket::accept() {
    if (!is_valid()) return std::nullopt;

    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = ::accept(fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len);

    if (client_fd < 0) {
        std::cerr << "Failed to accept connection: " << std::strerror(errno) << "\n";
        return std::nullopt;
    }

    return Socket(client_fd);
}

} // namespace tokoro
