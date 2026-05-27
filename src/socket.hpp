#ifndef TOKORO_SOCKET_HPP
#define TOKORO_SOCKET_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstdint>
#include <optional>

namespace tokoro {

/**
 * @brief An RAII wrapper around a POSIX socket file descriptor.
 * 
 * Manages the lifecycle of a socket descriptor. It is non-copyable to 
 * prevent accidental double-close, but movable to allow returning from functions
 * and storing in containers.
 */
class Socket {
public:
    // Creates a new TCP socket (IPv4 by default)
    Socket();
    
    // Wraps an existing file descriptor (e.g., from accept())
    explicit Socket(int fd);

    // Destructor: closes the socket if it is valid
    ~Socket();

    // Delete copy constructor and copy assignment operator
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    // Move constructor and move assignment operator
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;

    // Core POSIX operations
    bool bind(uint16_t port);
    bool listen(int backlog = 128);
    std::optional<Socket> accept();
    
    // Returns the underlying file descriptor
    [[nodiscard]] int get() const noexcept { return fd_; }
    
    // Checks if the socket holds a valid file descriptor
    [[nodiscard]] bool is_valid() const noexcept { return fd_ != -1; }
    
    // Enables SO_REUSEADDR
    bool set_reuse_address(bool reuse = true);

private:
    int fd_{-1};
};

} // namespace tokoro

#endif // TOKORO_SOCKET_HPP
